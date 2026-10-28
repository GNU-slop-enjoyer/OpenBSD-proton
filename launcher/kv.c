/*
 * Copyright (c) 2026 The OpenBSD Proton project contributors
 * Distributed under the LGPL 2.1 or later.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "kv.h"

struct lex {
	const char	*p, *end;
};

/* Next token: 1 string (into *out), '{', '}', or 0 at end. */
static int
lex_next(struct lex *l, char **out)
{
	struct buf b = { 0 };

	for (;;) {
		while (l->p < l->end && (*l->p == ' ' || *l->p == '\t' ||
		    *l->p == '\r' || *l->p == '\n' || *l->p == '\0'))
			l->p++;
		if (l->p + 1 < l->end && l->p[0] == '/' && l->p[1] == '/') {
			while (l->p < l->end && *l->p != '\n')
				l->p++;
			continue;
		}
		break;
	}
	if (l->p >= l->end)
		return 0;
	if (*l->p == '{' || *l->p == '}')
		return *l->p++;
	if (*l->p == '"') {
		l->p++;
		while (l->p < l->end && *l->p != '"') {
			if (*l->p == '\\' && l->p + 1 < l->end) {
				l->p++;
				switch (*l->p) {
				case 'n': buf_putc(&b, '\n'); break;
				case 't': buf_putc(&b, '\t'); break;
				default: buf_putc(&b, *l->p); break;
				}
			} else
				buf_putc(&b, *l->p);
			l->p++;
		}
		if (l->p < l->end)
			l->p++;
	} else {
		while (l->p < l->end && !strchr(" \t\r\n{}\"", *l->p))
			buf_putc(&b, *l->p++);
	}
	*out = buf_str(&b);
	/* Conditionals such as [$WIN32] are ignored. */
	while (l->p < l->end && (*l->p == ' ' || *l->p == '\t'))
		l->p++;
	if (l->p < l->end && *l->p == '[') {
		while (l->p < l->end && *l->p != ']')
			l->p++;
		if (l->p < l->end)
			l->p++;
	}
	return 1;
}

static struct kv *
parse_list(struct lex *l, int nested)
{
	struct kv *head = NULL, **tail = &head;
	char *key, *val;
	int t;

	for (;;) {
		t = lex_next(l, &key);
		if (t == 0 || t == '}')
			return head;
		if (t == '{')
			continue;	/* malformed; skip */
		struct kv *n = xcalloc(1, sizeof(*n));
		n->key = key;
		t = lex_next(l, &val);
		if (t == 1)
			n->val = val;
		else if (t == '{')
			n->child = parse_list(l, 1);
		*tail = n;
		tail = &n->next;
		if (t == 0 || (t == '}' && nested))
			return head;
	}
}

struct kv *
kv_parse_text(const char *s, size_t n)
{
	struct lex l = { s, s + n };

	return parse_list(&l, 0);
}

static const uint8_t *
bstr(const uint8_t *p, const uint8_t *end, char **out)
{
	const uint8_t *z = memchr(p, 0, end - p);

	if (z == NULL)
		return NULL;
	*out = xmalloc(z - p + 1);
	memcpy(*out, p, z - p + 1);
	return z + 1;
}

static const uint8_t *
parse_bin(const uint8_t *p, const uint8_t *end, struct kv **out)
{
	struct kv *head = NULL, **tail = &head;

	while (p < end) {
		uint8_t type = *p++;
		struct kv *n;
		char tmp[32];

		if (type == 8 || type == 11)
			break;
		n = xcalloc(1, sizeof(*n));
		if ((p = bstr(p, end, &n->key)) == NULL) {
			free(n);
			break;
		}
		switch (type) {
		case 0:
			p = parse_bin(p, end, &n->child);
			break;
		case 1:
			p = bstr(p, end, &n->val);
			break;
		case 2: case 4: case 6:
			if (end - p < 4)
				p = NULL;
			else {
				snprintf(tmp, sizeof(tmp), "%d", (int32_t)le32(p));
				n->val = xstrdup(tmp);
				p += 4;
			}
			break;
		case 3:
			if (end - p < 4)
				p = NULL;
			else {
				float f;
				uint32_t u = le32(p);
				memcpy(&f, &u, 4);
				snprintf(tmp, sizeof(tmp), "%g", f);
				n->val = xstrdup(tmp);
				p += 4;
			}
			break;
		case 7: case 10:
			if (end - p < 8)
				p = NULL;
			else {
				snprintf(tmp, sizeof(tmp), "%llu",
				    (unsigned long long)le64(p));
				n->val = xstrdup(tmp);
				p += 8;
			}
			break;
		default:
			p = NULL;
		}
		*tail = n;
		tail = &n->next;
		if (p == NULL)
			break;
	}
	*out = head;
	return p;
}

struct kv *
kv_parse_binary(const uint8_t *p, size_t n)
{
	struct kv *root = NULL;

	parse_bin(p, p + n, &root);
	return root;
}

void
kv_free(struct kv *k)
{
	while (k) {
		struct kv *next = k->next;
		kv_free(k->child);
		free(k->key);
		free(k->val);
		free(k);
		k = next;
	}
}

struct kv *
kv_get(struct kv *k, const char *path)
{
	char part[256];
	const char *slash;
	size_t n;

	while (*path) {
		slash = strchr(path, '/');
		n = slash ? (size_t)(slash - path) : strlen(path);
		if (n >= sizeof(part))
			return NULL;
		memcpy(part, path, n);
		part[n] = '\0';
		for (; k; k = k->next)
			if (strcasecmp(k->key, part) == 0)
				break;
		if (k == NULL)
			return NULL;
		if (!slash)
			return k;
		path = slash + 1;
		k = k->child;
	}
	return NULL;
}

const char *
kv_str(struct kv *k, const char *path)
{
	struct kv *n = kv_get(k, path);

	return n ? n->val : NULL;
}

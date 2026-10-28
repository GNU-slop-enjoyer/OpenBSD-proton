/*
 * Copyright (c) 2026 The OpenBSD Proton project contributors
 * Distributed under the LGPL 2.1 or later.
 */
#include <sys/stat.h>
#include <sys/time.h>

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "util.h"

int verbose;

void
buf_put(struct buf *b, const void *p, size_t n)
{
	if (b->len + n + 1 > b->cap) {
		size_t cap = b->cap ? b->cap : 256;
		while (cap < b->len + n + 1)
			cap *= 2;
		b->p = xrealloc(b->p, cap);
		b->cap = cap;
	}
	if (n)
		memcpy(b->p + b->len, p, n);
	b->len += n;
}

void
buf_putc(struct buf *b, int c)
{
	uint8_t ch = c;

	buf_put(b, &ch, 1);
}

void
buf_puts(struct buf *b, const char *s)
{
	buf_put(b, s, strlen(s));
}

void
buf_printf(struct buf *b, const char *fmt, ...)
{
	va_list ap;
	char *s;

	va_start(ap, fmt);
	if (vasprintf(&s, fmt, ap) == -1)
		die("out of memory");
	va_end(ap);
	buf_puts(b, s);
	free(s);
}

void
buf_free(struct buf *b)
{
	free(b->p);
	b->p = NULL;
	b->len = b->cap = 0;
}

char *
buf_str(struct buf *b)
{
	buf_put(b, "", 0);
	b->p[b->len] = '\0';
	return (char *)b->p;
}

void *
xmalloc(size_t n)
{
	void *p = malloc(n ? n : 1);

	if (p == NULL)
		die("out of memory");
	return p;
}

void *
xcalloc(size_t n, size_t m)
{
	void *p = calloc(n ? n : 1, m ? m : 1);

	if (p == NULL)
		die("out of memory");
	return p;
}

void *
xrealloc(void *o, size_t n)
{
	void *p = realloc(o, n ? n : 1);

	if (p == NULL)
		die("out of memory");
	return p;
}

char *
xstrdup(const char *s)
{
	char *p = strdup(s ? s : "");

	if (p == NULL)
		die("out of memory");
	return p;
}

char *
xasprintf(const char *fmt, ...)
{
	va_list ap;
	char *s;

	va_start(ap, fmt);
	if (vasprintf(&s, fmt, ap) == -1)
		die("out of memory");
	va_end(ap);
	return s;
}

void
die(const char *fmt, ...)
{
	va_list ap;

	fflush(stdout);
	fputs("proton-launcher: ", stderr);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
	exit(1);
}

void
warnx_(const char *fmt, ...)
{
	va_list ap;

	fflush(stdout);
	fputs("proton-launcher: ", stderr);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
}

void
debug(const char *fmt, ...)
{
	va_list ap;

	if (!verbose)
		return;
	fputs("debug: ", stderr);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
}

static const char b64chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

char *
b64_encode(const void *data, size_t n)
{
	const uint8_t *p = data;
	char *out = xmalloc(n / 3 * 4 + 5), *o = out;
	size_t i;

	for (i = 0; i + 2 < n; i += 3) {
		*o++ = b64chars[p[i] >> 2];
		*o++ = b64chars[(p[i] & 3) << 4 | p[i + 1] >> 4];
		*o++ = b64chars[(p[i + 1] & 15) << 2 | p[i + 2] >> 6];
		*o++ = b64chars[p[i + 2] & 63];
	}
	if (i < n) {
		*o++ = b64chars[p[i] >> 2];
		if (i + 1 < n) {
			*o++ = b64chars[(p[i] & 3) << 4 | p[i + 1] >> 4];
			*o++ = b64chars[(p[i + 1] & 15) << 2];
		} else {
			*o++ = b64chars[(p[i] & 3) << 4];
			*o++ = '=';
		}
		*o++ = '=';
	}
	*o = '\0';
	return out;
}

int
b64_decode(const char *s, struct buf *out)
{
	uint32_t acc = 0;
	int bits = 0;
	const char *c;

	for (; *s; s++) {
		if (*s == '=' || *s == '\n' || *s == '\r')
			continue;
		if ((c = strchr(b64chars, *s)) == NULL)
			return -1;
		acc = acc << 6 | (uint32_t)(c - b64chars);
		bits += 6;
		if (bits >= 8) {
			bits -= 8;
			buf_putc(out, acc >> bits & 0xff);
		}
	}
	return 0;
}

void
hex_encode(const uint8_t *p, size_t n, char *out)
{
	static const char h[] = "0123456789abcdef";
	size_t i;

	for (i = 0; i < n; i++) {
		out[2 * i] = h[p[i] >> 4];
		out[2 * i + 1] = h[p[i] & 15];
	}
	out[2 * n] = '\0';
}

char *
url_escape(const char *s)
{
	struct buf b = { 0 };

	for (; *s; s++) {
		unsigned char c = *s;
		if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
		    (c >= '0' && c <= '9') || strchr("-_.~", c))
			buf_putc(&b, c);
		else
			buf_printf(&b, "%%%02X", c);
	}
	return buf_str(&b);
}

int
read_file(const char *path, struct buf *b)
{
	char tmp[65536];
	ssize_t n;
	int fd;

	if ((fd = open(path, O_RDONLY)) == -1)
		return -1;
	while ((n = read(fd, tmp, sizeof(tmp))) > 0)
		buf_put(b, tmp, n);
	close(fd);
	buf_str(b);
	return n < 0 ? -1 : 0;
}

int
write_file_atomic(const char *path, const void *p, size_t n, int mode)
{
	char *tmp = xasprintf("%s.tmp", path);
	int fd, ret = -1;

	if ((fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC, mode)) == -1)
		goto out;
	if (write(fd, p, n) != (ssize_t)n || fsync(fd) == -1) {
		close(fd);
		unlink(tmp);
		goto out;
	}
	close(fd);
	if (rename(tmp, path) == -1)
		unlink(tmp);
	else
		ret = 0;
out:
	free(tmp);
	return ret;
}

int
mkdir_p(const char *path, int mode)
{
	char *p = xstrdup(path), *s;

	for (s = p + 1; *s; s++) {
		if (*s != '/')
			continue;
		*s = '\0';
		if (mkdir(p, mode) == -1 && errno != EEXIST) {
			free(p);
			return -1;
		}
		*s = '/';
	}
	if (mkdir(p, mode) == -1 && errno != EEXIST) {
		free(p);
		return -1;
	}
	free(p);
	return 0;
}

const char *
data_dir(void)
{
	static char *dir;
	const char *x, *home;

	if (dir)
		return dir;
	if ((x = getenv("PROTON_LAUNCHER_DIR")) && *x)
		dir = xstrdup(x);
	else if ((x = getenv("XDG_DATA_HOME")) && *x)
		dir = xasprintf("%s/proton-launcher", x);
	else {
		if ((home = getenv("HOME")) == NULL)
			die("HOME is not set");
		dir = xasprintf("%s/.local/share/proton-launcher", home);
	}
	if (mkdir_p(dir, 0700) == -1)
		die("cannot create %s", dir);
	return dir;
}

/*
 * $DATA/settings: "key value" lines, written by hand or by
 * "proton-launcher set".
 */
const char *
config_get(const char *key)
{
	static struct buf b;
	static int loaded;
	size_t klen = strlen(key);
	char *line, *nl;

	if (!loaded) {
		char *path = xasprintf("%s/settings", data_dir());
		loaded = 1;
		if (read_file(path, &b) == -1)
			buf_str(&b);
		free(path);
	}
	for (line = (char *)b.p; line && *line; line = nl) {
		if ((nl = strchr(line, '\n')))
			nl++;
		if (strncmp(line, key, klen) == 0 &&
		    (line[klen] == ' ' || line[klen] == '\t')) {
			static char val[1024];
			const char *v = line + klen;
			size_t n;
			while (*v == ' ' || *v == '\t')
				v++;
			n = nl ? (size_t)(nl - 1 - v) : strlen(v);
			if (n >= sizeof(val))
				n = sizeof(val) - 1;
			memcpy(val, v, n);
			val[n] = '\0';
			return val;
		}
	}
	return NULL;
}

uint32_t
le32(const uint8_t *p)
{
	return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24;
}

uint64_t
le64(const uint8_t *p)
{
	return le32(p) | (uint64_t)le32(p + 4) << 32;
}

double
now_sec(void)
{
	struct timeval tv;

	gettimeofday(&tv, NULL);
	return tv.tv_sec + tv.tv_usec / 1e6;
}

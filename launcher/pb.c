/*
 * Copyright (c) 2026 The OpenBSD Proton project contributors
 * Distributed under the LGPL 2.1 or later.
 */
#include <string.h>

#include "pb.h"

static void
put_varint(struct buf *b, uint64_t v)
{
	uint8_t tmp[10];
	int n = 0;

	do {
		tmp[n] = v & 0x7f;
		v >>= 7;
		if (v)
			tmp[n] |= 0x80;
		n++;
	} while (v);
	buf_put(b, tmp, n);
}

static void
put_key(struct buf *b, uint32_t field, int wt)
{
	put_varint(b, (uint64_t)field << 3 | wt);
}

void
pb_varint(struct buf *b, uint32_t field, uint64_t v)
{
	put_key(b, field, PB_VARINT);
	put_varint(b, v);
}

void
pb_fixed32(struct buf *b, uint32_t field, uint32_t v)
{
	uint8_t t[4] = { v, v >> 8, v >> 16, v >> 24 };

	put_key(b, field, PB_FIXED32);
	buf_put(b, t, 4);
}

void
pb_fixed64(struct buf *b, uint32_t field, uint64_t v)
{
	uint8_t t[8];
	int i;

	for (i = 0; i < 8; i++)
		t[i] = v >> (8 * i);
	put_key(b, field, PB_FIXED64);
	buf_put(b, t, 8);
}

void
pb_bytes(struct buf *b, uint32_t field, const void *p, size_t n)
{
	put_key(b, field, PB_LEN);
	put_varint(b, n);
	buf_put(b, p, n);
}

void
pb_str(struct buf *b, uint32_t field, const char *s)
{
	pb_bytes(b, field, s, strlen(s));
}

void
pb_msg(struct buf *b, uint32_t field, const struct buf *m)
{
	pb_bytes(b, field, m->p, m->len);
}

void
pbr_init(struct pbr *r, const void *p, size_t n)
{
	r->p = p;
	r->end = r->p + n;
}

static int
get_varint(struct pbr *r, uint64_t *v)
{
	int shift = 0;

	*v = 0;
	while (r->p < r->end && shift < 64) {
		uint8_t c = *r->p++;
		*v |= (uint64_t)(c & 0x7f) << shift;
		if (!(c & 0x80))
			return 0;
		shift += 7;
	}
	return -1;
}

int
pbr_next(struct pbr *r, struct pbf *f)
{
	uint64_t key, n;

	if (r->p >= r->end)
		return 0;
	if (get_varint(r, &key) == -1)
		return -1;
	f->field = key >> 3;
	f->wt = key & 7;
	f->data = NULL;
	f->len = 0;
	f->v = 0;
	switch (f->wt) {
	case PB_VARINT:
		return get_varint(r, &f->v) == -1 ? -1 : 1;
	case PB_FIXED64:
		if (r->end - r->p < 8)
			return -1;
		f->v = le64(r->p);
		r->p += 8;
		return 1;
	case PB_FIXED32:
		if (r->end - r->p < 4)
			return -1;
		f->v = le32(r->p);
		r->p += 4;
		return 1;
	case PB_LEN:
		if (get_varint(r, &n) == -1 || n > (uint64_t)(r->end - r->p))
			return -1;
		f->data = r->p;
		f->len = n;
		r->p += n;
		return 1;
	default:
		return -1;
	}
}

int
pb_get_u64(const void *p, size_t n, uint32_t field, uint64_t *v)
{
	struct pbr r;
	struct pbf f;
	int found = 0;

	pbr_init(&r, p, n);
	while (pbr_next(&r, &f) == 1)
		if (f.field == field && f.wt != PB_LEN) {
			*v = f.v;
			found = 1;
		}
	return found;
}

int
pb_get_bytes(const void *p, size_t n, uint32_t field, const uint8_t **d,
    size_t *len)
{
	struct pbr r;
	struct pbf f;
	int found = 0;

	pbr_init(&r, p, n);
	while (pbr_next(&r, &f) == 1)
		if (f.field == field && f.wt == PB_LEN) {
			*d = f.data;
			*len = f.len;
			found = 1;
		}
	return found;
}

char *
pb_get_str(const void *p, size_t n, uint32_t field)
{
	const uint8_t *d;
	size_t len;
	char *s;

	if (!pb_get_bytes(p, n, field, &d, &len))
		return NULL;
	s = xmalloc(len + 1);
	memcpy(s, d, len);
	s[len] = '\0';
	return s;
}

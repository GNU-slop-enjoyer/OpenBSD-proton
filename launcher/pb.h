/*
 * Minimal protocol buffers wire format: enough to build and read the
 * Steam messages proton-launcher uses, without generated code.
 *
 * Copyright (c) 2026 The OpenBSD Proton project contributors
 * Distributed under the LGPL 2.1 or later.
 */
#ifndef PB_H
#define PB_H

#include "util.h"

enum { PB_VARINT = 0, PB_FIXED64 = 1, PB_LEN = 2, PB_FIXED32 = 5 };

void	pb_varint(struct buf *, uint32_t field, uint64_t);
void	pb_fixed32(struct buf *, uint32_t field, uint32_t);
void	pb_fixed64(struct buf *, uint32_t field, uint64_t);
void	pb_bytes(struct buf *, uint32_t field, const void *, size_t);
void	pb_str(struct buf *, uint32_t field, const char *);
void	pb_msg(struct buf *, uint32_t field, const struct buf *);

struct pbr {
	const uint8_t	*p, *end;
};

struct pbf {
	uint32_t	 field;
	int		 wt;
	uint64_t	 v;	/* varint/fixed value */
	const uint8_t	*data;	/* PB_LEN payload */
	size_t		 len;
};

void	pbr_init(struct pbr *, const void *, size_t);
int	pbr_next(struct pbr *, struct pbf *);	/* 1 field, 0 end, -1 bad */

/* Convenience lookups on a whole message (first/last occurrence). */
int	pb_get_u64(const void *, size_t, uint32_t field, uint64_t *);
int	pb_get_bytes(const void *, size_t, uint32_t field,
	    const uint8_t **, size_t *);
char	*pb_get_str(const void *, size_t, uint32_t field);	/* malloc'd */

#endif

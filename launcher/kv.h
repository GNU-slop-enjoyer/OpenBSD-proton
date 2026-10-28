/*
 * Valve KeyValues: the text form (PICS app info) and the binary form
 * (PICS package info).
 *
 * Copyright (c) 2026 The OpenBSD Proton project contributors
 * Distributed under the LGPL 2.1 or later.
 */
#ifndef KV_H
#define KV_H

#include "util.h"

struct kv {
	char		*key;
	char		*val;		/* NULL for a subtree */
	struct kv	*child;		/* first child */
	struct kv	*next;		/* next sibling */
};

struct kv	*kv_parse_text(const char *, size_t);
struct kv	*kv_parse_binary(const uint8_t *, size_t);
void		 kv_free(struct kv *);
/* Path lookup with '/' separators, case-insensitive keys. */
struct kv	*kv_get(struct kv *, const char *path);
const char	*kv_str(struct kv *, const char *path);	/* NULL if absent */

#endif

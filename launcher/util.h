/*
 * proton-launcher: small helpers shared by every module.
 *
 * Copyright (c) 2026 The OpenBSD Proton project contributors
 * Distributed under the LGPL 2.1 or later.
 */
#ifndef UTIL_H
#define UTIL_H

#include <stddef.h>
#include <stdint.h>

struct buf {
	uint8_t	*p;
	size_t	 len, cap;
};

void	 buf_put(struct buf *, const void *, size_t);
void	 buf_putc(struct buf *, int);
void	 buf_puts(struct buf *, const char *);
void	 buf_printf(struct buf *, const char *, ...)
	    __attribute__((format(printf, 2, 3)));
void	 buf_free(struct buf *);
char	*buf_str(struct buf *);		/* NUL-terminate, return p */

void	*xmalloc(size_t);
void	*xcalloc(size_t, size_t);
void	*xrealloc(void *, size_t);
char	*xstrdup(const char *);
char	*xasprintf(const char *, ...) __attribute__((format(printf, 1, 2)));

void	 die(const char *, ...) __attribute__((format(printf, 1, 2), noreturn));
void	 warnx_(const char *, ...) __attribute__((format(printf, 1, 2)));
extern int verbose;
void	 debug(const char *, ...) __attribute__((format(printf, 1, 2)));

char	*b64_encode(const void *, size_t);
int	 b64_decode(const char *, struct buf *);
void	 hex_encode(const uint8_t *, size_t, char *);	/* 2n+1 bytes */
char	*url_escape(const char *);

int	 read_file(const char *, struct buf *);
int	 write_file_atomic(const char *, const void *, size_t, int mode);
int	 mkdir_p(const char *, int mode);

const char *data_dir(void);	/* $XDG_DATA_HOME/proton-launcher */
const char *config_get(const char *key);	/* $DATA/settings, may be NULL */

uint32_t le32(const uint8_t *);
uint64_t le64(const uint8_t *);
double	 now_sec(void);

#endif

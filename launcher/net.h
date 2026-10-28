/*
 * TLS connections, a small HTTP/1.1 client with keep-alive, and a
 * WebSocket client (RFC 6455) for Steam's connection managers.
 *
 * Copyright (c) 2026 The OpenBSD Proton project contributors
 * Distributed under the LGPL 2.1 or later.
 */
#ifndef NET_H
#define NET_H

#include <openssl/ssl.h>

#include "util.h"

struct conn {
	int	 fd;
	SSL	*ssl;
	char	*host;
	char	*port;
	uint8_t	 rbuf[65536];
	size_t	 rpos, rlen;
};

int	conn_open(struct conn *, const char *host, const char *port, int tls);
void	conn_close(struct conn *);
int	conn_write(struct conn *, const void *, size_t);
int	conn_read(struct conn *, void *, size_t);	/* exact */
int	conn_wait(struct conn *, int ms);	/* 1 readable, 0 timeout */

struct http_resp {
	int		 status;
	struct buf	 body;
	char		*headers;	/* raw, lower-cased names */
};

/*
 * GET or POST url ("https://host[:port]/path").  A struct conn may be
 * passed to reuse a keep-alive connection to the same host; it is
 * reopened as needed.  Returns 0 when a response was read.
 */
int	http_request(struct conn *, const char *method, const char *url,
	    const char *ctype, const void *body, size_t blen,
	    struct http_resp *);
void	http_resp_free(struct http_resp *);
const char *http_header(const struct http_resp *, const char *name);

int	ws_connect(struct conn *, const char *host, const char *port,
	    const char *path);
int	ws_send(struct conn *, const void *, size_t);
/* Read one binary message; answers pings.  -1 on close or error. */
int	ws_recv(struct conn *, struct buf *);

void	net_init(void);

#endif

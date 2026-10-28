/*
 * Copyright (c) 2026 The OpenBSD Proton project contributors
 * Distributed under the LGPL 2.1 or later.
 */
#include <sys/socket.h>
#include <sys/time.h>

#include <netinet/in.h>
#include <netinet/tcp.h>

#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/x509v3.h>

#include "net.h"

static SSL_CTX *ctx;

void
net_init(void)
{
	const char *ca;

	SSL_library_init();
	SSL_load_error_strings();
	if ((ctx = SSL_CTX_new(TLS_client_method())) == NULL)
		die("SSL_CTX_new failed");
	SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);
	if ((ca = getenv("SSL_CERT_FILE")) != NULL)
		SSL_CTX_load_verify_locations(ctx, ca, NULL);
	else if (access("/etc/ssl/cert.pem", R_OK) == 0)
		SSL_CTX_load_verify_locations(ctx, "/etc/ssl/cert.pem", NULL);
	else
		SSL_CTX_set_default_verify_paths(ctx);
}

static const struct timeval io_timeout = { 60, 0 };

int
conn_open(struct conn *c, const char *host, const char *port, int tls)
{
	struct addrinfo hints = { 0 }, *res, *ai;
	int fd = -1, one = 1, e;

	memset(c, 0, sizeof(*c));
	c->fd = -1;
	hints.ai_socktype = SOCK_STREAM;
	if ((e = getaddrinfo(host, port, &hints, &res)) != 0) {
		warnx_("%s: %s", host, gai_strerror(e));
		return -1;
	}
	for (ai = res; ai; ai = ai->ai_next) {
		if ((fd = socket(ai->ai_family, ai->ai_socktype,
		    ai->ai_protocol)) == -1)
			continue;
		if (connect(fd, ai->ai_addr, ai->ai_addrlen) == 0)
			break;
		close(fd);
		fd = -1;
	}
	freeaddrinfo(res);
	if (fd == -1) {
		warnx_("cannot connect to %s:%s", host, port);
		return -1;
	}
	setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
	/*
	 * A connection that goes silent (a NAT dropping it, a stalled
	 * server) must not hang a download thread forever: reads and
	 * writes fail after a minute and the request is retried.
	 */
	setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &io_timeout, sizeof(io_timeout));
	setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &io_timeout, sizeof(io_timeout));
	c->fd = fd;
	c->host = xstrdup(host);
	c->port = xstrdup(port);
	if (!tls)
		return 0;
	c->ssl = SSL_new(ctx);
	SSL_set_fd(c->ssl, fd);
	SSL_set_tlsext_host_name(c->ssl, host);
	X509_VERIFY_PARAM_set1_host(SSL_get0_param(c->ssl), host, 0);
	if (SSL_connect(c->ssl) != 1) {
		warnx_("TLS handshake with %s failed: %s", host,
		    ERR_error_string(ERR_get_error(), NULL));
		conn_close(c);
		return -1;
	}
	return 0;
}

void
conn_close(struct conn *c)
{
	if (c->ssl) {
		SSL_shutdown(c->ssl);
		SSL_free(c->ssl);
	}
	if (c->fd != -1)
		close(c->fd);
	free(c->host);
	free(c->port);
	memset(c, 0, sizeof(*c));
	c->fd = -1;
}

int
conn_write(struct conn *c, const void *p, size_t n)
{
	const uint8_t *s = p;

	while (n > 0) {
		ssize_t w;
		if (c->ssl) {
			int r = SSL_write(c->ssl, s, n > 1 << 30 ? 1 << 30 : n);
			if (r <= 0)
				return -1;
			w = r;
		} else if ((w = write(c->fd, s, n)) == -1) {
			if (errno == EINTR)
				continue;
			return -1;
		}
		s += w;
		n -= w;
	}
	return 0;
}

static int
fill(struct conn *c)
{
	ssize_t n;

	c->rpos = 0;
	if (c->ssl) {
		int r = SSL_read(c->ssl, c->rbuf, sizeof(c->rbuf));
		if (r <= 0)
			return -1;
		n = r;
	} else {
		do
			n = read(c->fd, c->rbuf, sizeof(c->rbuf));
		while (n == -1 && errno == EINTR);
		if (n <= 0)
			return -1;
	}
	c->rlen = n;
	return 0;
}

int
conn_read(struct conn *c, void *p, size_t n)
{
	uint8_t *d = p;

	while (n > 0) {
		size_t k;
		if (c->rpos == c->rlen && fill(c) == -1)
			return -1;
		k = c->rlen - c->rpos;
		if (k > n)
			k = n;
		memcpy(d, c->rbuf + c->rpos, k);
		c->rpos += k;
		d += k;
		n -= k;
	}
	return 0;
}

int
conn_wait(struct conn *c, int ms)
{
	struct pollfd pfd = { c->fd, POLLIN, 0 };

	if (c->rpos < c->rlen || (c->ssl && SSL_pending(c->ssl) > 0))
		return 1;
	return poll(&pfd, 1, ms) > 0;
}

/* Read a line terminated by CRLF into s (without the CRLF). */
static int
read_line(struct conn *c, struct buf *s)
{
	char ch;

	s->len = 0;
	for (;;) {
		if (conn_read(c, &ch, 1) == -1)
			return -1;
		if (ch == '\n')
			break;
		if (ch != '\r')
			buf_putc(s, ch);
	}
	buf_str(s);
	return 0;
}

static int
parse_url(const char *url, int *tls, char **host, char **port, char **path)
{
	const char *h, *e, *slash;
	char *hp, *colon;

	if (strncmp(url, "https://", 8) == 0) {
		*tls = 1;
		h = url + 8;
	} else if (strncmp(url, "http://", 7) == 0) {
		*tls = 0;
		h = url + 7;
	} else
		return -1;
	slash = strchr(h, '/');
	e = slash ? slash : h + strlen(h);
	hp = xmalloc(e - h + 1);
	memcpy(hp, h, e - h);
	hp[e - h] = '\0';
	if ((colon = strrchr(hp, ':')) != NULL) {
		*colon = '\0';
		*port = xstrdup(colon + 1);
	} else
		*port = xstrdup(*tls ? "443" : "80");
	*host = hp;
	*path = xstrdup(slash ? slash : "/");
	return 0;
}

const char *
http_header(const struct http_resp *r, const char *name)
{
	size_t n = strlen(name);
	const char *p;

	for (p = r->headers; p && *p; p = strchr(p, '\n') ? strchr(p, '\n') + 1 : NULL) {
		if (strncmp(p, name, n) == 0 && p[n] == ':') {
			static char val[512];
			const char *v = p + n + 1, *e;
			size_t k;
			while (*v == ' ')
				v++;
			e = strchr(v, '\n');
			k = e ? (size_t)(e - v) : strlen(v);
			if (k >= sizeof(val))
				k = sizeof(val) - 1;
			memcpy(val, v, k);
			val[k] = '\0';
			return val;
		}
	}
	return NULL;
}

static int
http_once(struct conn *c, const char *method, const char *host,
    const char *path, const char *ctype, const void *body, size_t blen,
    struct http_resp *r)
{
	struct buf req = { 0 }, line = { 0 }, hdr = { 0 };
	const char *v;
	int chunked = 0, closeit = 0, ret = -1;
	long long clen = -1;
	char *s;

	buf_printf(&req, "%s %s HTTP/1.1\r\nHost: %s\r\n"
	    "User-Agent: proton-launcher/0.1\r\nAccept-Encoding: identity\r\n"
	    "Connection: keep-alive\r\n", method, path, host);
	if (body) {
		buf_printf(&req, "Content-Type: %s\r\nContent-Length: %zu\r\n",
		    ctype ? ctype : "application/x-www-form-urlencoded", blen);
	}
	buf_puts(&req, "\r\n");
	if (body)
		buf_put(&req, body, blen);
	if (conn_write(c, req.p, req.len) == -1)
		goto out;
	if (read_line(c, &line) == -1 || strncmp((char *)line.p, "HTTP/1.", 7))
		goto out;
	r->status = atoi((char *)line.p + 9);
	for (;;) {
		if (read_line(c, &line) == -1)
			goto out;
		if (line.len == 0)
			break;
		for (s = (char *)line.p; *s && *s != ':'; s++)
			*s = tolower((unsigned char)*s);
		buf_puts(&hdr, (char *)line.p);
		buf_putc(&hdr, '\n');
	}
	r->headers = buf_str(&hdr);
	hdr.p = NULL;
	if ((v = http_header(r, "transfer-encoding")) && strstr(v, "chunked"))
		chunked = 1;
	if ((v = http_header(r, "content-length")))
		clen = atoll(v);
	if ((v = http_header(r, "connection")) && strcasecmp(v, "close") == 0)
		closeit = 1;
	if (strcmp(method, "HEAD") == 0 || r->status == 204 || r->status == 304)
		clen = 0;
	if (chunked) {
		for (;;) {
			long n;
			uint8_t tmp[16384];
			if (read_line(c, &line) == -1)
				goto out;
			n = strtol((char *)line.p, NULL, 16);
			if (n == 0) {
				do
					if (read_line(c, &line) == -1)
						goto out;
				while (line.len);
				break;
			}
			while (n > 0) {
				size_t k = n > (long)sizeof(tmp) ? sizeof(tmp) : (size_t)n;
				if (conn_read(c, tmp, k) == -1)
					goto out;
				buf_put(&r->body, tmp, k);
				n -= k;
			}
			if (read_line(c, &line) == -1)
				goto out;
		}
	} else if (clen >= 0) {
		uint8_t tmp[16384];
		while (clen > 0) {
			size_t k = clen > (long long)sizeof(tmp) ? sizeof(tmp) :
			    (size_t)clen;
			if (conn_read(c, tmp, k) == -1)
				goto out;
			buf_put(&r->body, tmp, k);
			clen -= k;
		}
	} else {
		uint8_t tmp[16384];
		size_t k;
		closeit = 1;
		for (;;) {
			if (c->rpos == c->rlen && fill(c) == -1)
				break;
			k = c->rlen - c->rpos;
			if (k > sizeof(tmp))
				k = sizeof(tmp);
			buf_put(&r->body, c->rbuf + c->rpos, k);
			c->rpos += k;
		}
	}
	buf_str(&r->body);
	ret = closeit ? 1 : 0;
out:
	buf_free(&req);
	buf_free(&line);
	buf_free(&hdr);
	return ret;
}

int
http_request(struct conn *c, const char *method, const char *url,
    const char *ctype, const void *body, size_t blen, struct http_resp *r)
{
	struct conn local = { .fd = -1 };
	char *host, *port, *path;
	int tls, try, ret = -1;

	memset(r, 0, sizeof(*r));
	if (parse_url(url, &tls, &host, &port, &path) == -1) {
		warnx_("bad URL %s", url);
		return -1;
	}
	if (c == NULL)
		c = &local;
	for (try = 0; try < 2; try++) {
		int fresh = 0;
		if (c->fd != -1 && (strcmp(c->host, host) ||
		    strcmp(c->port, port) || (c->ssl != NULL) != tls))
			conn_close(c);
		if (c->fd == -1) {
			if (conn_open(c, host, port, tls) == -1)
				break;
			fresh = 1;
		}
		http_resp_free(r);
		memset(r, 0, sizeof(*r));
		ret = http_once(c, method, host, path, ctype, body, blen, r);
		if (ret == 1) {
			conn_close(c);
			ret = 0;
		}
		if (ret == 0)
			break;
		conn_close(c);
		if (fresh)
			break;	/* a reused connection may have timed out */
	}
	if (c == &local && c->fd != -1)
		conn_close(c);
	free(host);
	free(port);
	free(path);
	return ret;
}

void
http_resp_free(struct http_resp *r)
{
	buf_free(&r->body);
	free(r->headers);
	r->headers = NULL;
}

int
ws_connect(struct conn *c, const char *host, const char *port,
    const char *path)
{
	struct buf req = { 0 }, line = { 0 };
	uint8_t key[16];
	char *k64;
	int ok = 0;

	if (conn_open(c, host, port, 1) == -1)
		return -1;
	RAND_bytes(key, sizeof(key));
	k64 = b64_encode(key, sizeof(key));
	buf_printf(&req, "GET %s HTTP/1.1\r\nHost: %s:%s\r\n"
	    "Upgrade: websocket\r\nConnection: Upgrade\r\n"
	    "Sec-WebSocket-Key: %s\r\nSec-WebSocket-Version: 13\r\n\r\n",
	    path, host, port, k64);
	free(k64);
	if (conn_write(c, req.p, req.len) == 0 && read_line(c, &line) == 0 &&
	    strncmp((char *)line.p, "HTTP/1.1 101", 12) == 0) {
		ok = 1;
		while (read_line(c, &line) == 0 && line.len)
			;
	}
	if (!ok)
		warnx_("websocket upgrade at %s failed: %s", host,
		    line.p ? (char *)line.p : "no response");
	buf_free(&req);
	buf_free(&line);
	if (!ok) {
		conn_close(c);
		return -1;
	}
	return 0;
}

static int
ws_frame(struct conn *c, int op, const void *p, size_t n)
{
	uint8_t hdr[14], mask[4];
	uint8_t *m;
	size_t h = 0, i;
	int ret;

	hdr[h++] = 0x80 | op;
	if (n < 126)
		hdr[h++] = 0x80 | n;
	else if (n < 65536) {
		hdr[h++] = 0x80 | 126;
		hdr[h++] = n >> 8;
		hdr[h++] = n;
	} else {
		hdr[h++] = 0x80 | 127;
		for (i = 0; i < 8; i++)
			hdr[h++] = (uint64_t)n >> (56 - 8 * i);
	}
	RAND_bytes(mask, 4);
	memcpy(hdr + h, mask, 4);
	h += 4;
	m = xmalloc(h + n);
	memcpy(m, hdr, h);
	for (i = 0; i < n; i++)
		m[h + i] = ((const uint8_t *)p)[i] ^ mask[i & 3];
	ret = conn_write(c, m, h + n);
	free(m);
	return ret;
}

int
ws_send(struct conn *c, const void *p, size_t n)
{
	return ws_frame(c, 2, p, n);
}

int
ws_recv(struct conn *c, struct buf *msg)
{
	msg->len = 0;
	for (;;) {
		uint8_t h[2], ext[8], mask[4] = { 0 };
		uint64_t n;
		int op, fin, masked, i;
		uint8_t *data;

		if (conn_read(c, h, 2) == -1)
			return -1;
		fin = h[0] & 0x80;
		op = h[0] & 0x0f;
		masked = h[1] & 0x80;
		n = h[1] & 0x7f;
		if (n == 126) {
			if (conn_read(c, ext, 2) == -1)
				return -1;
			n = ext[0] << 8 | ext[1];
		} else if (n == 127) {
			if (conn_read(c, ext, 8) == -1)
				return -1;
			for (n = 0, i = 0; i < 8; i++)
				n = n << 8 | ext[i];
		}
		if (masked && conn_read(c, mask, 4) == -1)
			return -1;
		if (n > 256u << 20)
			return -1;
		data = xmalloc(n);
		if (conn_read(c, data, n) == -1) {
			free(data);
			return -1;
		}
		if (masked)
			for (i = 0; (uint64_t)i < n; i++)
				data[i] ^= mask[i & 3];
		switch (op) {
		case 0x9:	/* ping */
			ws_frame(c, 0xa, data, n);
			free(data);
			continue;
		case 0xa:
			free(data);
			continue;
		case 0x8:
			debug("websocket closed by server");
			free(data);
			return -1;
		default:
			buf_put(msg, data, n);
			free(data);
			if (fin)
				return 0;
		}
	}
}

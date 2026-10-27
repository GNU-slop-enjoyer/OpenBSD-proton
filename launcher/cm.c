/*
 * Copyright (c) 2026 The OpenBSD Proton project contributors
 * Distributed under the LGPL 2.1 or later.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <openssl/rand.h>
#include <openssl/sha.h>
#include <zlib.h>

#include "cm.h"
#include "pb.h"

/* CMsgProtoBufHeader field numbers (steammessages_base.proto). */
enum {
	HDR_STEAMID = 1, HDR_SESSIONID = 2, HDR_JOBID_SOURCE = 10,
	HDR_JOBID_TARGET = 11, HDR_TARGET_JOB_NAME = 12, HDR_ERESULT = 13,
	HDR_ERROR_MESSAGE = 14,
};

#define STEAMID_ANON		0x01A0000000000000ull	/* universe 1, AnonUser */
#define STEAMID_INDIVIDUAL	0x0110000100000000ull	/* universe 1, desktop */
#define EOSTYPE_WIN10		16

const char *
eresult_str(int e)
{
	switch (e) {
	case 1: return "OK";
	case 2: return "failure";
	case 3: return "no connection";
	case 5: return "invalid password";
	case 6: return "logged in elsewhere";
	case 8: return "invalid parameter";
	case 9: return "file not found";
	case 10: return "busy";
	case 15: return "access denied";
	case 16: return "timeout";
	case 18: return "account not found";
	case 20: return "service unavailable";
	case 25: return "limit exceeded";
	case 27: return "expired";
	case 29: return "duplicate request";
	case 42: return "no match";
	case 63: return "account logon denied (Steam Guard)";
	case 65: return "invalid Steam Guard code";
	case 84: return "rate limit exceeded";
	case 85: return "two-factor code needed";
	case 88: return "two-factor code mismatch";
	default: return "error";
	}
}

static int
cm_pick_server(char **host, char **port)
{
	struct http_resp r;
	const char *p, *e;
	char *ep = NULL, *colon;

	if ((p = getenv("PROTON_LAUNCHER_CM")) && *p) {
		ep = xstrdup(p);
		goto split;
	}
	if (http_request(NULL, "GET", "https://api.steampowered.com/"
	    "ISteamDirectory/GetCMListForConnect/v1/?cellid=0&format=json",
	    NULL, NULL, 0, &r) == -1 || r.status != 200) {
		warnx_("cannot fetch the Steam server list");
		http_resp_free(&r);
		return -1;
	}
	/* {"endpoint":"host:port","legacy_endpoint":..,"type":"websockets",..} */
	for (p = (char *)r.body.p; (p = strstr(p, "\"endpoint\":\"")); p = e) {
		const char *obj_end = strchr(p, '}');
		p += 12;
		if ((e = strchr(p, '"')) == NULL)
			break;
		if (obj_end == NULL)
			obj_end = p + strlen(p);
		if (memmem(p, obj_end - p, "\"websockets\"", 12) == NULL)
			continue;
		ep = xmalloc(e - p + 1);
		memcpy(ep, p, e - p);
		ep[e - p] = '\0';
		break;
	}
	http_resp_free(&r);
	if (ep == NULL) {
		warnx_("no WebSocket Steam servers listed");
		return -1;
	}
split:
	if ((colon = strrchr(ep, ':')) == NULL) {
		*host = ep;
		*port = xstrdup("443");
	} else {
		*colon = '\0';
		*host = ep;
		*port = xstrdup(colon + 1);
	}
	return 0;
}

int
cm_connect(struct cm *cm)
{
	struct buf hello = { 0 };
	char *host, *port;
	int ret;

	memset(cm, 0, sizeof(*cm));
	cm->c.fd = -1;
	cm->steamid = 0;	/* until logon, as the Steam client sends */
	cm->next_job = 1;
	cm->heartbeat = 9;
	if (cm_pick_server(&host, &port) == -1)
		return -1;
	debug("connecting to CM %s:%s", host, port);
	ret = ws_connect(&cm->c, host, port, "/cmsocket/");
	free(host);
	free(port);
	if (ret == -1)
		return -1;
	pb_varint(&hello, 1, PROTOCOL_VERSION);
	ret = cm_send(cm, EMSG_CLIENT_HELLO, &hello, 0, NULL);
	buf_free(&hello);
	return ret;
}

void
cm_close(struct cm *cm)
{
	size_t i;

	if (cm->c.fd != -1 && cm->logged_on) {
		struct buf b = { 0 };
		cm_send(cm, EMSG_CLIENT_LOGOFF, &b, 0, NULL);
	}
	conn_close(&cm->c);
	for (i = cm->qhead; i < cm->nqueue; i++)
		buf_free(&cm->queue[i]);
	free(cm->queue);
	free(cm->licenses);
	cm->queue = NULL;
	cm->licenses = NULL;
	cm->nqueue = cm->qhead = cm->nlicenses = 0;
}

int
cm_send(struct cm *cm, uint32_t emsg, const struct buf *body,
    uint64_t jobid_source, const char *target_job_name)
{
	struct buf hdr = { 0 }, m = { 0 };
	uint8_t t[8];
	int ret;

	pb_fixed64(&hdr, HDR_STEAMID, cm->steamid);
	pb_varint(&hdr, HDR_SESSIONID, (uint32_t)cm->session_id);
	if (jobid_source)
		pb_fixed64(&hdr, HDR_JOBID_SOURCE, jobid_source);
	if (target_job_name)
		pb_str(&hdr, HDR_TARGET_JOB_NAME, target_job_name);
	emsg |= PROTO_MASK;
	t[0] = emsg; t[1] = emsg >> 8; t[2] = emsg >> 16; t[3] = emsg >> 24;
	t[4] = hdr.len; t[5] = hdr.len >> 8; t[6] = hdr.len >> 16;
	t[7] = hdr.len >> 24;
	buf_put(&m, t, 8);
	buf_put(&m, hdr.p, hdr.len);
	buf_put(&m, body->p, body->len);
	ret = ws_send(&cm->c, m.p, m.len);
	cm->last_send = now_sec();
	buf_free(&hdr);
	buf_free(&m);
	return ret;
}

static void
queue_push(struct cm *cm, const uint8_t *p, size_t n)
{
	struct buf b = { 0 };

	buf_put(&b, p, n);
	if (cm->qhead == cm->nqueue) {
		cm->qhead = cm->nqueue = 0;
	}
	cm->queue = xrealloc(cm->queue, (cm->nqueue + 1) * sizeof(*cm->queue));
	cm->queue[cm->nqueue++] = b;
}

static int
gunzip(const uint8_t *p, size_t n, struct buf *out)
{
	z_stream z = { 0 };
	uint8_t tmp[65536];
	int r;

	if (inflateInit2(&z, 16 + MAX_WBITS) != Z_OK)
		return -1;
	z.next_in = (uint8_t *)p;
	z.avail_in = n;
	do {
		z.next_out = tmp;
		z.avail_out = sizeof(tmp);
		r = inflate(&z, Z_NO_FLUSH);
		if (r != Z_OK && r != Z_STREAM_END) {
			inflateEnd(&z);
			return -1;
		}
		buf_put(out, tmp, sizeof(tmp) - z.avail_out);
	} while (r != Z_STREAM_END);
	inflateEnd(&z);
	return 0;
}

static void
unpack_multi(struct cm *cm, const uint8_t *body, size_t blen)
{
	struct buf unz = { 0 };
	const uint8_t *d, *p, *end;
	size_t len;
	uint64_t size_unzipped = 0;

	if (!pb_get_bytes(body, blen, 2, &d, &len))
		return;
	pb_get_u64(body, blen, 1, &size_unzipped);
	if (size_unzipped) {
		if (gunzip(d, len, &unz) == -1) {
			warnx_("bad compressed message from Steam");
			buf_free(&unz);
			return;
		}
		d = unz.p;
		len = unz.len;
	}
	for (p = d, end = d + len; end - p >= 4;) {
		uint32_t n = le32(p);
		p += 4;
		if (n > (size_t)(end - p))
			break;
		queue_push(cm, p, n);
		p += n;
	}
	buf_free(&unz);
}

static int
parse_msg(struct cm *cm, struct cm_msg *m)
{
	const uint8_t *p = m->raw.p;
	uint32_t hlen;
	struct pbr r;
	struct pbf f;

	if (m->raw.len < 8)
		return -1;
	m->emsg = le32(p);
	m->eresult = 1;
	m->jobid_target = 0;
	m->error_message = NULL;
	if (!(m->emsg & PROTO_MASK)) {
		/* Non-protobuf messages are not used by this client. */
		m->emsg &= ~PROTO_MASK;
		m->body = NULL;
		m->blen = 0;
		return 0;
	}
	m->emsg &= ~PROTO_MASK;
	hlen = le32(p + 4);
	if (hlen > m->raw.len - 8)
		return -1;
	pbr_init(&r, p + 8, hlen);
	while (pbr_next(&r, &f) == 1) {
		switch (f.field) {
		case HDR_STEAMID:
			if (f.v)
				cm->steamid = f.v;
			break;
		case HDR_SESSIONID:
			if (f.v)
				cm->session_id = (int32_t)f.v;
			break;
		case HDR_JOBID_TARGET:
			m->jobid_target = f.v;
			break;
		case HDR_ERESULT:
			m->eresult = (int32_t)f.v;
			break;
		case HDR_ERROR_MESSAGE:
			free(m->error_message);
			m->error_message = xmalloc(f.len + 1);
			memcpy(m->error_message, f.data, f.len);
			m->error_message[f.len] = '\0';
			break;
		}
	}
	m->body = p + 8 + hlen;
	m->blen = m->raw.len - 8 - hlen;
	return 0;
}

static void
note_message(struct cm *cm, struct cm_msg *m)
{
	struct pbr r;
	struct pbf f;

	if (m->emsg != EMSG_CLIENT_LICENSE_LIST)
		return;
	free(cm->licenses);
	cm->licenses = NULL;
	cm->nlicenses = 0;
	pbr_init(&r, m->body, m->blen);
	while (pbr_next(&r, &f) == 1) {
		struct license l = { 0 };
		uint64_t v;
		if (f.field != 2 || f.wt != PB_LEN)
			continue;
		if (pb_get_u64(f.data, f.len, 1, &v))
			l.package_id = v;
		if (pb_get_u64(f.data, f.len, 17, &v))
			l.access_token = v;
		cm->licenses = xrealloc(cm->licenses,
		    (cm->nlicenses + 1) * sizeof(*cm->licenses));
		cm->licenses[cm->nlicenses++] = l;
	}
	cm->have_licenses = 1;
	debug("%zu licenses", cm->nlicenses);
}

int
cm_recv(struct cm *cm, struct cm_msg *m, int timeout)
{
	double deadline = timeout < 0 ? 0 : now_sec() + timeout;

	memset(m, 0, sizeof(*m));
	for (;;) {
		if (cm->qhead < cm->nqueue) {
			m->raw = cm->queue[cm->qhead++];
		} else {
			int wait = 1000;
			if (cm->logged_on && now_sec() - cm->last_send >=
			    cm->heartbeat) {
				struct buf hb = { 0 };
				cm_send(cm, EMSG_CLIENT_HEARTBEAT, &hb, 0, NULL);
			}
			if (timeout >= 0 && now_sec() > deadline)
				return -1;
			if (!conn_wait(&cm->c, wait))
				continue;
			if (ws_recv(&cm->c, &m->raw) == -1) {
				warnx_("connection to Steam lost");
				return -1;
			}
		}
		if (parse_msg(cm, m) == -1) {
			buf_free(&m->raw);
			continue;
		}
		if (m->emsg == EMSG_MULTI) {
			unpack_multi(cm, m->body, m->blen);
			buf_free(&m->raw);
			continue;
		}
		debug("recv emsg %u eresult %d len %zu", m->emsg, m->eresult,
		    m->blen);
		note_message(cm, m);
		if (m->emsg == EMSG_CLIENT_LOGGED_OFF) {
			uint64_t e = 0;
			pb_get_u64(m->body, m->blen, 1, &e);
			warnx_("Steam logged this session off: %s (%d)",
			    eresult_str((int)e), (int)e);
			cm->logged_on = 0;
		}
		return 0;
	}
}

void
cm_msg_free(struct cm_msg *m)
{
	buf_free(&m->raw);
	free(m->error_message);
	m->error_message = NULL;
}

int
cm_call(struct cm *cm, uint32_t emsg, const struct buf *body, uint32_t want,
    struct cm_msg *out)
{
	uint64_t job = cm->next_job++;

	if (cm_send(cm, emsg, body, job, NULL) == -1)
		return -1;
	for (;;) {
		if (cm_recv(cm, out, 60) == -1)
			return -1;
		if (out->emsg == want &&
		    (out->jobid_target == job || out->jobid_target == 0 ||
		    out->jobid_target == ~0ull))
			return 0;
		cm_msg_free(out);
	}
}

int
cm_service(struct cm *cm, const char *method, const struct buf *req,
    struct cm_msg *out)
{
	uint64_t job = cm->next_job++;
	uint32_t emsg = cm->logged_on ? EMSG_SERVICE_METHOD_CALL :
	    EMSG_SERVICE_METHOD_CALL_NONAUTHED;

	if (cm_send(cm, emsg, req, job, method) == -1)
		return -1;
	for (;;) {
		if (cm_recv(cm, out, 60) == -1)
			return -1;
		if (out->emsg == EMSG_SERVICE_METHOD_RESPONSE &&
		    out->jobid_target == job)
			return 0;
		cm_msg_free(out);
	}
}

/*
 * The Steam client sends a binary KeyValues "MessageObject" with three
 * hashes identifying the machine.  Ours are random, made once per
 * installation and kept in $DATA/machine_id.
 */
static void
machine_id(struct buf *out)
{
	static const char *keys[] = { "BB3", "FF2", "3B3" };
	char *path = xasprintf("%s/machine_id", data_dir());
	struct buf seed = { 0 };
	int i;

	if (read_file(path, &seed) == -1 || seed.len < 32) {
		uint8_t r[32];
		buf_free(&seed);
		RAND_bytes(r, sizeof(r));
		buf_put(&seed, r, sizeof(r));
		write_file_atomic(path, r, sizeof(r), 0600);
	}
	buf_putc(out, 0);
	buf_puts(out, "MessageObject");
	buf_putc(out, 0);
	for (i = 0; i < 3; i++) {
		uint8_t h[SHA_DIGEST_LENGTH], in[33];
		char hex[2 * SHA_DIGEST_LENGTH + 1];
		memcpy(in, seed.p, 32);
		in[32] = i;
		SHA1(in, sizeof(in), h);
		hex_encode(h, sizeof(h), hex);
		buf_putc(out, 1);
		buf_puts(out, keys[i]);
		buf_putc(out, 0);
		buf_puts(out, hex);
		buf_putc(out, 0);
	}
	buf_putc(out, 8);
	buf_putc(out, 8);
	buf_free(&seed);
	free(path);
}

int
cm_logon(struct cm *cm, const char *account, const char *token)
{
	struct buf b = { 0 }, mid = { 0 };
	struct cm_msg m;
	uint64_t v;
	int e;

	cm->steamid = account ? STEAMID_INDIVIDUAL : STEAMID_ANON;
	machine_id(&mid);
	/* CMsgClientLogon (steammessages_clientserver_login.proto) */
	pb_varint(&b, 1, PROTOCOL_VERSION);
	pb_varint(&b, 3, cm->cell_id);
	pb_varint(&b, 5, 1771);			/* client_package_version */
	pb_str(&b, 6, "english");
	pb_varint(&b, 7, EOSTYPE_WIN10);
	if (account) {
		pb_varint(&b, 8, 1);		/* should_remember_password */
		pb_str(&b, 50, account);
		pb_str(&b, 108, token);		/* access_token: refresh token */
	}
	pb_bytes(&b, 30, mid.p, mid.len);	/* machine_id */
	buf_free(&mid);
	if (cm_send(cm, EMSG_CLIENT_LOGON, &b, 0, NULL) == -1) {
		buf_free(&b);
		return 3;
	}
	buf_free(&b);
	for (;;) {
		if (cm_recv(cm, &m, 60) == -1)
			return 3;
		if (m.emsg == EMSG_CLIENT_LOGON_RESPONSE)
			break;
		cm_msg_free(&m);
	}
	e = 2;
	if (pb_get_u64(m.body, m.blen, 1, &v))
		e = (int)v;
	if (pb_get_u64(m.body, m.blen, 3, &v) && v > 0)
		cm->heartbeat = (int)v;
	if (pb_get_u64(m.body, m.blen, 7, &v))
		cm->cell_id = (uint32_t)v;
	cm_msg_free(&m);
	if (e == 1) {
		cm->logged_on = 1;
		debug("logged on as %llu, session %d, heartbeat %d s",
		    (unsigned long long)cm->steamid, cm->session_id,
		    cm->heartbeat);
	}
	return e;
}

int
cm_wait_licenses(struct cm *cm)
{
	struct cm_msg m;
	double end = now_sec() + 30;

	while (!cm->have_licenses) {
		if (now_sec() > end || cm_recv(cm, &m, 30) == -1)
			return -1;
		cm_msg_free(&m);
	}
	return 0;
}

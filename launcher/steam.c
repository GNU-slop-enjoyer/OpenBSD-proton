/*
 * Copyright (c) 2026 The OpenBSD Proton project contributors
 * Distributed under the LGPL 2.1 or later.
 *
 * Message layouts follow the Steam client's protobuf definitions as
 * published by SteamDatabase/Protobufs (steammessages_auth.steamclient,
 * steammessages_clientserver*, steammessages_contentsystem.steamclient).
 */
#include <sys/stat.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "crypto.h"
#include "pb.h"
#include "steam.h"

/* EAuthTokenPlatformType, EAuthSessionGuardType */
#define PLATFORM_STEAMCLIENT	1
#define GUARD_NONE		1
#define GUARD_EMAIL_CODE	2
#define GUARD_DEVICE_CODE	3
#define GUARD_DEVICE_CONFIRM	4
#define GUARD_EMAIL_CONFIRM	5

static char *
prompt(const char *what, int secret)
{
	struct termios t, old;
	char line[512];
	int restore = 0;

	fprintf(stderr, "%s", what);
	fflush(stderr);
	if (secret && isatty(0) && tcgetattr(0, &old) == 0) {
		t = old;
		t.c_lflag &= ~ECHO;
		tcsetattr(0, TCSAFLUSH, &t);
		restore = 1;
	}
	if (fgets(line, sizeof(line), stdin) == NULL)
		line[0] = '\0';
	if (restore) {
		tcsetattr(0, TCSAFLUSH, &old);
		fputc('\n', stderr);
	}
	line[strcspn(line, "\r\n")] = '\0';
	return xstrdup(line);
}

static void
wipe(char *s)
{
	if (s) {
		volatile char *v = s;
		while (*v)
			*v++ = 0;
	}
	free(s);
}

static char *
guard_data_path(const char *account)
{
	return xasprintf("%s/guard-%s", data_dir(), account);
}

static int
service_ok(struct cm_msg *m, const char *what)
{
	if (m->eresult != 1) {
		warnx_("%s: %s (%d)%s%s", what, eresult_str(m->eresult),
		    m->eresult, m->error_message ? ": " : "",
		    m->error_message ? m->error_message : "");
		return 0;
	}
	return 1;
}

int
steam_auth(struct cm *cm, const char *account_in, char **refresh_out,
    char **account_out)
{
	struct buf req = { 0 }, dev = { 0 }, guard = { 0 };
	struct cm_msg m;
	char *account, *password, *mod, *exp, *enc, *gpath;
	uint64_t ts = 0, client_id = 0, steamid = 0, v;
	const uint8_t *request_id = NULL;
	size_t request_id_len = 0;
	struct buf rid = { 0 };
	struct pbr r;
	struct pbf f;
	int types[8], ntypes = 0, i, need_code = 0, code_type = 0;
	double interval = 5;
	char *refresh = NULL;

	account = account_in ? xstrdup(account_in) :
	    prompt("Steam account name: ", 0);
	if (!*account)
		die("no account name");
	password = prompt("Password: ", 1);

	/* Authentication.GetPasswordRSAPublicKey */
	pb_str(&req, 1, account);
	if (cm_service(cm, "Authentication.GetPasswordRSAPublicKey#1", &req,
	    &m) == -1)
		die("no answer from Steam");
	buf_free(&req);
	if (!service_ok(&m, "cannot get the login key"))
		exit(1);
	mod = pb_get_str(m.body, m.blen, 1);
	exp = pb_get_str(m.body, m.blen, 2);
	pb_get_u64(m.body, m.blen, 3, &ts);
	cm_msg_free(&m);
	if (!mod || !exp)
		die("unexpected login key reply");
	enc = rsa_encrypt_b64(mod, exp, password);
	wipe(password);
	free(mod);
	free(exp);
	if (enc == NULL)
		die("RSA encryption failed");

	/* Authentication.BeginAuthSessionViaCredentials */
	pb_str(&dev, 1, "proton-launcher (OpenBSD)");
	pb_varint(&dev, 2, PLATFORM_STEAMCLIENT);
	pb_varint(&dev, 3, 16);			/* os_type: Windows 10 */
	pb_str(&req, 2, account);
	pb_str(&req, 3, enc);
	pb_varint(&req, 4, ts);
	pb_varint(&req, 5, 1);			/* remember_login */
	pb_varint(&req, 6, PLATFORM_STEAMCLIENT);
	pb_varint(&req, 7, 1);			/* persistence: persistent */
	pb_str(&req, 8, "Client");		/* website_id */
	pb_msg(&req, 9, &dev);
	gpath = guard_data_path(account);
	if (read_file(gpath, &guard) == 0 && guard.len)
		pb_str(&req, 10, (char *)guard.p);
	buf_free(&guard);
	pb_varint(&req, 11, 0);			/* language: english */
	pb_varint(&req, 12, 2);			/* qos_level */
	free(enc);
	buf_free(&dev);
	if (cm_service(cm, "Authentication.BeginAuthSessionViaCredentials#1",
	    &req, &m) == -1)
		die("no answer from Steam");
	buf_free(&req);
	if (!service_ok(&m, "login failed")) {
		if (m.eresult == 5)
			fprintf(stderr, "Check the account name and password.\n");
		exit(1);
	}
	pbr_init(&r, m.body, m.blen);
	while (pbr_next(&r, &f) == 1) {
		switch (f.field) {
		case 1: client_id = f.v; break;
		case 2:
			buf_put(&rid, f.data, f.len);
			break;
		case 3: {
			float fl;
			uint32_t u = (uint32_t)f.v;
			memcpy(&fl, &u, 4);
			if (fl > 0.5 && fl < 60)
				interval = fl;
			break;
		}
		case 4:
			if (ntypes < 8 && pb_get_u64(f.data, f.len, 1, &v))
				types[ntypes++] = (int)v;
			break;
		case 5: steamid = f.v; break;
		}
	}
	cm_msg_free(&m);
	request_id = rid.p;
	request_id_len = rid.len;

	for (i = 0; i < ntypes; i++) {
		if (types[i] == GUARD_DEVICE_CODE || types[i] == GUARD_EMAIL_CODE) {
			need_code = 1;
			code_type = types[i];
		}
	}
	for (i = 0; i < ntypes; i++) {
		if (types[i] == GUARD_DEVICE_CONFIRM)
			fprintf(stderr, "You can approve this login in the "
			    "Steam mobile app.\n");
		if (types[i] == GUARD_EMAIL_CONFIRM)
			fprintf(stderr, "Steam sent you an email: open the link "
			    "in it to approve this login.\n");
	}
	if (need_code) {
		char *code = prompt(code_type == GUARD_EMAIL_CODE ?
		    "Steam Guard code from your email (or Enter after approving "
		    "in the app): " :
		    "Steam Guard code from the mobile app (or Enter after "
		    "approving in the app): ", 0);
		if (*code) {
			/* Authentication.UpdateAuthSessionWithSteamGuardCode */
			pb_varint(&req, 1, client_id);
			pb_fixed64(&req, 2, steamid);
			pb_str(&req, 3, code);
			pb_varint(&req, 4, code_type);
			if (cm_service(cm, "Authentication."
			    "UpdateAuthSessionWithSteamGuardCode#1", &req,
			    &m) == -1)
				die("no answer from Steam");
			buf_free(&req);
			if (!service_ok(&m, "Steam Guard code not accepted"))
				exit(1);
			cm_msg_free(&m);
		}
		free(code);
	} else if (ntypes)
		fprintf(stderr, "Waiting for the login to be approved...\n");

	/* Authentication.PollAuthSessionStatus until a token arrives. */
	for (i = 0; i < (int)(300 / interval) && refresh == NULL; i++) {
		pb_varint(&req, 1, client_id);
		pb_bytes(&req, 2, request_id, request_id_len);
		if (cm_service(cm, "Authentication.PollAuthSessionStatus#1",
		    &req, &m) == -1)
			die("no answer from Steam");
		buf_free(&req);
		if (!service_ok(&m, "login status"))
			exit(1);
		if (pb_get_u64(m.body, m.blen, 1, &v) && v)
			client_id = v;
		refresh = pb_get_str(m.body, m.blen, 3);
		if (refresh && !*refresh) {
			free(refresh);
			refresh = NULL;
		}
		if (refresh) {
			char *name = pb_get_str(m.body, m.blen, 6);
			char *gd = pb_get_str(m.body, m.blen, 7);
			if (name && *name) {
				free(account);
				account = name;
			} else
				free(name);
			if (gd && *gd) {
				free(gpath);
				gpath = guard_data_path(account);
				write_file_atomic(gpath, gd, strlen(gd), 0600);
			}
			wipe(gd);
		}
		cm_msg_free(&m);
		if (refresh == NULL)
			usleep((useconds_t)(interval * 1e6));
	}
	free(gpath);
	buf_free(&rid);
	if (refresh == NULL)
		die("the login was not approved in time");
	*refresh_out = refresh;
	*account_out = account;
	return 0;
}

void
steam_session(struct cm *cm, int anon)
{
	struct buf login = { 0 };
	char *path, *nl, *token;
	int e;

	if (cm_connect(cm) == -1)
		die("cannot connect to Steam");
	if (anon) {
		if ((e = cm_logon(cm, NULL, NULL)) != 1)
			die("anonymous logon failed: %s (%d)", eresult_str(e), e);
		return;
	}
	path = xasprintf("%s/login", data_dir());
	if (read_file(path, &login) == -1 || login.len == 0)
		die("not logged in; run: proton-launcher login");
	free(path);
	/* "account\ntoken\n" */
	if ((nl = strchr((char *)login.p, '\n')) == NULL)
		die("bad login file; run: proton-launcher login");
	*nl = '\0';
	token = nl + 1;
	token[strcspn(token, "\r\n")] = '\0';
	e = cm_logon(cm, (char *)login.p, token);
	memset(login.p, 0, login.len);
	buf_free(&login);
	if (e != 1) {
		if (e == 5 || e == 27 || e == 15)
			die("the saved login was rejected (%s); run: "
			    "proton-launcher login", eresult_str(e));
		die("logon failed: %s (%d)", eresult_str(e), e);
	}
}

static int
access_tokens(struct cm *cm, const uint32_t *apps, size_t n, uint64_t *tok)
{
	struct buf req = { 0 };
	struct cm_msg m;
	struct pbr r;
	struct pbf f;
	size_t i;

	for (i = 0; i < n; i++) {
		tok[i] = 0;
		pb_varint(&req, 2, apps[i]);
	}
	if (cm_call(cm, EMSG_CLIENT_PICS_ACCESS_TOKEN_REQUEST, &req,
	    EMSG_CLIENT_PICS_ACCESS_TOKEN_RESPONSE, &m) == -1) {
		buf_free(&req);
		return -1;
	}
	buf_free(&req);
	pbr_init(&r, m.body, m.blen);
	while (pbr_next(&r, &f) == 1) {
		uint64_t a = 0, t = 0;
		if (f.field != 3 || f.wt != PB_LEN)
			continue;
		pb_get_u64(f.data, f.len, 1, &a);
		pb_get_u64(f.data, f.len, 2, &t);
		for (i = 0; i < n; i++)
			if (apps[i] == a)
				tok[i] = t;
	}
	cm_msg_free(&m);
	return 0;
}

/* Send a product info request and collect responses until done. */
static int
pics_request(struct cm *cm, struct buf *req,
    void (*cb)(void *, int isapp, uint32_t id, const uint8_t *, size_t),
    void *arg)
{
	struct cm_msg m;
	uint64_t job = cm->next_job++, pending;
	struct pbr r;
	struct pbf f;

	pb_varint(req, 7, 0);	/* single_response: no */
	if (cm_send(cm, EMSG_CLIENT_PICS_PRODUCT_INFO_REQUEST, req, job,
	    NULL) == -1)
		return -1;
	do {
		do {
			if (cm_recv(cm, &m, 120) == -1)
				return -1;
			if (m.emsg == EMSG_CLIENT_PICS_PRODUCT_INFO_RESPONSE &&
			    (m.jobid_target == job || m.jobid_target == 0 ||
			    m.jobid_target == ~0ull))
				break;
			cm_msg_free(&m);
		} while (1);
		pending = 0;
		pb_get_u64(m.body, m.blen, 6, &pending);
		pbr_init(&r, m.body, m.blen);
		while (pbr_next(&r, &f) == 1) {
			const uint8_t *d;
			size_t len;
			uint64_t id = 0;
			if (f.wt != PB_LEN || (f.field != 1 && f.field != 3))
				continue;
			pb_get_u64(f.data, f.len, 1, &id);
			if (pb_get_bytes(f.data, f.len, 5, &d, &len))
				cb(arg, f.field == 1, (uint32_t)id, d, len);
		}
		cm_msg_free(&m);
	} while (pending);
	return 0;
}

struct collect {
	struct appinfo	*apps;
	size_t		 napps;
	struct pkginfo	*pkgs;
	size_t		 npkgs;
};

static void
collect_cb(void *arg, int isapp, uint32_t id, const uint8_t *d, size_t len)
{
	struct collect *c = arg;

	if (isapp) {
		c->apps = xrealloc(c->apps, (c->napps + 1) * sizeof(*c->apps));
		c->apps[c->napps].appid = id;
		c->apps[c->napps].kv = kv_parse_text((const char *)d, len);
		c->napps++;
	} else {
		/* uint32 packageid, then binary KeyValues */
		c->pkgs = xrealloc(c->pkgs, (c->npkgs + 1) * sizeof(*c->pkgs));
		c->pkgs[c->npkgs].packageid = id;
		c->pkgs[c->npkgs].kv = len > 4 ?
		    kv_parse_binary(d + 4, len - 4) : NULL;
		c->npkgs++;
	}
}

int
steam_app_info(struct cm *cm, const uint32_t *apps, size_t n,
    struct appinfo **out, size_t *nout)
{
	struct collect c = { 0 };
	size_t i, off;

	for (off = 0; off < n; off += 100) {
		size_t k = n - off < 100 ? n - off : 100;
		uint64_t *tok = xcalloc(k, sizeof(*tok));
		struct buf req = { 0 };
		if (access_tokens(cm, apps + off, k, tok) == -1) {
			free(tok);
			return -1;
		}
		for (i = 0; i < k; i++) {
			struct buf a = { 0 };
			pb_varint(&a, 1, apps[off + i]);
			if (tok[i])
				pb_varint(&a, 2, tok[i]);
			pb_msg(&req, 2, &a);
			buf_free(&a);
		}
		free(tok);
		if (pics_request(cm, &req, collect_cb, &c) == -1) {
			buf_free(&req);
			return -1;
		}
		buf_free(&req);
	}
	*out = c.apps;
	*nout = c.napps;
	return 0;
}

int
steam_package_info(struct cm *cm, struct pkginfo **out, size_t *nout)
{
	struct collect c = { 0 };
	size_t i, off;

	for (off = 0; off < cm->nlicenses; off += 100) {
		struct buf req = { 0 };
		for (i = off; i < cm->nlicenses && i < off + 100; i++) {
			struct buf p = { 0 };
			pb_varint(&p, 1, cm->licenses[i].package_id);
			if (cm->licenses[i].access_token)
				pb_varint(&p, 2, cm->licenses[i].access_token);
			pb_msg(&req, 1, &p);
			buf_free(&p);
		}
		if (pics_request(cm, &req, collect_cb, &c) == -1) {
			buf_free(&req);
			return -1;
		}
		buf_free(&req);
	}
	*out = c.pkgs;
	*nout = c.npkgs;
	return 0;
}

int
steam_depot_key(struct cm *cm, uint32_t app, uint32_t depot, uint8_t key[32])
{
	struct buf req = { 0 };
	struct cm_msg m;
	const uint8_t *k;
	size_t klen;
	uint64_t e = 2;
	int ret = -1;

	pb_varint(&req, 1, depot);
	pb_varint(&req, 2, app);
	if (cm_call(cm, EMSG_CLIENT_GET_DEPOT_KEY, &req,
	    EMSG_CLIENT_GET_DEPOT_KEY_RESPONSE, &m) == -1) {
		buf_free(&req);
		return -1;
	}
	buf_free(&req);
	pb_get_u64(m.body, m.blen, 1, &e);
	if (e == 1 && pb_get_bytes(m.body, m.blen, 3, &k, &klen) && klen == 32) {
		memcpy(key, k, 32);
		ret = 0;
	} else
		debug("depot %u key: %s (%d)", depot, eresult_str((int)e),
		    (int)e);
	cm_msg_free(&m);
	return ret;
}

int
steam_manifest_code(struct cm *cm, uint32_t app, uint32_t depot,
    uint64_t manifest, const char *branch, uint64_t *code)
{
	struct buf req = { 0 };
	struct cm_msg m;
	int ret = -1;

	pb_varint(&req, 1, app);
	pb_varint(&req, 2, depot);
	pb_varint(&req, 3, manifest);
	pb_str(&req, 4, branch);
	if (cm_service(cm, "ContentServerDirectory.GetManifestRequestCode#1",
	    &req, &m) == -1) {
		buf_free(&req);
		return -1;
	}
	buf_free(&req);
	if (m.eresult == 1 && pb_get_u64(m.body, m.blen, 1, code))
		ret = 0;
	else
		debug("manifest code for depot %u: %s", depot,
		    eresult_str(m.eresult));
	cm_msg_free(&m);
	return ret;
}

char **
steam_cdn_servers(struct cm *cm)
{
	struct buf req = { 0 };
	struct cm_msg m;
	struct pbr r;
	struct pbf f;
	char **list = NULL;
	size_t n = 0;

	pb_varint(&req, 1, cm->cell_id);
	pb_varint(&req, 2, 20);
	if (cm_service(cm, "ContentServerDirectory.GetServersForSteamPipe#1",
	    &req, &m) == -1) {
		buf_free(&req);
		return NULL;
	}
	buf_free(&req);
	pbr_init(&r, m.body, m.blen);
	while (pbr_next(&r, &f) == 1) {
		char *type, *host, *https;
		uint64_t china = 0;
		if (f.field != 1 || f.wt != PB_LEN)
			continue;
		type = pb_get_str(f.data, f.len, 1);
		host = pb_get_str(f.data, f.len, 9);	/* vhost */
		if (host == NULL || !*host) {
			free(host);
			host = pb_get_str(f.data, f.len, 8);
		}
		https = pb_get_str(f.data, f.len, 12);
		pb_get_u64(f.data, f.len, 7, &china);
		if (host && *host && !china && type &&
		    (strcmp(type, "CDN") == 0 || strcmp(type, "SteamCache") == 0)) {
			list = xrealloc(list, (n + 2) * sizeof(*list));
			list[n++] = xasprintf("%s://%s",
			    https && strcmp(https, "unavailable") == 0 ?
			    "http" : "https", host);
			list[n] = NULL;
		}
		free(type);
		free(host);
		free(https);
	}
	cm_msg_free(&m);
	if (n == 0) {
		list = xcalloc(3, sizeof(*list));
		list[0] = xstrdup("https://cache1-fra1.steamcontent.com");
		list[1] = xstrdup("https://cache2-fra1.steamcontent.com");
	}
	return list;
}

int
steam_free_license(struct cm *cm, uint32_t app)
{
	struct buf req = { 0 };
	struct cm_msg m;
	uint64_t e = 2, e2;
	const uint8_t *d;
	size_t len;
	int granted = 0;

	pb_varint(&req, 2, app);
	if (cm_call(cm, EMSG_CLIENT_REQUEST_FREE_LICENSE, &req,
	    EMSG_CLIENT_REQUEST_FREE_LICENSE_RESPONSE, &m) == -1) {
		buf_free(&req);
		return -1;
	}
	buf_free(&req);
	pb_get_u64(m.body, m.blen, 1, &e);
	/* granted_packageids: packed or not */
	if ((pb_get_bytes(m.body, m.blen, 2, &d, &len) && len > 0) ||
	    pb_get_u64(m.body, m.blen, 2, &e2))
		granted = 1;
	cm_msg_free(&m);
	if (e != 1)
		return -1;
	return granted;
}

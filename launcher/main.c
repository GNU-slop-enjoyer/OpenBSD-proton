/*
 * proton-launcher: log in to Steam, download the Windows games you own,
 * and start them (and your own games) through proton-openbsd(1).
 *
 * Copyright (c) 2026 The OpenBSD Proton project contributors
 * Distributed under the LGPL 2.1 or later.
 */
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "depot.h"
#include "steam.h"

static int anonymous;
static int threads = 8;
static const char *branch = "public";

static void
usage(void)
{
	fprintf(stderr,
	    "usage: proton-launcher [-v] [command]\n"
	    "\n"
	    "Steam:\n"
	    "  login [account]        log in (Steam Guard supported)\n"
	    "  logout                 forget the saved login\n"
	    "  library                list the Windows games you own\n"
	    "  info APPID             show depots and launch options\n"
	    "  install APPID          download or update a game\n"
	    "  uninstall APPID        delete a downloaded game\n"
	    "  free APPID             add a free game to your account\n"
	    "Your own games:\n"
	    "  add NAME PROGRAM.exe [ARG ...]\n"
	    "  remove NAME\n"
	    "Both:\n"
	    "  list                   installed games\n"
	    "  run APPID|NAME [ARG ...]\n"
	    "  menu                   interactive menu (the default)\n"
	    "\n"
	    "options for install/info: -a (anonymous), -b BRANCH, -j THREADS\n");
	exit(2);
}

static char *
path_in(const char *fmt, const char *s)
{
	return xasprintf("%s/%s/%s", data_dir(), fmt, s);
}

static const char *
games_dir(void)
{
	static char *d;
	const char *s;

	if (d)
		return d;
	if ((s = config_get("library_dir")))
		d = xstrdup(s);
	else
		d = xasprintf("%s/steamapps/common", data_dir());
	mkdir_p(d, 0755);
	return d;
}

/* ---- saved Steam games: $DATA/steamapps/APPID.info ---- */

struct game {
	char	*id;		/* appid or own-game name */
	char	*name;
	char	*dir;		/* install dir (Steam) */
	char	*exe;		/* absolute path */
	char	*args;
	char	*workdir;
	char	*dlc;		/* owned DLC, "APPID=NAME;..." */
	char	*buildid;
	int	 steam;
};

static void
game_free(struct game *g)
{
	free(g->id);
	free(g->name);
	free(g->dir);
	free(g->exe);
	free(g->args);
	free(g->workdir);
	free(g->dlc);
	free(g->buildid);
	memset(g, 0, sizeof(*g));
}

static char *
field(const char *text, const char *key)
{
	size_t n = strlen(key);
	const char *p;

	for (p = text; p && *p; p = strchr(p, '\n') ? strchr(p, '\n') + 1 : NULL)
		if (strncmp(p, key, n) == 0 && p[n] == '=') {
			const char *e = strchr(p, '\n');
			size_t k = e ? (size_t)(e - p - n - 1) : strlen(p + n + 1);
			char *v = xmalloc(k + 1);
			memcpy(v, p + n + 1, k);
			v[k] = '\0';
			return v;
		}
	return NULL;
}

static int
load_steam_game(const char *appid, struct game *g)
{
	struct buf b = { 0 };
	char *dir = xasprintf("%s/steamapps", data_dir());
	char *path = xasprintf("%s/%s.info", dir, appid);

	free(dir);
	memset(g, 0, sizeof(*g));
	if (read_file(path, &b) == -1) {
		free(path);
		return -1;
	}
	free(path);
	g->steam = 1;
	g->id = xstrdup(appid);
	g->name = field((char *)b.p, "name");
	g->dir = field((char *)b.p, "dir");
	g->exe = field((char *)b.p, "exe");
	g->args = field((char *)b.p, "args");
	g->workdir = field((char *)b.p, "workdir");
	g->dlc = field((char *)b.p, "dlc");
	g->buildid = field((char *)b.p, "buildid");
	buf_free(&b);
	if (g->name == NULL)
		g->name = xstrdup(appid);
	return 0;
}

static int
load_own_game(const char *name, struct game *g)
{
	struct buf b = { 0 };
	char *path = path_in("games", name);

	memset(g, 0, sizeof(*g));
	if (read_file(path, &b) == -1) {
		free(path);
		return -1;
	}
	free(path);
	g->id = xstrdup(name);
	g->name = xstrdup(name);
	g->exe = field((char *)b.p, "exe");
	g->args = field((char *)b.p, "args");
	g->workdir = field((char *)b.p, "workdir");
	buf_free(&b);
	return g->exe ? 0 : -1;
}

/* All installed games, Steam first, sorted by name. */
static int
game_cmp(const void *a, const void *b)
{
	const struct game *x = a, *y = b;

	if (x->steam != y->steam)
		return y->steam - x->steam;
	return strcasecmp(x->name, y->name);
}

static struct game *
all_games(size_t *n)
{
	struct game *list = NULL;
	const char *sub[] = { "steamapps", "games" };
	int s;

	*n = 0;
	for (s = 0; s < 2; s++) {
		char *dir = xasprintf("%s/%s", data_dir(), sub[s]);
		DIR *d = opendir(dir);
		struct dirent *e;
		free(dir);
		if (d == NULL)
			continue;
		while ((e = readdir(d))) {
			struct game g;
			char *id;
			if (e->d_name[0] == '.')
				continue;
			if (s == 0) {
				char *dot = strstr(e->d_name, ".info");
				if (dot == NULL || dot[5])
					continue;
				id = xstrdup(e->d_name);
				id[dot - e->d_name] = '\0';
				if (load_steam_game(id, &g) == -1) {
					free(id);
					continue;
				}
				free(id);
			} else if (load_own_game(e->d_name, &g) == -1)
				continue;
			list = xrealloc(list, (*n + 1) * sizeof(*list));
			list[(*n)++] = g;
		}
		closedir(d);
	}
	if (*n)
		qsort(list, *n, sizeof(*list), game_cmp);
	return list;
}

/* ---- Steam commands ---- */

static void owned_ids(struct cm *, uint32_t **, size_t *, uint32_t **,
    size_t *);

static void
cmd_login(const char *account)
{
	struct cm cm;
	char *token, *name, *path;
	struct buf b = { 0 };
	int e;

	if (cm_connect(&cm) == -1)
		die("cannot connect to Steam");
	steam_auth(&cm, account, &token, &name);
	/* Prove the token works for the client before saving it. */
	if ((e = cm_logon(&cm, name, token)) != 1)
		die("Steam accepted the password but refused the logon: "
		    "%s (%d)", eresult_str(e), e);
	cm_wait_licenses(&cm);
	{
		uint32_t *apps, *depots;
		size_t napps, ndepots;
		owned_ids(&cm, &apps, &napps, &depots, &ndepots);
		free(apps);
		free(depots);
	}
	buf_printf(&b, "%s\n%s\n", name, token);
	path = xasprintf("%s/login", data_dir());
	if (write_file_atomic(path, b.p, b.len, 0600) == -1)
		die("cannot write %s", path);
	memset(b.p, 0, b.len);
	buf_free(&b);
	memset(token, 0, strlen(token));
	free(token);
	printf("Logged in as %s (%zu licenses). The login is saved in %s "
	    "(readable only by you); \"proton-launcher logout\" removes it.\n",
	    name, cm.nlicenses, path);
	free(path);
	free(name);
	cm_close(&cm);
}

static void
cmd_logout(void)
{
	char *path = xasprintf("%s/login", data_dir());

	if (unlink(path) == -1 && errno != ENOENT)
		die("%s: %s", path, strerror(errno));
	printf("Logged out.\n");
	free(path);
}

static int
oslist_has_windows(struct kv *k, const char *path)
{
	const char *s = kv_str(k, path);

	return s == NULL || *s == '\0' || strstr(s, "windows") != NULL;
}

/*
 * $DATA/account: what proton-openbsd's steamclient64.dll tells games about
 * the account (see cmd_run): SteamID, persona name and owned app ids.
 */
static void
save_account(struct cm *cm, const uint32_t *apps, size_t napps)
{
	struct buf b = { 0 };
	char *path;
	size_t i;

	if (cm->steamid == 0 || (cm->steamid >> 52) != 0x011)
		return;		/* anonymous */
	buf_printf(&b, "steamid=%llu\n", (unsigned long long)cm->steamid);
	if (cm->persona && !strchr(cm->persona, '\n'))
		buf_printf(&b, "name=%s\n", cm->persona);
	buf_printf(&b, "apps=");
	for (i = 0; i < napps; i++)
		buf_printf(&b, "%s%u", i ? "," : "", apps[i]);
	buf_printf(&b, "\n");
	path = xasprintf("%s/account", data_dir());
	if (write_file_atomic(path, b.p, b.len, 0600) == -1)
		warnx_("cannot write %s", path);
	free(path);
	buf_free(&b);
}

/* Owned app and depot ids from the account's packages. */
static void
owned_ids(struct cm *cm, uint32_t **apps, size_t *napps, uint32_t **depots,
    size_t *ndepots)
{
	struct pkginfo *pk;
	size_t np, i;

	*apps = *depots = NULL;
	*napps = *ndepots = 0;
	if (cm_wait_licenses(cm) == -1)
		die("Steam did not send the license list");
	if (steam_package_info(cm, &pk, &np) == -1)
		die("cannot get package info");
	for (i = 0; i < np; i++) {
		struct kv *root = pk[i].kv, *n;
		if (root == NULL)
			continue;
		for (n = kv_get(root->child, "appids"); n && n->child; n = NULL) {
			struct kv *c;
			for (c = n->child; c; c = c->next) {
				uint32_t id = strtoul(c->val ? c->val : "0", NULL, 10);
				size_t k;
				for (k = 0; k < *napps && (*apps)[k] != id; k++)
					;
				if (k == *napps && id) {
					*apps = xrealloc(*apps, (*napps + 1) * 4);
					(*apps)[(*napps)++] = id;
				}
			}
		}
		for (n = kv_get(root->child, "depotids"); n && n->child; n = NULL) {
			struct kv *c;
			for (c = n->child; c; c = c->next) {
				*depots = xrealloc(*depots, (*ndepots + 1) * 4);
				(*depots)[(*ndepots)++] =
				    strtoul(c->val ? c->val : "0", NULL, 10);
			}
		}
		kv_free(root);
	}
	free(pk);
	save_account(cm, *apps, *napps);
}

static int
appinfo_cmp(const void *a, const void *b)
{
	const struct appinfo *x = a, *y = b;
	const char *nx = kv_str(x->kv, "appinfo/common/name");
	const char *ny = kv_str(y->kv, "appinfo/common/name");

	return strcasecmp(nx ? nx : "", ny ? ny : "");
}

static void
cmd_library(void)
{
	struct cm cm;
	uint32_t *apps, *depots;
	size_t napps, ndepots, ninfo, i, shown = 0;
	struct appinfo *info;
	struct buf cache = { 0 };
	char *path;

	steam_session(&cm, 0);
	owned_ids(&cm, &apps, &napps, &depots, &ndepots);
	if (steam_app_info(&cm, apps, napps, &info, &ninfo) == -1)
		die("cannot get app info");
	qsort(info, ninfo, sizeof(*info), appinfo_cmp);
	for (i = 0; i < ninfo; i++) {
		const char *type = kv_str(info[i].kv, "appinfo/common/type");
		const char *name = kv_str(info[i].kv, "appinfo/common/name");
		if (type == NULL || strcasecmp(type, "game") != 0 || !name)
			continue;
		if (!oslist_has_windows(info[i].kv, "appinfo/common/oslist"))
			continue;
		printf("%8u  %s\n", info[i].appid, name);
		buf_printf(&cache, "%u\t%s\n", info[i].appid, name);
		shown++;
	}
	printf("%zu Windows games. Install one with: proton-launcher install "
	    "APPID\n", shown);
	path = xasprintf("%s/library-cache", data_dir());
	write_file_atomic(path, cache.p ? cache.p : (uint8_t *)"", cache.len,
	    0600);
	free(path);
	buf_free(&cache);
	for (i = 0; i < ninfo; i++)
		kv_free(info[i].kv);
	free(info);
	free(apps);
	free(depots);
	cm_close(&cm);
}

static struct kv *
fetch_app(struct cm *cm, uint32_t appid)
{
	struct appinfo *info;
	size_t n;
	struct kv *k;

	if (steam_app_info(cm, &appid, 1, &info, &n) == -1 || n == 0)
		die("no app info for %u (unknown app, or no access)", appid);
	k = info[0].kv;
	free(info);
	return k;
}

static uint64_t
depot_manifest(struct kv *depot)
{
	char path[128];
	const char *s;

	snprintf(path, sizeof(path), "manifests/%s/gid", branch);
	if ((s = kv_str(depot->child, path)) == NULL) {
		snprintf(path, sizeof(path), "manifests/%s", branch);
		s = kv_str(depot->child, path);
	}
	return s ? strtoull(s, NULL, 10) : 0;
}

/* Why a depot is not installed, or NULL to install it. */
static const char *
depot_skip(struct kv *d, const uint32_t *owned, size_t nowned)
{
	const char *s;
	uint32_t id = strtoul(d->key, NULL, 10);
	size_t i;

	if (!oslist_has_windows(d->child, "config/oslist"))
		return "not for Windows";
	if ((s = kv_str(d->child, "config/osarch")) && strcmp(s, "32") == 0)
		return "32-bit only";
	if ((s = kv_str(d->child, "config/language")) && *s &&
	    strcasecmp(s, "english") != 0)
		return "other language";
	if ((s = kv_str(d->child, "config/lowviolence")) && strcmp(s, "1") == 0)
		return "low-violence version";
	if (kv_str(d->child, "depotfromapp") ||
	    ((s = kv_str(d->child, "sharedinstall")) && strcmp(s, "1") == 0))
		return "shared redistributable";
	if (owned) {
		for (i = 0; i < nowned && owned[i] != id; i++)
			;
		if (i == nowned)
			return "not owned";
	}
	if (depot_manifest(d) == 0)
		return "no manifest for this branch";
	return NULL;
}

struct launch {
	const char	*exe, *args, *workdir;
};

static int
pick_launch(struct kv *app, struct launch *l)
{
	struct kv *n = kv_get(app, "appinfo/config/launch"), *c;
	int pass;

	memset(l, 0, sizeof(*l));
	if (n == NULL)
		return -1;
	/* Prefer a Windows, 64-bit or unspecified entry without a beta key. */
	for (pass = 0; pass < 2; pass++)
		for (c = n->child; c; c = c->next) {
			const char *exe = kv_str(c->child, "executable");
			const char *arch = kv_str(c->child, "config/osarch");
			const char *type = kv_str(c->child, "type");
			if (exe == NULL || strstr(exe, "://") ||
			    !oslist_has_windows(c->child,
			    "config/oslist") || kv_str(c->child, "config/betakey"))
				continue;
			if (pass == 0 && ((arch && strcmp(arch, "64")) ||
			    (type && strcasecmp(type, "default") &&
			    strcasecmp(type, "none"))))
				continue;
			l->exe = exe;
			l->args = kv_str(c->child, "arguments");
			l->workdir = kv_str(c->child, "workingdir");
			return 0;
		}
	return -1;
}

static char *
unix_path(const char *base, const char *rel)
{
	char *p = xasprintf("%s/%s", base, rel ? rel : ""), *s;

	for (s = p; *s; s++)
		if (*s == '\\')
			*s = '/';
	return p;
}

static void
cmd_info(uint32_t appid)
{
	struct cm cm;
	struct kv *app, *depots, *d;
	struct launch l;
	uint32_t *apps = NULL, *owned = NULL;
	size_t napps = 0, nowned = 0;
	const char *s;

	steam_session(&cm, anonymous);
	if (!anonymous)
		owned_ids(&cm, &apps, &napps, &owned, &nowned);
	app = fetch_app(&cm, appid);
	printf("%u  %s\n", appid, (s = kv_str(app, "appinfo/common/name")) ?
	    s : "?");
	printf("  type %s", (s = kv_str(app, "appinfo/common/type")) ? s : "?");
	printf(", OS %s", (s = kv_str(app, "appinfo/common/oslist")) ? s : "any");
	printf(", install dir %s\n",
	    (s = kv_str(app, "appinfo/config/installdir")) ? s : "?");
	if ((depots = kv_get(app, "appinfo/depots")))
		for (d = depots->child; d; d = d->next) {
			const char *why;
			if (!isdigit((unsigned char)d->key[0]))
				continue;
			why = depot_skip(d, anonymous ? NULL : owned, nowned);
			printf("  depot %-8s %-28.28s %s\n", d->key,
			    (s = kv_str(d->child, "name")) ? s : "",
			    why ? why : "install");
		}
	if (pick_launch(app, &l) == 0)
		printf("  runs %s %s\n", l.exe, l.args ? l.args : "");
	else
		printf("  no Windows launch option\n");
	kv_free(app);
	free(apps);
	free(owned);
	cm_close(&cm);
}

/* Download one depot of app srcapp into dir: 0 done, -1 failed, 1 skipped. */
static int
install_depot(struct cm *cm, char **servers, uint32_t srcapp, struct kv *d,
    const char *dir)
{
	uint32_t depot = strtoul(d->key, NULL, 10);
	uint64_t gid = depot_manifest(d), code = 0;
	uint8_t key[32];
	struct manifest m;
	int r;

	printf("depot %u, manifest %llu\n", depot, (unsigned long long)gid);
	if (steam_depot_key(cm, srcapp, depot, key) == -1) {
		warnx_("no key for depot %u (not owned?); skipped", depot);
		return 1;
	}
	if (steam_manifest_code(cm, srcapp, depot, gid, branch, &code) == -1)
		code = 0;
	if (manifest_fetch(servers, depot, gid, code, key, &m) == -1)
		return -1;
	r = depot_download(servers, &m, key, dir, threads);
	manifest_free(&m);
	memset(key, 0, sizeof(key));
	return r;
}

/*
 * "dlc=APPID=NAME;..." for the game's DLC (appinfo/extended/listofdlc)
 * that the account owns; steamclient64.dll lists them to the game.
 */
static void
owned_dlc(struct cm *cm, struct kv *app, const uint32_t *apps, size_t napps,
    struct buf *info)
{
	const char *list = kv_str(app, "appinfo/extended/listofdlc");
	uint32_t *ids = NULL;
	size_t nids = 0, ninfo = 0, i, k;
	struct appinfo *ai = NULL;
	char *end;

	while (list && *list) {
		uint32_t id = strtoul(list, &end, 10);
		if (end == list)
			break;
		for (k = 0; k < napps && apps[k] != id; k++)
			;
		if (k < napps) {
			ids = xrealloc(ids, (nids + 1) * sizeof(*ids));
			ids[nids++] = id;
		}
		list = *end ? end + 1 : end;
	}
	if (nids == 0)
		return;
	if (steam_app_info(cm, ids, nids, &ai, &ninfo) == -1)
		ninfo = 0;
	buf_printf(info, "dlc=");
	for (i = 0; i < nids; i++) {
		const char *name = "";
		for (k = 0; k < ninfo; k++)
			if (ai[k].appid == ids[i]) {
				const char *n = kv_str(ai[k].kv,
				    "appinfo/common/name");
				if (n && !strpbrk(n, ";=\n"))
					name = n;
			}
		buf_printf(info, "%s%u=%s", i ? ";" : "", ids[i], name);
	}
	buf_printf(info, "\n");
	for (k = 0; k < ninfo; k++)
		kv_free(ai[k].kv);
	free(ai);
	free(ids);
}

static void
cmd_install(uint32_t appid)
{
	struct cm cm;
	struct kv *app, *depots, *d;
	struct launch l;
	uint32_t *apps = NULL, *owned = NULL;
	size_t napps = 0, nowned = 0;
	const char *name, *idir, *type, *s;
	char *dir, **servers, *infopath, *infodir;
	struct buf info = { 0 };
	int ndepots = 0, failed = 0, only32 = 0, is_game;

	steam_session(&cm, anonymous);
	if (!anonymous)
		owned_ids(&cm, &apps, &napps, &owned, &nowned);
	app = fetch_app(&cm, appid);
	name = kv_str(app, "appinfo/common/name");
	idir = kv_str(app, "appinfo/config/installdir");
	type = kv_str(app, "appinfo/common/type");
	is_game = type && strcasecmp(type, "game") == 0;
	if (name == NULL)
		name = "unknown";
	if (idir == NULL || !*idir || strchr(idir, '/') || strcmp(idir, "..") == 0)
		idir = NULL;
	dir = idir ? xasprintf("%s/%s", games_dir(), idir) :
	    xasprintf("%s/%u", games_dir(), appid);
	if (mkdir_p(dir, 0755) == -1)
		die("cannot create %s", dir);
	printf("Installing %s (%u) into %s\n", name, appid, dir);
	if ((servers = steam_cdn_servers(&cm)) == NULL)
		die("cannot get the content server list");
	if ((depots = kv_get(app, "appinfo/depots")) == NULL)
		die("%s has no depots", name);
	for (d = depots->child; d; d = d->next) {
		uint32_t depot;
		const char *why, *from;

		if (!isdigit((unsigned char)d->key[0]) || d->child == NULL)
			continue;
		depot = strtoul(d->key, NULL, 10);
		from = kv_str(d->child, "depotfromapp");
		if (from && strcmp(from, "1007") == 0 && !is_game &&
		    oslist_has_windows(d->child, "config/oslist")) {
			/*
			 * Tools and dedicated servers load steamclient64.dll
			 * from their own folder; SteamCMD installs the
			 * Steamworks SDK Redist (app 1007) depot next to them.
			 */
			struct kv *sdk = fetch_app(&cm, 1007), *sd;
			char key[64];
			snprintf(key, sizeof(key), "appinfo/depots/%u", depot);
			if ((sd = kv_get(sdk, key)) && depot_manifest(sd)) {
				if (install_depot(&cm, servers, 1007, sd, dir) == 0)
					ndepots++;
				else
					failed++;
			}
			kv_free(sdk);
			continue;
		}
		if ((why = depot_skip(d, anonymous ? NULL : owned, nowned))) {
			if (strcmp(why, "32-bit only") == 0)
				only32++;
			debug("skip depot %u: %s", depot, why);
			continue;
		}
		switch (install_depot(&cm, servers, appid, d, dir)) {
		case 0: ndepots++; break;
		case -1: failed++; break;
		}
	}
	if (ndepots == 0 && !failed) {
		if (only32)
			die("%s only has 32-bit Windows depots; OpenBSD cannot "
			    "run 32-bit Windows programs", name);
		die("nothing to download for %s (see: proton-launcher info %u)",
		    name, appid);
	}
	if (failed)
		die("%d depot(s) failed; run the same command again to resume",
		    failed);

	infodir = xasprintf("%s/steamapps", data_dir());
	mkdir_p(infodir, 0755);
	buf_printf(&info, "name=%s\ndir=%s\n", name, dir);
	if ((s = kv_str(app, "appinfo/depots/branches/public/buildid")))
		buf_printf(&info, "buildid=%s\n", s);
	if (!anonymous)
		owned_dlc(&cm, app, apps, napps, &info);
	if (pick_launch(app, &l) == 0) {
		char *exe = unix_path(dir, l.exe), *sl, *ids;
		buf_printf(&info, "exe=%s\n", exe);
		/*
		 * steam_appid.txt next to the program is the Steamworks SDK's
		 * way to start a game without the Steam client relaunching
		 * it (SteamAPI_RestartAppIfNecessary); games whose Steam
		 * features are optional then start.
		 */
		if ((sl = strrchr(exe, '/'))) {
			struct stat st;
			*sl = '\0';
			ids = xasprintf("%s/steam_appid.txt", exe);
			if (stat(ids, &st) == -1) {
				char num[16];
				snprintf(num, sizeof(num), "%u", appid);
				write_file_atomic(ids, num, strlen(num), 0644);
			}
			free(ids);
		}
		free(exe);
		if (l.args)
			buf_printf(&info, "args=%s\n", l.args);
		if (l.workdir && *l.workdir) {
			char *wd = unix_path(dir, l.workdir);
			buf_printf(&info, "workdir=%s\n", wd);
			free(wd);
		}
	}
	infopath = xasprintf("%s/%u.info", infodir, appid);
	if (write_file_atomic(infopath, info.p, info.len, 0644) == -1)
		die("cannot write %s", infopath);
	printf("Done. Start it with: proton-launcher run %u\n", appid);
	if (pick_launch(app, &l) == -1)
		printf("Steam lists no Windows launch option; set one with "
		    "\"exe=...\" in %s\n", infopath);
	free(infopath);
	free(infodir);
	buf_free(&info);
	free(dir);
	kv_free(app);
	free(apps);
	free(owned);
	cm_close(&cm);
}

static int
rm_rf(const char *path)
{
	struct stat st;
	DIR *d;
	struct dirent *e;

	if (lstat(path, &st) == -1)
		return errno == ENOENT ? 0 : -1;
	if (!S_ISDIR(st.st_mode))
		return unlink(path);
	if ((d = opendir(path)) == NULL)
		return -1;
	while ((e = readdir(d))) {
		char *sub;
		if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0)
			continue;
		sub = xasprintf("%s/%s", path, e->d_name);
		rm_rf(sub);
		free(sub);
	}
	closedir(d);
	return rmdir(path);
}

static void
cmd_uninstall(const char *appid)
{
	struct game g;
	char *info;

	if (load_steam_game(appid, &g) == -1)
		die("%s is not installed", appid);
	if (g.dir && strncmp(g.dir, games_dir(), strlen(games_dir())) == 0 &&
	    g.dir[strlen(games_dir())] == '/') {
		printf("Deleting %s\n", g.dir);
		rm_rf(g.dir);
	}
	info = xasprintf("%s/steamapps/%s.info", data_dir(), appid);
	unlink(info);
	free(info);
	printf("Removed %s. Its wine prefix stays in %s/prefixes/%s\n",
	    g.name, data_dir(), appid);
	game_free(&g);
}

static void
cmd_free(uint32_t appid)
{
	struct cm cm;
	int r;

	steam_session(&cm, 0);
	r = steam_free_license(&cm, appid);
	if (r == 1)
		printf("Added %u to your account.\n", appid);
	else if (r == 0)
		printf("Steam granted nothing: %u is not free, or you already "
		    "own it.\n", appid);
	else
		die("the request failed");
	cm_close(&cm);
}

/* ---- own games ---- */

static int
valid_name(const char *s)
{
	if (!*s || strlen(s) > 64 || isdigit((unsigned char)*s))
		return 0;
	for (; *s; s++)
		if (!isalnum((unsigned char)*s) && !strchr("-_. ", *s))
			return 0;
	return 1;
}

static void
cmd_add(const char *name, const char *exe, int argc, char **argv)
{
	char real[PATH_MAX], *dir, *path, *wd, *slash;
	struct buf b = { 0 };
	int i;

	if (!valid_name(name))
		die("name must be letters, digits, \"-_. \" and not start "
		    "with a digit");
	if (realpath(exe, real) == NULL)
		die("%s: %s", exe, strerror(errno));
	dir = xasprintf("%s/games", data_dir());
	mkdir_p(dir, 0755);
	buf_printf(&b, "exe=%s\n", real);
	if (argc > 0) {
		buf_puts(&b, "args=");
		for (i = 0; i < argc; i++)
			buf_printf(&b, "%s%s", i ? " " : "", argv[i]);
		buf_putc(&b, '\n');
	}
	wd = xstrdup(real);
	if ((slash = strrchr(wd, '/')))
		*slash = '\0';
	buf_printf(&b, "workdir=%s\n", wd);
	free(wd);
	path = xasprintf("%s/%s", dir, name);
	if (write_file_atomic(path, b.p, b.len, 0644) == -1)
		die("cannot write %s", path);
	printf("Added \"%s\". Start it with: proton-launcher run \"%s\"\n",
	    name, name);
	free(path);
	free(dir);
	buf_free(&b);
}

static void
cmd_remove(const char *name)
{
	char *path = path_in("games", name);

	if (!valid_name(name) || unlink(path) == -1)
		die("no game called \"%s\"", name);
	printf("Removed \"%s\" from the list (its files are untouched).\n",
	    name);
	free(path);
}

static void
cmd_list(void)
{
	struct game *g;
	size_t n, i;

	g = all_games(&n);
	if (n == 0)
		printf("No games yet. Install one from Steam "
		    "(proton-launcher library) or add your own "
		    "(proton-launcher add NAME PROGRAM.exe).\n");
	for (i = 0; i < n; i++) {
		printf("%-10s %-40s %s\n", g[i].steam ? g[i].id : "own",
		    g[i].name, g[i].exe ? g[i].exe : "(no program set)");
		game_free(&g[i]);
	}
	free(g);
}

/* ---- running ---- */

static void
split_args(const char *s, char ***argv, int *argc)
{
	struct buf w = { 0 };
	int inq = 0, have = 0;

	for (;; s++) {
		if (*s == '\0' || (!inq && (*s == ' ' || *s == '\t'))) {
			if (have) {
				*argv = xrealloc(*argv, (*argc + 2) * sizeof(char *));
				(*argv)[(*argc)++] = xstrdup(buf_str(&w));
				(*argv)[*argc] = NULL;
				w.len = 0;
				have = 0;
			}
			if (*s == '\0')
				break;
		} else if (*s == '"')
			inq = !inq, have = 1;
		else {
			buf_putc(&w, *s);
			have = 1;
		}
	}
	buf_free(&w);
}

/*
 * The account and game for proton-openbsd's steamclient64.dll, which
 * games' Steamworks API talks to in place of Valve's client.
 */
static void
steam_env(struct game *g)
{
	struct buf b = { 0 };
	char *path, *v, *ud;
	uint64_t steamid = 0;
	const char *lang;

	path = xasprintf("%s/account", data_dir());
	if (read_file(path, &b) == 0) {
		if ((v = field((char *)b.p, "steamid"))) {
			steamid = strtoull(v, NULL, 10);
			setenv("PROTON_STEAM_ID", v, 1);
			free(v);
		}
		if ((v = field((char *)b.p, "name"))) {
			setenv("PROTON_STEAM_NAME", v, 1);
			free(v);
		}
		if ((v = field((char *)b.p, "apps"))) {
			setenv("PROTON_STEAM_APPS", v, 1);
			free(v);
		}
		buf_free(&b);
	}
	free(path);
	if (g->dlc)
		setenv("PROTON_STEAM_DLC", g->dlc, 1);
	if (g->buildid)
		setenv("PROTON_STEAM_BUILDID", g->buildid, 1);
	if (g->dir)
		setenv("PROTON_STEAM_GAMEDIR", g->dir, 1);
	if ((lang = config_get("language")))
		setenv("PROTON_STEAM_LANGUAGE", lang, 1);
	/* stats, achievements and cloud files, outside the prefix */
	ud = xasprintf("%s/userdata/%u/%s", data_dir(),
	    (unsigned)(steamid & 0xffffffff), g->id);
	mkdir_p(ud, 0700);
	setenv("PROTON_STEAM_USERDATA", ud, 1);
	free(ud);
}

static void
cmd_run(const char *id, int argc, char **argv)
{
	struct game g;
	const char *proton, *shared;
	char **av = NULL, *pfx, *pfxname;
	int ac = 0, i;
	size_t n;

	if (load_steam_game(id, &g) == -1 && load_own_game(id, &g) == -1)
		die("no installed game \"%s\" (see: proton-launcher list)", id);
	if (g.exe == NULL)
		die("no program set for %s", g.name);
	if (access(g.exe, R_OK) == -1)
		die("%s: %s", g.exe, strerror(errno));
	proton = config_get("proton");
	if (proton == NULL)
		proton = "proton-openbsd";
	av = xcalloc(5, sizeof(char *));
	av[ac++] = xstrdup(proton);
	n = strlen(g.exe);
	if (n > 4 && (strcasecmp(g.exe + n - 4, ".bat") == 0 ||
	    strcasecmp(g.exe + n - 4, ".cmd") == 0)) {
		/* Batch files run in wine's cmd, from their own directory. */
		const char *base = strrchr(g.exe, '/');
		av[ac++] = xstrdup("cmd");
		av[ac++] = xstrdup("/c");
		av[ac++] = xstrdup(base ? base + 1 : g.exe);
		if (!g.workdir) {
			g.workdir = xstrdup(g.exe);
			*strrchr(g.workdir, '/') = '\0';
		}
	} else {
		av[ac++] = xstrdup("run");
		av[ac++] = xstrdup(g.exe);
	}
	if (g.args)
		split_args(g.args, &av, &ac);
	for (i = 0; i < argc; i++) {
		av = xrealloc(av, (ac + 2) * sizeof(char *));
		av[ac++] = xstrdup(argv[i]);
	}
	av = xrealloc(av, (ac + 1) * sizeof(char *));
	av[ac] = NULL;

	/* One prefix per game, as Proton keeps one per app. */
	shared = config_get("shared_prefix");
	pfxname = shared && strcmp(shared, "yes") == 0 ? xstrdup("shared") :
	    xstrdup(g.id);
	for (i = 0; pfxname[i]; i++)
		if (pfxname[i] == ' ' || pfxname[i] == '/')
			pfxname[i] = '_';
	pfx = xasprintf("%s/prefixes/%s", data_dir(), pfxname);
	mkdir_p(pfx, 0755);
	if (getenv("PROTON_PREFIX") == NULL)
		setenv("PROTON_PREFIX", pfx, 1);
	if (g.steam) {
		setenv("SteamAppId", g.id, 1);
		setenv("SteamGameId", g.id, 1);
		steam_env(&g);
	}
	if (g.workdir && chdir(g.workdir) == -1)
		warnx_("cannot change to %s: %s", g.workdir, strerror(errno));
	else if (!g.workdir) {
		char *d = xstrdup(g.exe), *s = strrchr(d, '/');
		if (s) {
			*s = '\0';
			if (chdir(d) == -1)
				warnx_("cannot change to %s", d);
		}
		free(d);
	}
	printf("Starting %s (prefix %s)\n", g.name, getenv("PROTON_PREFIX"));
	fflush(stdout);
	execvp(av[0], av);
	die("cannot run %s: %s (is the proton-openbsd package installed?)",
	    av[0], strerror(errno));
}

/* ---- interactive menu ---- */

static char *
ask(const char *q)
{
	char line[1024];

	printf("%s", q);
	fflush(stdout);
	if (fgets(line, sizeof(line), stdin) == NULL)
		return NULL;
	line[strcspn(line, "\r\n")] = '\0';
	return xstrdup(line);
}

static int
logged_in(char *who, size_t n)
{
	struct buf b = { 0 };
	char *path = xasprintf("%s/login", data_dir()), *nl;
	int ok = 0;

	if (read_file(path, &b) == 0 && b.len &&
	    (nl = strchr((char *)b.p, '\n'))) {
		*nl = '\0';
		snprintf(who, n, "%s", (char *)b.p);
		ok = 1;
	}
	if (b.p)
		memset(b.p, 0, b.len);
	buf_free(&b);
	free(path);
	return ok;
}

/* Run a command in a child so the menu survives die(). */
static void
sub(void (*fn)(void *), void *arg)
{
	pid_t pid;
	int st;

	fflush(stdout);
	if ((pid = fork()) == 0) {
		fn(arg);
		exit(0);
	}
	if (pid > 0)
		waitpid(pid, &st, 0);
}

static void f_login(void *a) { (void)a; cmd_login(NULL); }
static void f_library(void *a) { (void)a; cmd_library(); }
static void f_install(void *a) { cmd_install(*(uint32_t *)a); }
static void f_run(void *a) { cmd_run(a, 0, NULL); }

static void
cmd_menu(void)
{
	for (;;) {
		struct game *g;
		size_t n, i;
		char who[256], *c;

		g = all_games(&n);
		printf("\n");
		if (logged_in(who, sizeof(who)))
			printf("Steam: logged in as %s\n", who);
		else
			printf("Steam: not logged in\n");
		if (n == 0)
			printf("No games installed yet.\n");
		for (i = 0; i < n; i++)
			printf("  %2zu) %-40s %s\n", i + 1, g[i].name,
			    g[i].steam ? "Steam" : "own");
		printf("\n  number = play   l = log in   s = Steam library   "
		    "i = install   a = add own game   q = quit\n");
		c = ask("> ");
		if (c == NULL || strcmp(c, "q") == 0) {
			free(c);
			for (i = 0; i < n; i++)
				game_free(&g[i]);
			free(g);
			return;
		}
		if (isdigit((unsigned char)c[0])) {
			size_t k = strtoul(c, NULL, 10);
			if (k >= 1 && k <= n)
				sub(f_run, g[k - 1].id);
		} else if (strcmp(c, "l") == 0)
			sub(f_login, NULL);
		else if (strcmp(c, "s") == 0)
			sub(f_library, NULL);
		else if (strcmp(c, "i") == 0) {
			char *id = ask("App ID to install (see s): ");
			uint32_t appid = id ? strtoul(id, NULL, 10) : 0;
			if (appid)
				sub(f_install, &appid);
			free(id);
		} else if (strcmp(c, "a") == 0) {
			char *name = ask("Name: "), *exe = NULL;
			if (name && *name)
				exe = ask("Path to the .exe: ");
			if (exe && *exe) {
				pid_t pid;
				int st;
				fflush(stdout);
				if ((pid = fork()) == 0) {
					cmd_add(name, exe, 0, NULL);
					exit(0);
				}
				if (pid > 0)
					waitpid(pid, &st, 0);
			}
			free(name);
			free(exe);
		}
		free(c);
		for (i = 0; i < n; i++)
			game_free(&g[i]);
		free(g);
	}
}

static uint32_t
appid_arg(const char *s)
{
	char *end;
	unsigned long v = strtoul(s, &end, 10);

	if (*s == '\0' || *end || v == 0 || v > UINT32_MAX)
		die("not an app ID: %s", s);
	return v;
}

int
main(int argc, char **argv)
{
	const char *cmd;
	int ch;

	while ((ch = getopt(argc, argv, "+ab:j:v")) != -1) {
		switch (ch) {
		case 'a': anonymous = 1; break;
		case 'b': branch = optarg; break;
		case 'j':
			threads = atoi(optarg);
			if (threads < 1 || threads > 64)
				die("-j: 1 to 64");
			break;
		case 'v': verbose = 1; break;
		default: usage();
		}
	}
	argc -= optind;
	argv += optind;
	net_init();
	cmd = argc > 0 ? argv[0] : "menu";
	/* Options may also follow the command (not for run/add arguments). */
	if (argc > 1 && strcmp(cmd, "run") && strcmp(cmd, "add")) {
		int i, n = 1;
		for (i = 1; i < argc; i++) {
			if (strcmp(argv[i], "-a") == 0)
				anonymous = 1;
			else if (strcmp(argv[i], "-v") == 0)
				verbose = 1;
			else if (strcmp(argv[i], "-b") == 0 && i + 1 < argc)
				branch = argv[++i];
			else if (strcmp(argv[i], "-j") == 0 && i + 1 < argc) {
				threads = atoi(argv[++i]);
				if (threads < 1 || threads > 64)
					die("-j: 1 to 64");
			} else
				argv[n++] = argv[i];
		}
		argc = n;
	}

	if (strcmp(cmd, "login") == 0 && argc <= 2)
		cmd_login(argc == 2 ? argv[1] : NULL);
	else if (strcmp(cmd, "logout") == 0 && argc == 1)
		cmd_logout();
	else if (strcmp(cmd, "library") == 0 && argc == 1)
		cmd_library();
	else if (strcmp(cmd, "info") == 0 && argc == 2)
		cmd_info(appid_arg(argv[1]));
	else if (strcmp(cmd, "install") == 0 && argc == 2)
		cmd_install(appid_arg(argv[1]));
	else if (strcmp(cmd, "uninstall") == 0 && argc == 2) {
		appid_arg(argv[1]);
		cmd_uninstall(argv[1]);
	} else if (strcmp(cmd, "free") == 0 && argc == 2)
		cmd_free(appid_arg(argv[1]));
	else if (strcmp(cmd, "add") == 0 && argc >= 3)
		cmd_add(argv[1], argv[2], argc - 3, argv + 3);
	else if (strcmp(cmd, "remove") == 0 && argc == 2)
		cmd_remove(argv[1]);
	else if (strcmp(cmd, "list") == 0 && argc == 1)
		cmd_list();
	else if (strcmp(cmd, "run") == 0 && argc >= 2)
		cmd_run(argv[1], argc - 2, argv + 2);
	else if (strcmp(cmd, "menu") == 0)
		cmd_menu();
	else
		usage();
	return 0;
}

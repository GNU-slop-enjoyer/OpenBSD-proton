/*
 * steamapi.exe: checks proton-openbsd's steamclient64.dll through a
 * game's own Steamworks library.
 *
 *	steamapi.exe path\to\steam_api64.dll
 *
 * Run it under steam.exe (as proton-openbsd does for Steam games), with
 * SteamAppId set.  It initialises the Steam API, reads the account and
 * app, sets an achievement and stores stats, writes and reads a cloud
 * file, and checks that the callbacks and call results arrive through
 * manual dispatch.  Output is TAP.
 *
 * Copyright (c) 2026 The OpenBSD Proton project contributors
 * Distributed under the LGPL 2.1 or later.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
	int32_t	 user;
	int32_t	 id;
	uint8_t	*param;
	int32_t	 size;
} callback_msg;

static HMODULE api;
static int ntest, nfail;

static void
ok(int cond, const char *what)
{
	ntest++;
	if (!cond)
		nfail++;
	printf("%s %d - %s\n", cond ? "ok" : "not ok", ntest, what);
	fflush(stdout);
}

static void *
sym(const char *name)
{
	void *p = (void *)GetProcAddress(api, name);

	if (p == NULL)
		printf("# %s not exported\n", name);
	return p;
}

/* first exported accessor of the given interface, newest version first */
static void *
iface(const char *prefix)
{
	char name[96];
	void *(*get)(void);
	int v;

	for (v = 30; v > 0; v--) {
		snprintf(name, sizeof(name), "SteamAPI_%s_v%03d", prefix, v);
		if ((get = (void *)GetProcAddress(api, name)))
			return get();
	}
	printf("# no accessor for %s\n", prefix);
	return NULL;
}

int
main(int argc, char **argv)
{
	int8_t (*init)(void);
	int8_t (*running)(void);
	int32_t (*pipe_of)(void);
	void (*md_init)(void), (*md_frame)(int32_t), (*md_free)(int32_t);
	int8_t (*md_next)(int32_t, callback_msg *);
	int8_t (*md_result)(int32_t, uint64_t, void *, int32_t, int32_t,
	    int8_t *);
	uint64_t (*get_steamid)(void *);
	const char *(*persona)(void *);
	int8_t (*subscribed_app)(void *, uint32_t);
	uint32_t (*appid_of)(void *);
	int8_t (*set_ach)(void *, const char *), (*get_ach)(void *,
	    const char *, int8_t *), (*store)(void *);
	int8_t (*fwrite_)(void *, const char *, const void *, int32_t);
	int32_t (*fread_)(void *, const char *, void *, int32_t);
	uint64_t (*players)(void *);
	void *user, *friends, *apps, *utils, *stats, *rs;
	int32_t pipe;
	callback_msg m;
	int got_stored = 0, got_done = 0, i;
	uint64_t call;
	char buf[64];
	int8_t a = 0;

	if (argc != 2) {
		fprintf(stderr, "usage: steamapi.exe steam_api64.dll\n");
		return 2;
	}
	printf("1..12\n");
	if ((api = LoadLibraryA(argv[1])) == NULL) {
		printf("Bail out! cannot load %s (%lu)\n", argv[1],
		    (unsigned long)GetLastError());
		return 1;
	}
	init = sym("SteamAPI_Init");
	running = sym("SteamAPI_IsSteamRunning");
	md_init = sym("SteamAPI_ManualDispatch_Init");
	ok(running && running(), "SteamAPI_IsSteamRunning");
	if (!init || !init()) {
		printf("Bail out! SteamAPI_Init failed\n");
		return 1;
	}
	ok(1, "SteamAPI_Init");
	md_init();	/* only valid after SteamAPI_Init */
	pipe_of = sym("SteamAPI_GetHSteamPipe");
	md_frame = sym("SteamAPI_ManualDispatch_RunFrame");
	md_next = sym("SteamAPI_ManualDispatch_GetNextCallback");
	md_free = sym("SteamAPI_ManualDispatch_FreeLastCallback");
	md_result = sym("SteamAPI_ManualDispatch_GetAPICallResult");
	pipe = pipe_of ? pipe_of() : 0;

	user = iface("SteamUser");
	friends = iface("SteamFriends");
	apps = iface("SteamApps");
	utils = iface("SteamUtils");
	stats = iface("SteamUserStats");
	rs = iface("SteamRemoteStorage");
	get_steamid = sym("SteamAPI_ISteamUser_GetSteamID");
	persona = sym("SteamAPI_ISteamFriends_GetPersonaName");
	subscribed_app = sym("SteamAPI_ISteamApps_BIsSubscribedApp");
	appid_of = sym("SteamAPI_ISteamUtils_GetAppID");
	set_ach = sym("SteamAPI_ISteamUserStats_SetAchievement");
	get_ach = sym("SteamAPI_ISteamUserStats_GetAchievement");
	store = sym("SteamAPI_ISteamUserStats_StoreStats");
	players = sym("SteamAPI_ISteamUserStats_GetNumberOfCurrentPlayers");
	fwrite_ = sym("SteamAPI_ISteamRemoteStorage_FileWrite");
	fread_ = sym("SteamAPI_ISteamRemoteStorage_FileRead");

	printf("# steamid %llu, persona \"%s\", appid %u\n",
	    (unsigned long long)get_steamid(user), persona(friends),
	    appid_of(utils));
	ok((get_steamid(user) >> 52) == 0x011, "GetSteamID is an individual account");
	ok(persona(friends) && *persona(friends), "GetPersonaName");
	ok(appid_of(utils) == (uint32_t)atoi(getenv("SteamAppId") ?
	    getenv("SteamAppId") : "0"), "GetAppID is SteamAppId");
	ok(subscribed_app(apps, appid_of(utils)), "BIsSubscribedApp(own app)");
	ok(set_ach(stats, "TEST_ACHIEVEMENT"), "SetAchievement");
	ok(get_ach(stats, "TEST_ACHIEVEMENT", &a) && a, "GetAchievement after Set");
	ok(store(stats), "StoreStats");
	ok(fwrite_(rs, "test/cloud.txt", "hello", 5) &&
	    fread_(rs, "test/cloud.txt", buf, sizeof(buf)) == 5 &&
	    memcmp(buf, "hello", 5) == 0, "cloud file write and read");
	call = players(stats);

	for (i = 0; i < 50 && !(got_stored && got_done); i++) {
		md_frame(pipe);
		while (md_next(pipe, &m)) {
			printf("# callback %d (%d bytes)\n", m.id, m.size);
			if (m.id == 1102)
				got_stored = 1;
			if (m.id == 703) {
				uint8_t r[16];
				int8_t failed = 1;
				uint64_t c;
				memcpy(&c, m.param, 8);
				if (c == call && md_result(pipe, c, r,
				    sizeof(r), 1107, &failed) && !failed)
					got_done = 1;
			}
			md_free(pipe);
		}
		Sleep(20);
	}
	ok(got_stored, "UserStatsStored_t callback");
	ok(got_done, "GetNumberOfCurrentPlayers call result");
	return nfail != 0;
}

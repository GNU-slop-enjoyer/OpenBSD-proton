/*
 * Steamworks interface methods.  Each function takes the interface
 * pointer first (the Win64 "this") and then the method's parameters;
 * impl.list says which method and signature each one implements, and
 * gen.sh puts it into every interface version with that signature.
 *
 * Copyright (c) 2026 The OpenBSD Proton project contributors
 * Distributed under the LGPL 2.1 or later.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "steamclient.h"

#define UNUSED(x) (void)(x)

/* ---- ISteamClient ---- */

int32_t client_CreateSteamPipe(void *self) { UNUSED(self); return 1; }
int8_t client_BReleaseSteamPipe(void *self, int32_t p) { UNUSED(self); UNUSED(p); return 1; }
int32_t client_ConnectToGlobalUser(void *self, int32_t p) { UNUSED(self); UNUSED(p); return 1; }

int32_t
client_CreateLocalUser(void *self, int32_t *pipe, uint32_t type)
{
	UNUSED(self);
	UNUSED(type);
	if (pipe)
		*pipe = 1;
	return 1;
}

/* ---- ISteamUser ---- */

int32_t user_GetHSteamUser(void *self) { UNUSED(self); return 1; }
int8_t user_BLoggedOn(void *self) { UNUSED(self); return 1; }

uint64_t *
user_GetSteamID(void *self, uint64_t *ret)
{
	UNUSED(self);
	*ret = cfg.steamid;
	return ret;
}

static void
make_dirs(const char *path)
{
	char buf[MAX_PATH];
	size_t i;

	snprintf(buf, sizeof(buf), "%s", path);
	for (i = 3; buf[i]; i++)
		if (buf[i] == '\\') {
			buf[i] = '\0';
			CreateDirectoryA(buf, NULL);
			buf[i] = '\\';
		}
	CreateDirectoryA(buf, NULL);
}

int8_t
user_GetUserDataFolder(void *self, char *buf, int32_t size)
{
	UNUSED(self);
	make_dirs(cfg.userdir);
	if (size <= 0 || (int)strlen(cfg.userdir) >= size)
		return 0;
	strcpy(buf, cfg.userdir);
	return 1;
}

#pragma pack(push, 8)
struct auth_ticket_response { uint32_t ticket; int32_t result; };
struct validate_auth_ticket { uint64_t steamid; int32_t response; uint64_t owner; };
struct web_api_ticket { uint32_t ticket; int32_t result; int32_t size; uint8_t data[2560]; };
#pragma pack(pop)

static uint32_t next_ticket = 1;

/*
 * A session ticket other copies of this client accept (BeginAuthSession):
 * "OBSD", SteamID, appid.  Steam's own servers do not accept it.
 */
uint32_t
user_GetAuthSessionTicket(void *self, void *ticket, int32_t max,
    uint32_t *len, void *identity)
{
	struct auth_ticket_response r;
	unsigned char t[16];

	UNUSED(self);
	UNUSED(identity);
	memcpy(t, "OBSD", 4);
	memcpy(t + 4, &cfg.steamid, 8);
	memcpy(t + 12, &cfg.appid, 4);
	if (max < (int32_t)sizeof(t)) {
		if (len)
			*len = 0;
		return 0;
	}
	memcpy(ticket, t, sizeof(t));
	if (len)
		*len = sizeof(t);
	r.ticket = next_ticket++;
	r.result = K_ERESULT_OK;
	post_callback(CB_GET_AUTH_SESSION_TICKET, &r, sizeof(r));
	return r.ticket;
}

uint32_t
user_GetAuthTicketForWebApi(void *self, const char *identity)
{
	struct web_api_ticket r;

	UNUSED(self);
	UNUSED(identity);
	memset(&r, 0, sizeof(r));
	r.ticket = next_ticket++;
	r.result = K_ERESULT_NO_CONNECTION;
	post_callback(CB_GET_TICKET_FOR_WEB_API, &r, sizeof(r));
	return r.ticket;
}

uint32_t
user_BeginAuthSession(void *self, const void *ticket, int32_t len,
    uint64_t steamid)
{
	struct validate_auth_ticket v;
	const unsigned char *t = ticket;
	uint64_t id;

	UNUSED(self);
	if (len < 16 || memcmp(t, "OBSD", 4) != 0)
		return 1;	/* k_EBeginAuthSessionResultInvalidTicket */
	memcpy(&id, t + 4, 8);
	if (id != steamid)
		return 1;
	v.steamid = steamid;
	v.response = 0;		/* k_EAuthSessionResponseOK */
	v.owner = steamid;
	post_callback(CB_VALIDATE_AUTH_TICKET, &v, sizeof(v));
	return 0;
}

uint32_t
user_UserHasLicenseForApp(void *self, uint64_t steamid, uint32_t appid)
{
	UNUSED(self);
	if (steamid != cfg.steamid)
		return 2;	/* k_EUserHasLicenseResultNoAuth */
	return owns_app(appid) ? 0 : 1;
}

int8_t user_BIsBehindNAT(void *self) { UNUSED(self); return 1; }
int32_t user_GetPlayerSteamLevel(void *self) { UNUSED(self); return 1; }
uint32_t user_GetVoiceOptimalSampleRate(void *self) { UNUSED(self); return 11025; }

uint64_t
user_RequestEncryptedAppTicket(void *self, void *data, int32_t len)
{
	int32_t result = K_ERESULT_NO_CONNECTION;

	UNUSED(self);
	UNUSED(data);
	UNUSED(len);
	return post_result(CB_ENCRYPTED_APP_TICKET, &result, sizeof(result), 0);
}

uint64_t
user_RequestStoreAuthURL(void *self, const char *url)
{
	char r[512];

	UNUSED(self);
	memset(r, 0, sizeof(r));
	snprintf(r, sizeof(r), "%s", url ? url : "");
	return post_result(CB_STORE_AUTH_URL, r, sizeof(r), 0);
}

/* ---- ISteamUtils ---- */

static DWORD start_ticks;

uint32_t
utils_GetSecondsSinceAppActive(void *self)
{
	UNUSED(self);
	if (start_ticks == 0)
		start_ticks = GetTickCount();
	return (GetTickCount() - start_ticks) / 1000;
}

uint32_t utils_GetSecondsSinceComputerActive(void *self) { UNUSED(self); return GetTickCount() / 1000; }
uint32_t utils_GetConnectedUniverse(void *self) { UNUSED(self); return 1; }
uint32_t utils_GetServerRealTime(void *self) { UNUSED(self); return (uint32_t)time(NULL); }

const char *
utils_GetIPCountry(void *self)
{
	static char c[3] = "US";
	char buf[16];

	UNUSED(self);
	if (GetLocaleInfoA(LOCALE_USER_DEFAULT, LOCALE_SISO3166CTRYNAME, buf,
	    sizeof(buf)) == 3) {
		c[0] = buf[0];
		c[1] = buf[1];
	}
	return c;
}

uint8_t utils_GetCurrentBatteryPower(void *self) { UNUSED(self); return 255; }
uint32_t utils_GetAppID(void *self) { UNUSED(self); return cfg.appid; }

int8_t
utils_IsAPICallCompleted(void *self, uint64_t call, int8_t *failed)
{
	UNUSED(self);
	return (int8_t)call_completed(call, failed);
}

uint32_t
utils_GetAPICallFailureReason(void *self, uint64_t call)
{
	int8_t failed;

	UNUSED(self);
	if (!call_completed(call, &failed))
		return 3;	/* k_ESteamAPICallFailureInvalidHandle */
	return failed ? 1 : (uint32_t)-1;	/* NetworkFailure : None */
}

int8_t
utils_GetAPICallResult(void *self, uint64_t call, void *out, int32_t size,
    int32_t expected, int8_t *failed)
{
	UNUSED(self);
	return (int8_t)get_call_result(call, out, size, expected, failed);
}

const char *utils_GetSteamUILanguage(void *self) { UNUSED(self); return cfg.language; }
int8_t utils_IsRunningUnderProton(void *self) { UNUSED(self); return 1; }

int8_t
utils_InitFilterText(void *self, uint32_t flags)
{
	UNUSED(self);
	UNUSED(flags);
	return 1;
}

/* No filtering: the text is copied unchanged. */
int32_t
utils_FilterText(void *self, uint32_t context, uint64_t source,
    const char *in, char *out, uint32_t size)
{
	UNUSED(self);
	UNUSED(context);
	UNUSED(source);
	if (size == 0)
		return 0;
	snprintf(out, size, "%s", in ? in : "");
	return 0;
}

/* ---- ISteamApps ---- */

int8_t apps_BIsSubscribed(void *self) { UNUSED(self); return 1; }
int8_t apps_BIsSubscribedApp(void *self, uint32_t a) { UNUSED(self); return (int8_t)owns_app(a); }
int8_t apps_BIsDlcInstalled(void *self, uint32_t a) { UNUSED(self); return (int8_t)owns_app(a); }
int8_t apps_BIsAppInstalled(void *self, uint32_t a) { UNUSED(self); return a == cfg.appid; }
const char *apps_GetCurrentGameLanguage(void *self) { UNUSED(self); return cfg.language; }
const char *apps_GetAvailableGameLanguages(void *self) { UNUSED(self); return cfg.language; }
uint32_t apps_GetEarliestPurchaseUnixTime(void *self, uint32_t a) { UNUSED(self); return owns_app(a) ? 1262304000 : 0; }
int32_t apps_GetDLCCount(void *self) { UNUSED(self); return (int32_t)cfg.ndlc; }
int32_t apps_GetAppBuildId(void *self) { UNUSED(self); return cfg.buildid; }
int8_t apps_MarkContentCorrupt(void *self, int8_t a) { UNUSED(self); UNUSED(a); return 1; }
int8_t apps_SetDlcContext(void *self, uint32_t a) { UNUSED(self); UNUSED(a); return 1; }

int8_t
apps_BGetDLCDataByIndex(void *self, int32_t i, uint32_t *appid,
    int8_t *available, char *name, int32_t size)
{
	UNUSED(self);
	if (i < 0 || (size_t)i >= cfg.ndlc)
		return 0;
	if (appid)
		*appid = cfg.dlc[i];
	if (available)
		*available = 1;
	if (name && size > 0)
		snprintf(name, size, "%s", cfg.dlcname[i]);
	return 1;
}

uint32_t
apps_GetAppInstallDir(void *self, uint32_t appid, char *buf, uint32_t size)
{
	UNUSED(self);
	if (appid != cfg.appid || cfg.gamedir[0] == '\0') {
		if (size)
			buf[0] = '\0';
		return 0;
	}
	if (size)
		snprintf(buf, size, "%s", cfg.gamedir);
	return (uint32_t)strlen(cfg.gamedir) + 1;
}

uint64_t *
apps_GetAppOwner(void *self, uint64_t *ret)
{
	UNUSED(self);
	*ret = cfg.steamid;
	return ret;
}

const char *apps_GetLaunchQueryParam(void *self, const char *k) { UNUSED(self); UNUSED(k); return ""; }

int32_t
apps_GetLaunchCommandLine(void *self, char *buf, int32_t size)
{
	UNUSED(self);
	if (size > 0)
		buf[0] = '\0';
	return 0;
}

/* ---- ISteamFriends ---- */

const char *friends_GetPersonaName(void *self) { UNUSED(self); return cfg.name; }
uint32_t friends_GetPersonaState(void *self) { UNUSED(self); return 1; }	/* online */

const char *
friends_GetFriendPersonaName(void *self, uint64_t id)
{
	UNUSED(self);
	return id == cfg.steamid ? cfg.name : "";
}

uint32_t
friends_GetFriendPersonaState(void *self, uint64_t id)
{
	UNUSED(self);
	return id == cfg.steamid ? 1 : 0;
}

const char *friends_GetPlayerNickname(void *self, uint64_t id) { UNUSED(self); UNUSED(id); return NULL; }
int8_t friends_SetRichPresence(void *self, const char *k, const char *v) { UNUSED(self); UNUSED(k); UNUSED(v); return 1; }
int8_t friends_RequestUserInformation(void *self, uint64_t id, int8_t n) { UNUSED(self); UNUSED(id); UNUSED(n); return 0; }

/* ---- ISteamUserStats: stored in userdata\stats.txt ---- */

struct stat_entry {
	char	 name[128];
	char	 type;		/* 'i', 'f' or 'a' (achievement) */
	int32_t	 i;
	float	 f;
	uint32_t time;		/* achievement unlock time */
};

static struct stat_entry *stats;
static size_t nstats;
static int stats_loaded;

static void
stats_path(char *buf, size_t n)
{
	snprintf(buf, n, "%s\\stats.txt", cfg.userdir);
}

static void
stats_load(void)
{
	char path[MAX_PATH], line[256];
	FILE *f;

	if (stats_loaded)
		return;
	stats_loaded = 1;
	stats_path(path, sizeof(path));
	if ((f = fopen(path, "r")) == NULL)
		return;
	while (fgets(line, sizeof(line), f)) {
		struct stat_entry e;
		char type[2];

		memset(&e, 0, sizeof(e));
		if (sscanf(line, "%1s %127s", type, e.name) != 2)
			continue;
		e.type = type[0];
		if (e.type == 'i')
			sscanf(line, "%*s %*s %d", &e.i);
		else if (e.type == 'f')
			sscanf(line, "%*s %*s %f", &e.f);
		else if (e.type == 'a')
			sscanf(line, "%*s %*s %d %u", &e.i, &e.time);
		else
			continue;
		stats = realloc(stats, (nstats + 1) * sizeof(*stats));
		stats[nstats++] = e;
	}
	fclose(f);
}

static int
stats_save(void)
{
	char path[MAX_PATH], tmp[MAX_PATH];
	FILE *f;
	size_t i;

	make_dirs(cfg.userdir);
	stats_path(path, sizeof(path));
	snprintf(tmp, sizeof(tmp), "%s.new", path);
	if ((f = fopen(tmp, "w")) == NULL)
		return 0;
	for (i = 0; i < nstats; i++) {
		if (stats[i].type == 'i')
			fprintf(f, "i %s %d\n", stats[i].name, stats[i].i);
		else if (stats[i].type == 'f')
			fprintf(f, "f %s %.9g\n", stats[i].name, stats[i].f);
		else
			fprintf(f, "a %s %d %u\n", stats[i].name, stats[i].i,
			    stats[i].time);
	}
	if (fclose(f) != 0)
		return 0;
	return MoveFileExA(tmp, path, MOVEFILE_REPLACE_EXISTING) != 0;
}

static struct stat_entry *
stat_find(const char *name, char type, int create)
{
	size_t i;

	stats_load();
	if (name == NULL || strlen(name) >= sizeof(stats->name) ||
	    strpbrk(name, " \t\r\n"))
		return NULL;
	for (i = 0; i < nstats; i++)
		if (stats[i].type == type && strcmp(stats[i].name, name) == 0)
			return &stats[i];
	if (!create)
		return NULL;
	stats = realloc(stats, (nstats + 1) * sizeof(*stats));
	memset(&stats[nstats], 0, sizeof(*stats));
	snprintf(stats[nstats].name, sizeof(stats->name), "%s", name);
	stats[nstats].type = type;
	return &stats[nstats++];
}

#pragma pack(push, 8)
struct user_stats_received { uint64_t game; int32_t result; uint64_t user; };
struct user_stats_stored { uint64_t game; int32_t result; };
struct achievement_stored {
	uint64_t game; int8_t group; char name[128];
	uint32_t cur; uint32_t max;
};
#pragma pack(pop)

void
stats_announce(void)
{
	static int done;
	struct user_stats_received r;

	if (done)
		return;
	done = 1;
	stats_load();
	r.game = cfg.appid;
	r.result = K_ERESULT_OK;
	r.user = cfg.steamid;
	post_callback(CB_USER_STATS_RECEIVED, &r, sizeof(r));
}

int8_t
stats_RequestCurrentStats(void *self)
{
	UNUSED(self);
	stats_announce();
	return 1;
}

int8_t stats_RequestCurrentStats_gid(void *self, uint64_t g) { UNUSED(g); return stats_RequestCurrentStats(self); }

/* The stats a game defines live on Steam's servers; unknown ones read 0. */
int8_t
stats_GetStat(void *self, const char *name, int32_t *v)
{
	struct stat_entry *e;

	UNUSED(self);
	if (v == NULL)
		return 0;
	e = stat_find(name, 'i', 0);
	*v = e ? e->i : 0;
	return 1;
}

int8_t
stats_GetStatFloat(void *self, const char *name, float *v)
{
	struct stat_entry *e;

	UNUSED(self);
	if (v == NULL)
		return 0;
	e = stat_find(name, 'f', 0);
	*v = e ? e->f : 0;
	return 1;
}

int8_t
stats_SetStat(void *self, const char *name, int32_t v)
{
	struct stat_entry *e;

	UNUSED(self);
	if ((e = stat_find(name, 'i', 1)) == NULL)
		return 0;
	e->i = v;
	return 1;
}

int8_t
stats_SetStatFloat(void *self, const char *name, float v)
{
	struct stat_entry *e;

	UNUSED(self);
	if ((e = stat_find(name, 'f', 1)) == NULL)
		return 0;
	e->f = v;
	return 1;
}

int8_t
stats_UpdateAvgRateStat(void *self, const char *name, float count,
    double length)
{
	struct stat_entry *e;

	UNUSED(self);
	if ((e = stat_find(name, 'f', 1)) == NULL)
		return 0;
	if (length > 0)
		e->f = (float)(count / length);
	return 1;
}

int8_t
stats_GetAchievementAndUnlockTime(void *self, const char *name,
    int8_t *achieved, uint32_t *when)
{
	struct stat_entry *e;

	UNUSED(self);
	e = stat_find(name, 'a', 0);
	if (achieved)
		*achieved = e ? (int8_t)e->i : 0;
	if (when)
		*when = e && e->i ? e->time : 0;
	return 1;
}

int8_t
stats_GetAchievement(void *self, const char *name, int8_t *achieved)
{
	return stats_GetAchievementAndUnlockTime(self, name, achieved, NULL);
}

int8_t
stats_SetAchievement(void *self, const char *name)
{
	struct stat_entry *e;
	struct achievement_stored s;

	UNUSED(self);
	if ((e = stat_find(name, 'a', 1)) == NULL)
		return 0;
	if (!e->i) {
		e->i = 1;
		e->time = (uint32_t)time(NULL);
		memset(&s, 0, sizeof(s));
		s.game = cfg.appid;
		snprintf(s.name, sizeof(s.name), "%s", name);
		post_callback(CB_USER_ACHIEVEMENT_STORED, &s, sizeof(s));
	}
	return 1;
}

int8_t
stats_ClearAchievement(void *self, const char *name)
{
	struct stat_entry *e;

	UNUSED(self);
	if ((e = stat_find(name, 'a', 1)) == NULL)
		return 0;
	e->i = 0;
	e->time = 0;
	return 1;
}

int8_t
stats_IndicateAchievementProgress(void *self, const char *name,
    uint32_t cur, uint32_t max)
{
	struct achievement_stored s;

	UNUSED(self);
	memset(&s, 0, sizeof(s));
	s.game = cfg.appid;
	snprintf(s.name, sizeof(s.name), "%s", name ? name : "");
	s.cur = cur;
	s.max = max;
	post_callback(CB_USER_ACHIEVEMENT_STORED, &s, sizeof(s));
	return 1;
}

int8_t
stats_StoreStats(void *self)
{
	struct user_stats_stored s;

	UNUSED(self);
	stats_load();
	s.game = cfg.appid;
	s.result = stats_save() ? K_ERESULT_OK : K_ERESULT_FAIL;
	post_callback(CB_USER_STATS_STORED, &s, sizeof(s));
	return 1;
}

int8_t
stats_ResetAllStats(void *self, int8_t achievements)
{
	size_t i, j;

	UNUSED(self);
	stats_load();
	for (i = j = 0; i < nstats; i++)
		if (stats[i].type == 'a' && !achievements)
			stats[j++] = stats[i];
	nstats = j;
	return 1;
}

/* older interfaces pass the CGameID first */
int8_t stats_GetStat_gid(void *self, uint64_t g, const char *n, int32_t *v) { UNUSED(g); return stats_GetStat(self, n, v); }
int8_t stats_GetStatFloat_gid(void *self, uint64_t g, const char *n, float *v) { UNUSED(g); return stats_GetStatFloat(self, n, v); }
int8_t stats_SetStat_gid(void *self, uint64_t g, const char *n, int32_t v) { UNUSED(g); return stats_SetStat(self, n, v); }
int8_t stats_SetStatFloat_gid(void *self, uint64_t g, const char *n, float v) { UNUSED(g); return stats_SetStatFloat(self, n, v); }
int8_t stats_GetAchievement_gid(void *self, uint64_t g, const char *n, int8_t *a) { UNUSED(g); return stats_GetAchievement(self, n, a); }
int8_t stats_SetAchievement_gid(void *self, uint64_t g, const char *n) { UNUSED(g); return stats_SetAchievement(self, n); }
int8_t stats_ClearAchievement_gid(void *self, uint64_t g, const char *n) { UNUSED(g); return stats_ClearAchievement(self, n); }
int8_t stats_StoreStats_gid(void *self, uint64_t g) { UNUSED(g); return stats_StoreStats(self); }

#pragma pack(push, 8)
struct leaderboard_find { uint64_t board; uint8_t found; };
struct current_players { uint8_t success; int32_t players; };
struct global_ach_ready { uint64_t game; int32_t result; };
#pragma pack(pop)

uint64_t
stats_FindLeaderboard(void *self, const char *name)
{
	struct leaderboard_find r = { 0, 0 };

	UNUSED(self);
	UNUSED(name);
	return post_result(CB_LEADERBOARD_FIND_RESULT, &r, sizeof(r), 0);
}

uint64_t
stats_FindOrCreateLeaderboard(void *self, const char *name, uint32_t sort,
    uint32_t display)
{
	UNUSED(sort);
	UNUSED(display);
	return stats_FindLeaderboard(self, name);
}

uint64_t
stats_GetNumberOfCurrentPlayers(void *self)
{
	struct current_players r = { 0, 0 };

	UNUSED(self);
	return post_result(CB_NUMBER_OF_CURRENT_PLAYERS, &r, sizeof(r), 0);
}

uint64_t
stats_RequestGlobalAchievementPercentages(void *self)
{
	struct global_ach_ready r;

	UNUSED(self);
	r.game = cfg.appid;
	r.result = K_ERESULT_NO_CONNECTION;
	return post_result(CB_GLOBAL_ACH_PERCENTAGES_READY, &r, sizeof(r), 0);
}

uint64_t
stats_RequestUserStats(void *self, uint64_t user)
{
	struct user_stats_received r;

	UNUSED(self);
	r.game = cfg.appid;
	r.result = user == cfg.steamid ? K_ERESULT_OK : K_ERESULT_FAIL;
	r.user = user;
	return post_result(CB_USER_STATS_RECEIVED, &r, sizeof(r), 0);
}

int8_t
stats_GetUserStat(void *self, uint64_t user, const char *name, int32_t *v)
{
	return user == cfg.steamid ? stats_GetStat(self, name, v) : 0;
}

int8_t
stats_GetUserStatFloat(void *self, uint64_t user, const char *name, float *v)
{
	return user == cfg.steamid ? stats_GetStatFloat(self, name, v) : 0;
}

int8_t
stats_GetUserAchievement(void *self, uint64_t user, const char *name,
    int8_t *a)
{
	return user == cfg.steamid ? stats_GetAchievement(self, name, a) : 0;
}

int8_t
stats_GetUserAchievementAndUnlockTime(void *self, uint64_t user,
    const char *name, int8_t *a, uint32_t *t)
{
	return user == cfg.steamid ?
	    stats_GetAchievementAndUnlockTime(self, name, a, t) : 0;
}

/* ---- ISteamRemoteStorage: files in userdata\remote ---- */

static int
remote_path(const char *name, char *out, size_t n)
{
	size_t i, len;

	if (name == NULL || *name == '\0' || strstr(name, "..") ||
	    name[0] == '/' || name[0] == '\\' || strchr(name, ':'))
		return 0;
	len = (size_t)snprintf(out, n, "%s\\remote\\%s", cfg.userdir, name);
	if (len >= n)
		return 0;
	for (i = strlen(cfg.userdir); out[i]; i++)
		if (out[i] == '/')
			out[i] = '\\';
	return 1;
}

static void
make_parent(const char *path)
{
	char dir[MAX_PATH], *s;

	snprintf(dir, sizeof(dir), "%s", path);
	if ((s = strrchr(dir, '\\'))) {
		*s = '\0';
		make_dirs(dir);
	}
}

int8_t
rs_FileWrite(void *self, const char *name, const void *data, int32_t size)
{
	char path[MAX_PATH], tmp[MAX_PATH];
	HANDLE h;
	DWORD n = 0;
	BOOL ok;

	UNUSED(self);
	if (size < 0 || !remote_path(name, path, sizeof(path)))
		return 0;
	make_parent(path);
	snprintf(tmp, sizeof(tmp), "%s.new", path);
	h = CreateFileA(tmp, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
	    FILE_ATTRIBUTE_NORMAL, NULL);
	if (h == INVALID_HANDLE_VALUE)
		return 0;
	ok = WriteFile(h, data, (DWORD)size, &n, NULL) && n == (DWORD)size;
	CloseHandle(h);
	if (!ok || !MoveFileExA(tmp, path, MOVEFILE_REPLACE_EXISTING)) {
		DeleteFileA(tmp);
		return 0;
	}
	return 1;
}

int32_t
rs_FileRead(void *self, const char *name, void *data, int32_t size)
{
	char path[MAX_PATH];
	HANDLE h;
	DWORD n = 0;

	UNUSED(self);
	if (size <= 0 || !remote_path(name, path, sizeof(path)))
		return 0;
	h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL,
	    OPEN_EXISTING, 0, NULL);
	if (h == INVALID_HANDLE_VALUE)
		return 0;
	if (!ReadFile(h, data, (DWORD)size, &n, NULL))
		n = 0;
	CloseHandle(h);
	return (int32_t)n;
}

int8_t
rs_FileExists(void *self, const char *name)
{
	char path[MAX_PATH];
	DWORD a;

	UNUSED(self);
	if (!remote_path(name, path, sizeof(path)))
		return 0;
	a = GetFileAttributesA(path);
	return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

int8_t
rs_FileDelete(void *self, const char *name)
{
	char path[MAX_PATH];

	UNUSED(self);
	return remote_path(name, path, sizeof(path)) && DeleteFileA(path);
}

int8_t rs_FileForget(void *self, const char *name) { return rs_FileExists(self, name); }

int32_t
rs_GetFileSize(void *self, const char *name)
{
	char path[MAX_PATH];
	WIN32_FILE_ATTRIBUTE_DATA d;

	UNUSED(self);
	if (!remote_path(name, path, sizeof(path)) ||
	    !GetFileAttributesExA(path, GetFileExInfoStandard, &d))
		return 0;
	return (int32_t)d.nFileSizeLow;
}

int64_t
rs_GetFileTimestamp(void *self, const char *name)
{
	char path[MAX_PATH];
	WIN32_FILE_ATTRIBUTE_DATA d;
	ULARGE_INTEGER t;

	UNUSED(self);
	if (!remote_path(name, path, sizeof(path)) ||
	    !GetFileAttributesExA(path, GetFileExInfoStandard, &d))
		return 0;
	t.LowPart = d.ftLastWriteTime.dwLowDateTime;
	t.HighPart = d.ftLastWriteTime.dwHighDateTime;
	return (int64_t)((t.QuadPart - 116444736000000000ULL) / 10000000);
}

struct remote_file { char name[MAX_PATH]; int32_t size; };
static struct remote_file *rfiles;
static int32_t nrfiles;

static void
list_dir(const char *dir, const char *prefix)
{
	char pattern[MAX_PATH], sub[MAX_PATH], rel[MAX_PATH];
	WIN32_FIND_DATAA fd;
	HANDLE h;

	snprintf(pattern, sizeof(pattern), "%s\\*", dir);
	if ((h = FindFirstFileA(pattern, &fd)) == INVALID_HANDLE_VALUE)
		return;
	do {
		if (strcmp(fd.cFileName, ".") == 0 ||
		    strcmp(fd.cFileName, "..") == 0)
			continue;
		snprintf(rel, sizeof(rel), "%s%s", prefix, fd.cFileName);
		if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
			snprintf(sub, sizeof(sub), "%s\\%s", dir, fd.cFileName);
			strncat(rel, "/", sizeof(rel) - strlen(rel) - 1);
			list_dir(sub, rel);
			continue;
		}
		rfiles = realloc(rfiles, (nrfiles + 1) * sizeof(*rfiles));
		snprintf(rfiles[nrfiles].name, MAX_PATH, "%s", rel);
		rfiles[nrfiles].size = (int32_t)fd.nFileSizeLow;
		nrfiles++;
	} while (FindNextFileA(h, &fd));
	FindClose(h);
}

int32_t
rs_GetFileCount(void *self)
{
	char dir[MAX_PATH];

	UNUSED(self);
	nrfiles = 0;
	snprintf(dir, sizeof(dir), "%s\\remote", cfg.userdir);
	list_dir(dir, "");
	return nrfiles;
}

const char *
rs_GetFileNameAndSize(void *self, int32_t i, int32_t *size)
{
	UNUSED(self);
	if (i < 0 || i >= nrfiles) {
		if (size)
			*size = 0;
		return "";
	}
	if (size)
		*size = rfiles[i].size;
	return rfiles[i].name;
}

int8_t
rs_GetQuota(void *self, uint64_t *total, uint64_t *avail)
{
	UNUSED(self);
	if (total)
		*total = 1ULL << 30;
	if (avail)
		*avail = 1ULL << 30;
	return 1;
}

int8_t rs_True(void *self) { UNUSED(self); return 1; }
uint32_t rs_GetSyncPlatforms(void *self, const char *n) { UNUSED(self); UNUSED(n); return 0xffffffff; }
int8_t rs_SetSyncPlatforms(void *self, const char *n, uint32_t p) { UNUSED(self); UNUSED(n); UNUSED(p); return 1; }

#pragma pack(push, 8)
struct file_write_async { int32_t result; };
struct file_read_async { uint64_t call; int32_t result; uint32_t offset; uint32_t read; };
#pragma pack(pop)

uint64_t
rs_FileWriteAsync(void *self, const char *name, const void *data,
    uint32_t size)
{
	struct file_write_async r;

	r.result = rs_FileWrite(self, name, data, (int32_t)size) ?
	    K_ERESULT_OK : K_ERESULT_FAIL;
	return post_result(CB_REMOTE_FILE_WRITE_ASYNC, &r, sizeof(r), 0);
}

/* data of finished FileReadAsync calls, for FileReadAsyncComplete */
struct async_read { uint64_t call; void *data; uint32_t size; struct async_read *next; };
static struct async_read *reads;

uint64_t
rs_FileReadAsync(void *self, const char *name, uint32_t offset,
    uint32_t size)
{
	struct file_read_async r;
	struct async_read *a;
	int32_t total = rs_GetFileSize(self, name);
	unsigned char *buf;

	memset(&r, 0, sizeof(r));
	r.offset = offset;
	if (!rs_FileExists(self, name) || offset + size > (uint32_t)total) {
		r.result = K_ERESULT_FILE_NOT_FOUND;
		return post_result(CB_REMOTE_FILE_READ_ASYNC, &r, sizeof(r), 0);
	}
	buf = malloc(total ? total : 1);
	rs_FileRead(self, name, buf, total);
	a = calloc(1, sizeof(*a));
	a->data = malloc(size ? size : 1);
	memcpy(a->data, buf + offset, size);
	a->size = size;
	free(buf);
	r.result = K_ERESULT_OK;
	r.read = size;
	/* the result names its own call handle */
	a->call = post_result_self(CB_REMOTE_FILE_READ_ASYNC, &r, sizeof(r));
	a->next = reads;
	reads = a;
	return a->call;
}

int8_t
rs_FileReadAsyncComplete(void *self, uint64_t call, void *out,
    uint32_t size)
{
	struct async_read **p, *a;

	UNUSED(self);
	for (p = &reads; (a = *p); p = &a->next)
		if (a->call == call) {
			if (size > a->size)
				size = a->size;
			memcpy(out, a->data, size);
			*p = a->next;
			free(a->data);
			free(a);
			return 1;
		}
	return 0;
}

/* ---- ISteamMatchmaking: no lobbies without Steam's servers ---- */

#pragma pack(push, 8)
struct lobby_created { int32_t result; uint64_t lobby; };
struct lobby_enter { uint64_t lobby; uint32_t perms; int8_t blocked; uint32_t response; };
#pragma pack(pop)

uint64_t
mm_RequestLobbyList(void *self)
{
	uint32_t n = 0;

	UNUSED(self);
	return post_result(CB_LOBBY_MATCH_LIST, &n, sizeof(n), 0);
}

uint64_t
mm_CreateLobby(void *self, uint32_t type, int32_t max)
{
	struct lobby_created r = { K_ERESULT_NO_CONNECTION, 0 };

	UNUSED(self);
	UNUSED(type);
	UNUSED(max);
	return post_result(CB_LOBBY_CREATED, &r, sizeof(r), 0);
}

uint64_t
mm_JoinLobby(void *self, uint64_t lobby)
{
	struct lobby_enter r;

	UNUSED(self);
	memset(&r, 0, sizeof(r));
	r.lobby = lobby;
	r.response = 2;		/* k_EChatRoomEnterResponseDoesntExist */
	return post_result(CB_LOBBY_ENTER, &r, sizeof(r), 0);
}

/*
 * ISteamUserStats with STEAMUSERSTATS_INTERFACE_VERSION013 and later has no
 * RequestCurrentStats(): Steam sends the stats when the game starts.
 */
void *
client_GetISteamUserStats(void *self, int32_t user, int32_t pipe,
    const char *version)
{
	UNUSED(self);
	UNUSED(user);
	UNUSED(pipe);
	stats_announce();
	return get_iface(version);
}

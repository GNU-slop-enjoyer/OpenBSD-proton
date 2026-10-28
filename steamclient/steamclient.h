/*
 * steamclient64.dll for proton-openbsd: an open implementation of the
 * Steam client interfaces that the Steamworks API (steam_api64.dll) of a
 * game talks to.  It answers for the account proton-launcher(1) logged in
 * with: its SteamID, persona name, the apps and DLC it owns, and keeps
 * user stats, achievements and Steam Cloud files in a local directory.
 * Online services (lobbies, P2P networking, workshop, leaderboards) are
 * not available and report failure, as Steam does when offline.
 *
 * Copyright (c) 2026 The OpenBSD Proton project contributors
 * Distributed under the LGPL 2.1 or later.
 */
#ifndef STEAMCLIENT_H
#define STEAMCLIENT_H

#include <stddef.h>
#include <stdint.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

struct iface_desc {
	const char	 *version;	/* "SteamUser023" */
	const char	 *iface;	/* "ISteamUser" */
	void *const	 *vtbl;
	int		  nslots;
};

extern const struct iface_desc iface_table[];

/* interface objects as the game sees them: a pointer to a vtable */
struct iface_obj {
	void *const		*vtbl;
	const struct iface_desc	*desc;
};

void	*get_iface(const char *version);

/* defaults for methods without an implementation (generated tables) */
uint64_t d_zero(void);
void	 d_void(void);
float	 d_float(void);
double	 d_double(void);
const char *d_str(void);
void	*d_ret2(void *, void *);
void	*d_ret8(void *, void *);
void	*d_ret16(void *, void *);
void	*d_ret20(void *, void *);
void	*d_ret40(void *, void *);
void	*iface_arg1(void *, const char *);
void	*iface_arg2(void *, int32_t, const char *);
void	*iface_arg3(void *, int32_t, int32_t, const char *);

/* account and game, from proton-launcher (see config_load()) */
struct config {
	uint32_t	 appid;
	uint64_t	 steamid;
	char		 name[128];
	char		 language[64];
	uint32_t	*apps;		/* owned apps, including DLC */
	size_t		 napps;
	uint32_t	*dlc;		/* this game's DLC that is owned */
	char	       **dlcname;
	size_t		 ndlc;
	int32_t		 buildid;
	char		 gamedir[MAX_PATH];	/* Windows path */
	char		 userdir[MAX_PATH];	/* stats and cloud files */
};

extern struct config cfg;

void	 config_load(void);
int	 owns_app(uint32_t);
void	 trace(const char *, ...);

/* callbacks and asynchronous call results */
void	 post_callback(int id, const void *data, int size);
uint64_t post_result(int id, const void *data, int size, int failed);
uint64_t post_result_self(int id, void *data, int size);
int	 call_completed(uint64_t call, int8_t *failed);
int	 get_call_result(uint64_t call, void *out, int size, int expected,
	    int8_t *failed);

/* callback ids (k_iSteam*Callbacks + n) and their layouts on Win64 */
#define CB_STEAM_API_CALL_COMPLETED	703
#define CB_USER_STATS_RECEIVED		1101
#define CB_USER_STATS_STORED		1102
#define CB_USER_ACHIEVEMENT_STORED	1103
#define CB_LEADERBOARD_FIND_RESULT	1104
#define CB_NUMBER_OF_CURRENT_PLAYERS	1107
#define CB_GLOBAL_ACH_PERCENTAGES_READY	1110
#define CB_ENCRYPTED_APP_TICKET		154
#define CB_GET_AUTH_SESSION_TICKET	163
#define CB_GET_TICKET_FOR_WEB_API	168
#define CB_VALIDATE_AUTH_TICKET		143
#define CB_STORE_AUTH_URL		165
#define CB_REMOTE_FILE_WRITE_ASYNC	1331
#define CB_REMOTE_FILE_READ_ASYNC	1332
#define CB_LOBBY_MATCH_LIST		510
#define CB_LOBBY_CREATED		513
#define CB_LOBBY_ENTER			504

#define K_ERESULT_OK		1
#define K_ERESULT_FAIL		2
#define K_ERESULT_NO_CONNECTION	3
#define K_ERESULT_FILE_NOT_FOUND 9

#endif

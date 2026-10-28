/*
 * Steam account operations on top of the CM connection: Steam Guard
 * login, PICS product info, depot keys and content server lookups.
 *
 * Copyright (c) 2026 The OpenBSD Proton project contributors
 * Distributed under the LGPL 2.1 or later.
 */
#ifndef STEAM_H
#define STEAM_H

#include "cm.h"
#include "kv.h"

/* Interactive login; returns 0 and fills account/refresh token. */
int	steam_auth(struct cm *, const char *account, char **refresh_token,
	    char **account_out);

/*
 * Connect and log on with the saved token ($DATA/login), or anonymously
 * when anon is set.  Dies with a helpful message on failure.
 */
void	steam_session(struct cm *, int anon);

/* PICS: app info (text KeyValues root "appinfo") for each appid. */
struct appinfo {
	uint32_t	 appid;
	struct kv	*kv;
};
int	steam_app_info(struct cm *, const uint32_t *, size_t,
	    struct appinfo **, size_t *);
/* Package info for the licenses of the session (binary KeyValues). */
struct pkginfo {
	uint32_t	 packageid;
	struct kv	*kv;
};
int	steam_package_info(struct cm *, struct pkginfo **, size_t *);

int	steam_depot_key(struct cm *, uint32_t app, uint32_t depot,
	    uint8_t key[32]);
int	steam_manifest_code(struct cm *, uint32_t app, uint32_t depot,
	    uint64_t manifest, const char *branch, uint64_t *code);
/* Content servers; NULL-terminated list of "https://host" strings. */
char	**steam_cdn_servers(struct cm *);
int	steam_free_license(struct cm *, uint32_t app);

#endif

/*
 * SteamPipe depot manifests and chunk downloads.
 *
 * Copyright (c) 2026 The OpenBSD Proton project contributors
 * Distributed under the LGPL 2.1 or later.
 */
#ifndef DEPOT_H
#define DEPOT_H

#include "util.h"

struct chunk {
	uint8_t		sha[20];	/* SHA-1 of the uncompressed data */
	uint64_t	offset;
	uint32_t	size;		/* uncompressed */
	uint32_t	csize;
};

struct mfile {
	char		*name;		/* '/' separated, relative */
	uint64_t	 size;
	uint32_t	 flags;
	uint8_t		 sha[20];
	char		*link;
	struct chunk	*chunks;
	size_t		 nchunks;
};

#define DEPOT_FLAG_EXECUTABLE	32
#define DEPOT_FLAG_DIRECTORY	64
#define DEPOT_FLAG_SYMLINK	512

struct manifest {
	uint32_t	 depot;
	uint64_t	 gid;
	struct mfile	*files;
	size_t		 nfiles;
	uint64_t	 total;
};

int	manifest_fetch(char **servers, uint32_t depot, uint64_t gid,
	    uint64_t code, const uint8_t key[32], struct manifest *);
void	manifest_free(struct manifest *);
/* Download every file of m into dir, verifying and resuming. */
int	depot_download(char **servers, const struct manifest *,
	    const uint8_t key[32], const char *dir, int threads);

#endif

/*
 * Copyright (c) 2026 The OpenBSD Proton project contributors
 * Distributed under the LGPL 2.1 or later.
 */
#ifndef CRYPTO_H
#define CRYPTO_H

#include "util.h"

/* RSA PKCS#1 v1.5 with a hex modulus/exponent, base64 result. */
char	*rsa_encrypt_b64(const char *mod_hex, const char *exp_hex,
	    const char *plain);
/*
 * Steam's symmetric encryption: a 16-byte IV encrypted with AES-256-ECB,
 * then AES-256-CBC with PKCS#7 padding (SteamKit2 CryptoHelper).
 */
int	steam_decrypt(const uint8_t key[32], const uint8_t *, size_t,
	    struct buf *);
void	sha1(const void *, size_t, uint8_t out[20]);

#endif

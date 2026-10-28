/*
 * Copyright (c) 2026 The OpenBSD Proton project contributors
 * Distributed under the LGPL 2.1 or later.
 */
/* The RSA_* calls exist in LibreSSL and OpenSSL; OpenSSL 3 deprecates them. */
#define OPENSSL_SUPPRESS_DEPRECATED

#include <string.h>

#include <openssl/bn.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/sha.h>

#include "crypto.h"

char *
rsa_encrypt_b64(const char *mod_hex, const char *exp_hex, const char *plain)
{
	BIGNUM *n = NULL, *e = NULL;
	RSA *rsa;
	uint8_t *out;
	char *b64 = NULL;
	int len;

	if (BN_hex2bn(&n, mod_hex) == 0 || BN_hex2bn(&e, exp_hex) == 0) {
		BN_free(n);
		BN_free(e);
		return NULL;
	}
	rsa = RSA_new();
	RSA_set0_key(rsa, n, e, NULL);
	out = xmalloc(RSA_size(rsa));
	len = RSA_public_encrypt(strlen(plain), (const uint8_t *)plain, out,
	    rsa, RSA_PKCS1_PADDING);
	if (len > 0)
		b64 = b64_encode(out, len);
	free(out);
	RSA_free(rsa);
	return b64;
}

int
steam_decrypt(const uint8_t key[32], const uint8_t *in, size_t n,
    struct buf *out)
{
	EVP_CIPHER_CTX *c;
	uint8_t iv[16 + 16];
	int l1 = 0, l2 = 0, ok = 0;
	size_t start;

	if (n < 32 || (n - 16) % 16)
		return -1;
	if ((c = EVP_CIPHER_CTX_new()) == NULL)
		return -1;
	if (EVP_DecryptInit_ex(c, EVP_aes_256_ecb(), NULL, key, NULL) != 1)
		goto out;
	EVP_CIPHER_CTX_set_padding(c, 0);
	if (EVP_DecryptUpdate(c, iv, &l1, in, 16) != 1 ||
	    EVP_DecryptFinal_ex(c, iv + l1, &l2) != 1 || l1 + l2 != 16)
		goto out;
	if (EVP_DecryptInit_ex(c, EVP_aes_256_cbc(), NULL, key, iv) != 1)
		goto out;
	EVP_CIPHER_CTX_set_padding(c, 1);
	start = out->len;
	buf_put(out, NULL, 0);
	if (out->cap < start + n) {
		out->p = xrealloc(out->p, start + n + 16);
		out->cap = start + n + 16;
	}
	if (EVP_DecryptUpdate(c, out->p + start, &l1, in + 16, n - 16) != 1 ||
	    EVP_DecryptFinal_ex(c, out->p + start + l1, &l2) != 1)
		goto out;
	out->len = start + l1 + l2;
	ok = 1;
out:
	EVP_CIPHER_CTX_free(c);
	return ok ? 0 : -1;
}

void
sha1(const void *p, size_t n, uint8_t out[20])
{
	SHA1(p, n, out);
}

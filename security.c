/*
 * Copyright 1990-1996 Chris Eleveld
 * Copyright 1992 Robert Slaven
 * Copyright 1992-2024 Jillian Alana Bolton
 * Copyright 1992-1995 David P. Mott
 *
 * The BSD 2-Clause License
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *     2. Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* security.c

  Routines for supporting haven adminstration.

 Chris Eleveld      (The Insane Hermit)
 Robert Slaven      (The Celestial Knight/Tigger)
 Jillian Alana Bolton  (Creel)
 Dave Mott          (Energizer Rabbit)

*/

#if defined(__APPLE__)
#include <sys/random.h>
#endif

#include <assert.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <string.h>
#include <unistd.h>

#include "db.h"
#include "log.h"
#include "lorien.h"
#include "newplayer.h"
#include "parse.h"
#include "platform.h"
/* .shutdown is available to level 4 and higher. */
#include "commands.h"
#include "security.h"

parse_error
haven_shutdown(struct splayer *pplayer)
{
	snprintf(sendbuf, sendbufsz,
	    "Shutting down to shut down command by %s from %s.", pplayer->name,
	    pplayer->host);
	logmsg(sendbuf);
	snprintf(sendbuf, sendbufsz,
	    ">> Shutting down to shut down command by %s from %s.\r\n",
	    pplayer->name, pplayer->host);
	sendall(sendbuf, ALL, 0);
	ldb_close(&lorien_db);
	exit(0);
	return PARSE_OK; /* not reached */
}

/* generates a sha512-suitable salt, e.g. "$6$0123456789ABCDEF$" */
int
generate_sha512_salt(char *buf, size_t sz)
{
	const char prefix[] = "$6$";
	char entropy[12]; /* 96 bits */
	int rc;
	size_t l = strlen(prefix);

	/* if entropy size is not multiple of 3, the salt would be padded */
	assert((sizeof(entropy) % 3) == 0);

	/* ensure enough space for:
	 * - 3 byte prefix
	 * - sizeof(entropy)
	 * - a NUL character
	 * - a trailing '$' after the entropy bits
	 */
	assert(sz >= 21);

	rc = getentropy((void *)entropy, sizeof(entropy));
	if (rc != 0)
		return -1;

	l = strlcpy(buf, prefix, sz);
	if (l != strlen(prefix))
		return -1;

	rc = EVP_EncodeBlock((void *)(buf + l), (void *)entropy,
	    sizeof(entropy));

	if (rc != (sizeof(entropy) * 4 / 3))
		return -1;

	/* 20 == length of a sha512-suitable salt including all three '$' */
	assert(strlcat(buf, "$", sz) == 20);

	return 0;
}

static const EVP_MD *hashfunc = NULL;
static size_t hashsz = 0;

int
init_security(void)
{
	if (!hashfunc) {
		OpenSSL_add_all_digests();
		hashfunc = EVP_get_digestbyname("SHA512");
		if (hashfunc)
			hashsz = EVP_MD_size(hashfunc);

		assert((20 + hashsz) <= MAX_PASS);
	}

	return (hashfunc) ? 0 : -1;
}

int
mkpasswd(char *buf, size_t sz, const char *key)
{
	char salt[21] = { 0 };
	char hashed[MAX_PASS];

	if (init_security() != 0)
		return -1;

	int rc = generate_sha512_salt(salt, sizeof(salt));
	if (rc != 0) {
		return -1;
	}

	assert(salt[0] == '$');
	assert(salt[2] == '$');
	assert(salt[19] == '$');
	assert(sz >= (20 + hashsz));

	rc = hashpass(hashed, sizeof(hashed), key, salt);
	strlcpy(buf, hashed, sz);

	return rc;
}

/* returns 0 if the guess matches, 1 if it doesn't,
 * and -1 if the hash couldn't be computed.
 */
int
ckpasswd(const char *authstr, const char *guess)
{
	char salt[21] = { 0 };
	char hashed[MAX_PASS];
	int rc;

	if (init_security() != 0)
		return -1;

	strlcpy(salt, authstr, sizeof(salt));
	assert(salt[0] == '$');
	assert(salt[2] == '$');
	assert(salt[19] == '$');

	rc = hashpass(hashed, sizeof(hashed), guess, salt);
	if (rc != 0)
		return -1;

	if (strcmp(hashed, authstr))
		return 1;

	return 0;
}

int
hashpass(char *out, size_t sz, const char *key, const char *salt)
{
	int rc = -1;
	EVP_MD_CTX *ctx = NULL;
	unsigned char *outhash = NULL;
	unsigned int len = 0;

	assert(key);
	assert(salt);
	assert(out);
	assert(strlen(salt) == 20);
	assert(sz >= (20 + hashsz));

	ctx = EVP_MD_CTX_new();
	if (!ctx)
		goto out;

	if (!EVP_DigestInit_ex(ctx, hashfunc, NULL))
		goto out;

	if (!EVP_DigestUpdate(ctx, &salt[3], 16))
		goto out;

	if (!EVP_DigestUpdate(ctx, key, strlen(key)))
		goto out;

	strlcpy(out, salt, 21);

	outhash = OPENSSL_malloc(hashsz);
	if (!outhash)
		goto out;

	if (!EVP_DigestFinal_ex(ctx, outhash, &len)) {
		ERR_print_errors_fp(stderr);
		goto out;
	}

	len = EVP_EncodeBlock((void *)&out[20], outhash, len);

	if (sz <= len + 20)
		goto out;

	out[len + 20] = (char)0;

	rc = 0;

out:
	if (outhash)
		OPENSSL_free(outhash);
	if (ctx)
		EVP_MD_CTX_free(ctx);

	return rc;
}

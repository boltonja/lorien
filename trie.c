/*
 * Copyright 2008-2025 Bolton-Dormer Research Partnership
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

/* trie.c - routines to implement a trie with a variety of search algorithms
 * Jillian Alana Bolton
 *
 */

#include <assert.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/resource.h>

#include "trie.h"

trie *
trie_new(void)
{
	return calloc(1, sizeof(trie));
}

/* given a root, find the first terminator
 * does not check the payload of the supplied root,
 * it only checks the leaves.
 */
trie *
trie_find_first(trie *root)
{
	trie *leaf;
	int i;

	if (!root)
		return NULL;

	for (i = 0; i < TRIE_SPAN; i++) {
		if (root->leaves[i] && root->leaves[i]->payload)
			return root->leaves[i];
		else {
			if (root->leaves[i]) {
				leaf = trie_find_first(root->leaves[i]);
				if (leaf)
					return leaf;
			}
			continue;
		}
	}
	return NULL;
}

/* pass true to free the payload, too */
int
trie_collapse(trie *root, bool free_payloads)
{
	int i;

	if (!root)
		return 0;
	for (i = 0; i < TRIE_SPAN; i++) {
		trie_collapse(root->leaves[i], free_payloads);
		root->leaves[i] = NULL;
	}
	if (free_payloads) {
		free(root->payload);
		root->payload = NULL;
	}
	free(root);
	return 1;
}

/* trie_delete()
 *
 * returns 0 if the key was not found,
 * or 1 if it was found and successfully
 * deleted
 *
 */
int
trie_delete(trie *root, unsigned char *key, size_t ksz, bool free_payloads)
{
	if (!(root && key))
		return 0;

	if (!ksz) {
		if (root->payload) {
			if (free_payloads)
				free(root->payload);
			root->payload = NULL;
			return 1;
		} else
			return 0;
	}

	if (!root->leaves[*key])
		return 0;

	if (!trie_delete(root->leaves[*key], key + 1, ksz - 1, free_payloads))
		return 0;

	/* was this the last key under this node? */
	if (!trie_find_first(root->leaves[*key])) {
		trie_collapse(root->leaves[*key], free_payloads);
		root->leaves[*key] = NULL;
	}
	return 1;
}

/* trie_add()
 *
 * returns a pointer to the added terminator
 * allocates new leaves as needed
 *
 * if it cannot allocate a necessary leaf, returns
 * NULL and writes ENOMEM into status.
 *
 * if the terminator leaf already exists and ALREADY contains
 * a non-NULL terminator, it will not be overwritten.  instead,
 * trie_add() returns the leaf and writes EEXIST into status.
 *
 * Other success modes place 0 in status.
 *
 */
trie *
trie_add(trie *root, unsigned char *key, size_t ksz, void *payload,
	 int *status)
{
	if (!root)
		return NULL;

	if (!ksz) {
		/* end of key */
		if (root->payload) {
			if (status)
				*status = EEXIST;
			return root;
		} else {
			if (status)
				*status = 0;
			root->payload = payload;
			return root;
		}
	}

	if (!root->leaves[*key]) {
		root->leaves[*key] = trie_new();
		if (!root->leaves[*key]) {
			if (status)
				*status = ENOMEM;
			return NULL; /* couldn't get resource */
		}
	}
	return trie_add(root->leaves[*key], key + 1, ksz - 1, payload, status);
}

void *
trie_payload(trie *root)
{
	return (root) ? root->payload : NULL;
}

/* trie_preorder() performs pre-order traversal, controlled
 * by a function:
 *
 * int func(void *ctx, void *payload);
 *
 * For each terminal node, func() is called with the corresponding payload
 * and the supplied context.
 *
 * The traversal space can be restricted, if appropriate, to a band of the
 * keyspace using the lowfilt and hifilt parameters.
 *
 * As a special case, if the trie has no terminal nodes (or if root is NULL)
 * the traversal returns -1.
 *
 * The traversal terminates when all terminal nodes are exhausted, or when
 * func() returns 0.  In either case, the traversal returns the last value
 * returned by func().
 *
 */
int
trie_preorder(trie *root, void *ctx, int (*func)(void *, void *), int lowfilt,
    int hifilt)
{
	int i;
	int rc = -1;
	int frc = -1;

	if (!root)
		return rc;

	for (i = lowfilt; i <= hifilt; i++) {
		if (root->leaves[i]) {
			if (root->leaves[i]->payload) {
				frc = func(ctx,
					   (void *)root->leaves[i]->payload);
				if (!frc)
					return frc;
			}
			rc = trie_preorder(root->leaves[i], ctx, func, lowfilt,
			    hifilt);
			if (!rc)
				return rc;
		}
	}
	return (frc == -1) ? rc : frc;
}

/* this is the post-order traversal.  aside from the direction, its details
 * are the same as trie_preorder() above.
 */
int
trie_postorder(trie *root, void *ctx, int (*func)(void *, void *), int lowfilt,
    int hifilt)
{
	int i;
	int rc = -1;
	int frc = -1;

	if (!root)
		return rc;

	for (i = hifilt; i >= lowfilt; i--) {
		if (root->leaves[i]) {
			rc = trie_postorder(root->leaves[i], ctx, func, lowfilt,
			    hifilt);
			if (!rc)
				return rc;
			if (root->leaves[i]->payload) {
				frc = func(ctx,
				    (void *)root->leaves[i]->payload);
				if (!frc)
					return frc;
			}
		}
	}
	return (frc == -1) ? rc : frc;
}

/* given a root, find a sole terminator
 * does not check the payload of the supplied root,
 * it only checks the leaves.
 *
 * returns:
 * NULL if no matches
 * root if multiple matches
 * terminating leaf if and only if there is exactly one terminator.
 */
trie *
trie_find_only(trie *root)
{
	trie *cursor = root;
	trie *leaf = NULL;
	trie *otherleaf;
	int i;

	if (!root)
		return NULL;

	for (i = 0; i < 256; i++) {
		if (cursor->leaves[i] && cursor->leaves[i]->payload) {
			if (leaf)
				return root; /* more than one found */
			else
				leaf = cursor->leaves[i];
		} else {
			if (cursor->leaves[i]) {
				otherleaf = trie_find_only(cursor->leaves[i]);
				if (leaf && otherleaf)
					return root; /* more than one found */
				else if (otherleaf == cursor->leaves[i])
					return root;
				else
					leaf = otherleaf;
			}
		}
	}
	return leaf;
}

trie *
trie_get(trie *root, unsigned char *key, size_t ksz)
{
	/* the following code matches the exact case, cleanly */
	if (!(root && key))
		return NULL;

	if (!ksz)
		return (root->payload) ? root : NULL;

	if (!root->leaves[*key])
		return NULL;

	return trie_get(root->leaves[*key], key + 1, ksz - 1);
}

/* trie_match()
 *
 * matched is the number of characters in the key that matched
 * characters in the trie.
 * matched is zero if no characters matched, but the returned
 * pointer can still be non-NULL if a completion was found.
 * matched is undefined if the returned trie pointer is NULL.
 */
trie *
trie_match(trie *root, unsigned char *key, size_t ksz, size_t *matched,
	   int mode)
{
	trie *leaf;
	unsigned char blindk;
	size_t reclen = 0; /* recursed match len */
	size_t *matchlen;

	matchlen = (matched) ? matched : &reclen;

	*matchlen = 0;

	if (!(root && key))
		return NULL;

	if (!ksz) {
                /* end of supplied key, prefer exact match*/
		if (!root->payload && (mode & trie_keymatch_abbrev)) {
			/* try to complete an abbreviation */
			if (mode & trie_keymatch_ambiguous)
				return trie_find_first(root);
			else {
				leaf = trie_find_only(root);
				return (leaf == root) ? NULL : leaf;
			}
		} else
			return (root->payload) ? root : NULL;
	} else {
		leaf = NULL;
		if (root->leaves[*key])
			leaf = trie_match(root->leaves[*key], key + 1, ksz - 1,
					  &reclen, mode);

		if ((!leaf) && (mode & trie_keymatch_caseblind) &&
		    isalpha(*key)) {
			blindk = isupper(*key) ? tolower(*key) : toupper(*key);
			leaf = NULL;
			if (root->leaves[blindk])
				trie_match(root->leaves[blindk], key + 1, ksz - 1,
					   &reclen, mode);
		}
		if (leaf)
			*matchlen = 1 + reclen;
		else if (mode & trie_keymatch_substring) {
			/* prefer to consume the longest segment of key
			 * possible, therefore only match the current root if
			 * sub-matches fail.
			 */
			if (root->payload) {
				/* we don't update matchlen here because it
				 * would count the root twice */
				leaf = root;
			} else if (mode & trie_keymatch_abbrev) {
				/* try to complete an abbreviation */
				if (mode & trie_keymatch_ambiguous)
					return trie_find_first(root);
				else {
					leaf = trie_find_only(
					    root); /* returns root on multiple
						    */
					return (leaf == root) ? NULL : leaf;
				}
			}
		}
		return leaf;
	}

}

#ifdef TESTTRIE
char *keys[] = { "one", "two", "three", "four", "five", "six",
		 "sixteen", "seven", "seventy", "seventy-five",
		 "seventy-eight", "eight", "nine", "ten", NULL };
char *payloads[] = { "ONE", "TWO", "THREE", "FOUR", "FIVE", "SIX",
		     "SIXTEEN", "SEVEN", "SEVENTY", "SEVENTY-FIVE",
		     "SEVENTY-EIGHT", "EIGHT", "NINE", "TEN", NULL };

char *sorted12[] = {
	"EIGHT", "FIVE", "FOUR", "NINE", "ONE", "SEVEN", "SEVENTY",
	"SIX", "SIXTEEN", "TEN", "THREE", "TWO", NULL };

char *sorted14[] = {
	"EIGHT", "FIVE", "FOUR", "NINE", "ONE", "SEVEN", "SEVENTY",
	"SEVENTY-EIGHT", "SEVENTY-FIVE", "SIX", "SIXTEEN", "TEN", "THREE",
	"TWO", NULL };

int
nevercalled_cb(void *ctx, void *payload)
{
	assert(0);
	return 1; /* NOTREACHED */
}

static int stop_cb_ctx = 0;
static int twelve_cb_ctx = 0;
static int fourteen_cb_ctx = 0;

int
twelve_cb(void *ctx, void *payload)
{
	char *p = payload;
	int *cp = ctx;
	int rc;

	assert(p);
	assert(ctx == (void *)&twelve_cb_ctx);

	printf("element %d expected |%s| got |%s|\n", *cp, sorted12[*cp], p);
	rc = strcmp(sorted12[*cp], p);
	assert(!rc);

	(*cp)++;
	return *cp;
}

int
fourteen_cb(void *ctx, void *payload)
{
	char *p = payload;
	int *cp = ctx;
	int rc;

	assert(p);
	assert(ctx == (void *)&fourteen_cb_ctx);

	printf("element %d expected |%s| got |%s|\n", *cp, sorted14[*cp], p);
	rc = strcmp(sorted14[*cp], p);
	assert(!rc);

	(*cp)++;
	return *cp;
}
int
evlewt_cb(void *ctx, void *payload)
{
	char *p = payload;
	int *cp = ctx;
	int rc;

	assert(p);
	assert(ctx == (void *)&twelve_cb_ctx);

	printf("element %d expected |%s| got |%s|\n", *cp, sorted12[*cp], p);
	rc = strcmp(sorted12[*cp], p);
	assert(!rc);

	(*cp)--;
	return 1;
}

int
neetruof_cb(void *ctx, void *payload)
{
	char *p = payload;
	int *cp = ctx;
	int rc;

	assert(p);
	assert(ctx == (void *)&fourteen_cb_ctx);

	printf("element %d expected |%s| got |%s|\n", *cp, sorted14[*cp], p);
	rc = strcmp(sorted14[*cp], p);
	assert(!rc);

	(*cp)--;
	return 1;
}

int
stop_cb(void *ctx, void *payload)
{
	char *p = payload;
	int *cp = (int *)ctx;

	assert(p);
	assert(ctx == (void *)&stop_cb_ctx);
	(*cp)++;

	if (*cp == 6)
		return 0;

	return *cp;
}

int
main(int argc, char *argv[])
{
	trie *test_trie;
	trie *leaf;
	size_t matched;
	int rc;

	printf("new trie test...");
	fflush(stdout);
	test_trie = trie_new();
	assert(test_trie);
	printf(" passed\n");
	fflush(stdout);

	printf("empty trie get test...");
	fflush(stdout);
	leaf = trie_get(test_trie, (unsigned char *)keys[0], strlen(keys[0]));
	assert(leaf == NULL);
	printf(" passed\n");
	fflush(stdout);

	printf("empty trie preorder test...");
	fflush(stdout);
	rc = trie_preorder(test_trie, NULL, nevercalled_cb, 0, 0);
	assert(-1 == rc);
	printf(" passed\n");
	fflush(stdout);

	printf("trie population test...");
	fflush(stdout);
	for (int i = 0; keys[i] != NULL; i++) {
		leaf = trie_add(test_trie, (unsigned char *) keys[i],
				strlen(keys[i]), payloads[i], NULL);
		assert(leaf);
		assert(trie_payload(leaf) == (void *)payloads[i]);

		if (keys[i + 1]) {
			leaf = trie_get(test_trie,
					(unsigned char *)keys[i + 1],
					strlen(keys[i + 1]));
			assert(leaf == NULL);
		}
	}
	printf(" passed\n");
	fflush(stdout);

	printf("NULL trie preorder test...");
	fflush(stdout);
	rc = trie_preorder(NULL, NULL, nevercalled_cb, 0, 0);
	assert(-1 == rc);
	printf(" passed\n");
	fflush(stdout);

	printf("trie preorder bail test...");
	fflush(stdout);
	stop_cb_ctx = 0;
	rc = trie_preorder(test_trie, &stop_cb_ctx, stop_cb, 'A', 'z');
	assert(0 == rc);
	assert(6 == stop_cb_ctx);
	printf(" passed\n");
	fflush(stdout);

	printf("trie preorder narrow filter test...\n");
	fflush(stdout);
	twelve_cb_ctx = 0;
	rc = trie_preorder(test_trie, &twelve_cb_ctx, twelve_cb, 'E', 'y');
	assert(rc == twelve_cb_ctx);
	assert(12 == twelve_cb_ctx);
	printf(" passed\n");
	fflush(stdout);

	printf("trie preorder wide filter test...\n");
	fflush(stdout);
	fourteen_cb_ctx = 0;
	rc = trie_preorder(test_trie, &fourteen_cb_ctx, fourteen_cb, '-', 'y');
	assert(rc == fourteen_cb_ctx);
	assert(14 == fourteen_cb_ctx);
	printf(" passed\n");
	fflush(stdout);

	printf("trie postorder bail test...");
	fflush(stdout);
	stop_cb_ctx = 0;
	rc = trie_postorder(test_trie, &stop_cb_ctx, stop_cb, 'A', 'z');
	assert(0 == rc);
	assert(6 == stop_cb_ctx);
	printf(" passed\n");
	fflush(stdout);

	printf("trie postorder narrow filter test...\n");
	fflush(stdout);
	twelve_cb_ctx = 11;
	rc = trie_postorder(test_trie, &twelve_cb_ctx, evlewt_cb, 'E', 'y');
	assert(1 == rc);
	assert(-1 == twelve_cb_ctx);
	printf(" passed\n");
	fflush(stdout);

	printf("trie postorder wide filter test...\n");
	fflush(stdout);
	fourteen_cb_ctx = 13;
	rc = trie_postorder(test_trie, &fourteen_cb_ctx, neetruof_cb, '-', 'z');
	assert(1 == rc);
	assert(-1 == fourteen_cb_ctx);
	printf(" passed\n");
	fflush(stdout);

	printf("trie find first test...");
	fflush(stdout);
	leaf = trie_find_first(test_trie);
	assert(leaf);
	assert(leaf->payload);
	rc = strcmp(payloads[11], (char *)trie_payload(leaf));
	assert(!rc);
	printf(" passed\n");
	fflush(stdout);

	printf("trie exact match test...");
	fflush(stdout);
	leaf = trie_match(test_trie, (unsigned char *)"five", strlen("five"),
			  &matched, trie_keymatch_exact);
	assert(leaf);
	assert(leaf->payload);
	rc = strcmp((char *)trie_payload(leaf), "FIVE");
	assert(!rc);
	assert(4 == matched);
	printf(" passed\n");
	fflush(stdout);

	printf("trie exact match negative test...");
	fflush(stdout);
	leaf = trie_match(test_trie, (unsigned char *)"fiv", strlen("fiv"),
			  &matched, trie_keymatch_exact);
	assert(!leaf);
	/* contents of matched are undefined if leaf is null */
	assert(NULL == trie_payload(leaf));
	printf(" passed\n");
	fflush(stdout);

	printf("trie substring match test...");
	fflush(stdout);
	leaf = trie_match(test_trie, (unsigned char *)"seventy",
			  strlen("seventy"), &matched, trie_keymatch_substring);
	assert(leaf);
	assert(7 == matched);
	assert(trie_payload(leaf));
	rc = strcmp(trie_payload(leaf), "SEVENTY");
	assert(!rc);
	printf(" passed\n");
	fflush(stdout);

	printf("trie abbrev match negative test...");
	fflush(stdout);
	leaf = trie_match(test_trie, (unsigned char *)"seventy-",
			  strlen("seventy-"), &matched, trie_keymatch_abbrev);
	assert(!leaf);
	printf(" passed\n");
	fflush(stdout);

	printf("trie substring abbrev match test...");
	fflush(stdout);
	leaf = trie_match(test_trie, (unsigned char *)"sixt",
			  strlen("sixt"), &matched,
			  trie_keymatch_substring_abbrev);
	assert(leaf);
	assert(4 == matched);
	assert(trie_payload(leaf));
	rc = strcmp(trie_payload(leaf), "SIXTEEN");
	assert(!rc);
	printf(" passed\n");
	fflush(stdout);

	printf("trie substring first match test...");
	fflush(stdout);
	leaf = trie_match(test_trie, (unsigned char *)"seventy-",
			  strlen("seventy-"), &matched,
			  trie_keymatch_substring_first);
	assert(leaf);
	assert(8 == matched);
	assert(trie_payload(leaf));
	rc = strcmp(trie_payload(leaf), "SEVENTY-EIGHT");
	assert(!rc);
	printf(" passed\n");
	fflush(stdout);

	printf("trie delete test...");
	fflush(stdout);
	rc = trie_delete(test_trie, (unsigned char *)"six", strlen("six"), false);
	assert(1 == rc);
	printf(" passed\n");
	fflush(stdout);

	printf("trie delete negative test...");
	fflush(stdout);
	rc = trie_delete(test_trie, (unsigned char *)"six", strlen("six"), false);
	assert(0 == rc);
	printf(" passed\n");
	fflush(stdout);

	printf("trie collapse test...");
	fflush(stdout);
	rc = trie_collapse(test_trie, false);
	assert(1 == rc);
	printf(" passed\n");
	fflush(stdout);

	printf("trie find first after collapse test...");
	fflush(stdout);
	leaf = trie_find_first(test_trie);
	assert(NULL == leaf);
	printf(" passed\n");
	fflush(stdout);

	printf("trie collapse NULL test...");
	rc = trie_collapse(NULL, false);
	assert(0 == rc);
	printf(" passed\n");
	fflush(stdout);

	printf("all tests passed\n");

	return 0;
}
#endif

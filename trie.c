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

#include <sys/queue.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/time.h>

#include <assert.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>

#include "trie.h"

trie *
trie_new(void)
{
	return calloc(1, sizeof(trie));
}

static int
find_first_cb(void *ctx, trie *root)
{
	trie **p = (trie **)ctx;
	*p = root;
	return 0;
}

/* given a root, find the first terminator
 * does not check the payload of the supplied root,
 * it only checks the leaves.
 */
trie *
trie_find_first(trie *root)
{
	int rc;
	trie *first = NULL;

	if (!root)
		return NULL;

	rc = trie_preorder(root, &first, find_first_cb, 0, TRIE_SPAN);
	if (rc == -1)
		return NULL;
	return first;
}

/* pass true to free the payload, too */
int
trie_collapse(trie *root, bool free_payloads)
{
        int i;
	SLIST_HEAD(f, trie_node) fl = SLIST_HEAD_INITIALIZER(fl);
	trie *p = root;

	if (!root)
		return 0;

	do {
		for (i = 0; i < TRIE_SPAN; i++)
			if (p->leaves[i]) {
				SLIST_INSERT_HEAD(&fl, p->leaves[i], entries);
				p->leaves[i] = NULL;
			}

		if (free_payloads)
			free(p->payload);
		p->payload = NULL;

		free(p);
		p = NULL;
		if (!SLIST_EMPTY(&fl)) {
			p = SLIST_FIRST(&fl);
			SLIST_REMOVE_HEAD(&fl, entries);
		}
	} while (p);

	return 1;
}

/* trie_isempty()
 *
 */
bool
trie_isempty(trie *root)
{
	int i;

	if (trie_find_first(root))
		return false;

	for (i = 0; i< TRIE_SPAN; i++)
		if (root->leaves[i]) {
			errno = EEXIST;
			warn("found non-null leaf in empty trie");
		}

	return true;
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
	trie *parent;
	unsigned char pkey;

	if (!(root && key && ksz))
		return 0;

	do {
		parent = root;
		pkey = *key;
		root = root->leaves[*key++];
		if (!root)
			return 0;
	} while ((--ksz) > 0);

	if (!root->payload)
		return 0;

	if (free_payloads)
		free(root->payload);

	root->payload = NULL;

	if (trie_isempty(root)) {
		parent->leaves[pkey] = NULL;
		free(root);
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
trie_add(trie *root, unsigned char *key, size_t ksz, void *payload, int *status)
{
	trie *leaf;
	trie *parent = root;
	int mystat;

	if (!status)
		status = &mystat;

	if (!(root && key && ksz && payload))
		return NULL;

	do {
		leaf = parent->leaves[*key++];
	        if (leaf)
			parent = leaf;
		ksz--;
	} while (leaf && ksz);

	*status = 0;

	if (leaf) {
		if (ksz)
			err(EX_SOFTWARE, "search stopped before end of key");

		if (leaf->payload)
			*status = EEXIST;
		else
			leaf->payload = payload;

		return leaf;
	}

	ksz++;
	key--;
	trie *newleaves[ksz];

	for (int i = 0; i < ksz; i++) {
		newleaves[i] = trie_new();
		if (newleaves[i])
			continue;

		for (int j = 0; j < i; j++)
			free(newleaves[j]);
		*status = ENOMEM;
		return NULL;
	}

	for (int i = 0; i < ksz; i++) {
		leaf = newleaves[i];
		parent->leaves[key[i]] = leaf;
		parent = leaf;
	}

	leaf->payload = payload;

	return leaf;
}

void *
trie_payload(trie *root)
{
	return (root) ? root->payload : NULL;
}

/* trie_preorder() performs pre-order traversal, controlled
 * by a function:
 *
 * int func(void *ctx, struct trie_node *payload);
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
trie_preorder(trie *root, void *ctx, int (*func)(void *,trie *), int lowfilt,
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
				    (void *)root->leaves[i]);
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
trie_postorder(trie *root, void *ctx, int (*func)(void *, trie *), int lowfilt,
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
				    (void *)root->leaves[i]);
				if (!frc)
					return frc;
			}
		}
	}
	return (frc == -1) ? rc : frc;
}

struct find_only_ctx {
	size_t count;
	trie *root;
};

static int
find_only_cb(void *ctx, trie *root)
{
	struct find_only_ctx *cp = ctx;

	if (++cp->count > 1)
		return 0;

	cp->root = root;
	return 1;
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
	int rc;
	struct find_only_ctx ctx = { 0 };

	if (!root)
		return NULL;

	rc = trie_preorder(root, &ctx, find_only_cb, 0, TRIE_SPAN);
	if (rc == -1 || !ctx.root)
		return NULL;

	if (ctx.count > 1)
		return root;

	return ctx.root;
}

trie *
trie_get(trie *root, unsigned char *key, size_t ksz)
{
	if (!(root && key && ksz))
		return NULL;

	do {
		root = root->leaves[*key++];
		if (!root)
			return NULL;
	} while ((--ksz) > 0);

	if (root->payload)
		return root;

	return NULL;
}

/* trie_match()
 *
 * matched is the count of characters consumed from key
 * return value is NULL if:
 *    - no characters were matched
 *    - mode is other than trie_match_fuzzy and the key was not wholly consumed
 *    - mode is trie_match_autocomplete and multiple keys have values
 *    - no values exist below the consumed characters from key
 *    - mode is invalid, root is NULL, key is NULL, or ksz is zero
 */

trie *
trie_match(trie *root, unsigned char *key, size_t ksz, size_t *matched,
    enum trie_match_modes mode)
{
	size_t len = ksz;
	unsigned char *cp = key;
	trie *leaf = NULL;

	if (!(root && key && ksz) || (mode > trie_match_max))
		goto out;

	/* match initial part of key */

	for (leaf = root; len > 0; len--) {
		if (!leaf->leaves[*cp])
			break;
		leaf = leaf->leaves[*cp++];
	}

	/* no characters in key matched */
	if (len == ksz) {
		leaf = NULL;
		goto out;
	}

	/* exact match */
	if (!len && leaf->payload)
		goto out;

	if (mode == trie_match_fuzzy) {
		leaf = trie_find_first(leaf);
		if (!leaf) {
			errno = ENOENT;
			warn("trie_match_fuzzy: no values beyond partial key");
		}
		goto out;
	}

	/* all characters must be consumed for ambiguous or abbrev match */
	if (len) {
		leaf = NULL;
		goto out;
	}

	if (mode == trie_match_ambiguous)
		leaf = trie_find_first(leaf);
	else { // mode == trie_match_autocomplete
		trie *found = trie_find_only(leaf);
		leaf = (found == leaf) ? NULL : found;
	}

out:
	if (matched)
		*matched = cp - key;
	return leaf;
}

#ifdef TESTTRIE
char *keys[] = { "one", "two", "three", "four", "five", "six", "sixteen",
	"seven", "seventy", "seventy-five", "seventy-eight", "eight", "nine",
	"ten", NULL };
char *payloads[] = { "ONE", "TWO", "THREE", "FOUR", "FIVE", "SIX", "SIXTEEN",
	"SEVEN", "SEVENTY", "SEVENTY-FIVE", "SEVENTY-EIGHT", "EIGHT", "NINE",
	"TEN", NULL };

char *sorted12[] = { "EIGHT", "FIVE", "FOUR", "NINE", "ONE", "SEVEN", "SEVENTY",
	"SIX", "SIXTEEN", "TEN", "THREE", "TWO", NULL };

char *sorted14[] = { "EIGHT", "FIVE", "FOUR", "NINE", "ONE", "SEVEN", "SEVENTY",
	"SEVENTY-EIGHT", "SEVENTY-FIVE", "SIX", "SIXTEEN", "TEN", "THREE",
	"TWO", NULL };

int
nevercalled_cb(void *ctx, trie *root)
{
	assert(0);
	return 1; /* NOTREACHED */
}

static int stop_cb_ctx = 0;
static int twelve_cb_ctx = 0;
static int fourteen_cb_ctx = 0;

int
twelve_cb(void *ctx, trie *root)
{
	char *p = trie_payload(root);
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
fourteen_cb(void *ctx, trie *root)
{
	char *p = trie_payload(root);
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
evlewt_cb(void *ctx, trie *root)
{
	char *p = trie_payload(root);
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
neetruof_cb(void *ctx, trie *root)
{
	char *p = trie_payload(root);
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
stop_cb(void *ctx, trie *root)
{
	char *p = trie_payload(root);
	int *cp = (int *)ctx;

	assert(p);
	assert(ctx == (void *)&stop_cb_ctx);
	(*cp)++;
	printf("element %d |%s|\n", *cp, p);

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
		leaf = trie_add(test_trie, (unsigned char *)keys[i],
		    strlen(keys[i]), payloads[i], NULL);
		assert(leaf);
		assert(trie_payload(leaf) == (void *)payloads[i]);

		if (keys[i + 1]) {
			leaf = trie_get(test_trie, (unsigned char *)keys[i + 1],
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

	printf("trie get test...");
	fflush(stdout);
	leaf = trie_get(test_trie, (unsigned char *)"five", strlen("five"));
	assert(leaf);
	assert(leaf->payload);
	rc = strcmp((char *)trie_payload(leaf), "FIVE");
	assert(!rc);
	printf(" passed\n");
	fflush(stdout);

	printf("trie get negative test...");
	fflush(stdout);
	leaf = trie_get(test_trie, (unsigned char *)"fiv", strlen("fiv"));
	assert(!leaf);
	assert(NULL == trie_payload(leaf));
	printf(" passed\n");
	fflush(stdout);

	printf("trie exact match (autocomplete) test...");
	fflush(stdout);
	matched = 0;
	leaf = trie_match(test_trie, (unsigned char *)"six", strlen("six"),
	    &matched, trie_match_autocomplete);
	assert(leaf);
	assert(leaf->payload);
	rc = strcmp((char *)trie_payload(leaf), "SIX");
	assert(!rc);
	assert(3 == matched);
	printf(" passed\n");
	fflush(stdout);

	printf("trie exact match (ambiguous) test...");
	fflush(stdout);
	matched = 0;
	leaf = trie_match(test_trie, (unsigned char *)"six", strlen("six"),
	    &matched, trie_match_ambiguous);
	assert(leaf);
	assert(leaf->payload);
	rc = strcmp((char *)trie_payload(leaf), "SIX");
	assert(!rc);
	assert(3 == matched);
	printf(" passed\n");
	fflush(stdout);

	printf("trie exact match (fuzzy) test...");
	fflush(stdout);
	matched = 0;
	leaf = trie_match(test_trie, (unsigned char *)"six", strlen("six"),
	    &matched, trie_match_ambiguous);
	assert(leaf);
	assert(leaf->payload);
	rc = strcmp((char *)trie_payload(leaf), "SIX");
	assert(!rc);
	assert(3 == matched);
	printf(" passed\n");
	fflush(stdout);

	printf("trie autocomplete match test...");
	fflush(stdout);
	matched = 0;
	leaf = trie_match(test_trie, (unsigned char *)"seventy",
			  strlen("seventy"), &matched, trie_match_autocomplete);
	assert(leaf);
	assert(7 == matched);
	assert(trie_payload(leaf));
	rc = strcmp(trie_payload(leaf), "SEVENTY");
	assert(!rc);
	printf(" passed\n");
	fflush(stdout);

	printf("trie autocomplete match negative test...");
	fflush(stdout);
	matched = 0;
	leaf = trie_match(test_trie, (unsigned char *)"seventy-",
	    strlen("seventy-"), &matched, trie_match_autocomplete);
	assert(!leaf);
	assert(8 == matched);

	matched = 0;
	leaf = trie_match(test_trie, (unsigned char *)"seventy-BLAH",
			  strlen("seventy-BLAH"), &matched, trie_match_autocomplete);
	assert(!leaf);
	assert(8 == matched);
	printf(" passed\n");
	fflush(stdout);

	printf("trie ambiguous match test...");
	fflush(stdout);
	matched = 0;
	leaf = trie_match(test_trie, (unsigned char *)"sixt", strlen("sixt"),
	    &matched, trie_match_ambiguous);
	assert(leaf);
	assert(4 == matched);
	assert(trie_payload(leaf));
	rc = strcmp(trie_payload(leaf), "SIXTEEN");
	assert(!rc);
	printf(" passed\n");
	fflush(stdout);

	printf("trie ambiguous match negative test...");
	fflush(stdout);
	matched = 0;
	leaf = trie_match(test_trie, (unsigned char *)"sixtBLAH", strlen("sixtBLAH"),
	    &matched, trie_match_ambiguous);
	assert(!leaf);
	assert(4 == matched);
	printf(" passed\n");
	fflush(stdout);

	printf("trie fuzzy match test...");
	fflush(stdout);
	matched = 0;
	leaf = trie_match(test_trie, (unsigned char *)"seventy-",
	    strlen("seventy-"), &matched, trie_match_fuzzy);
	assert(leaf);
	assert(8 == matched);
	assert(trie_payload(leaf));
	rc = strcmp(trie_payload(leaf), "SEVENTY-EIGHT");
	assert(!rc);

	matched = 0;
	leaf = trie_match(test_trie, (unsigned char *)"seventy-BLAH",
	    strlen("seventy-BLAH"), &matched, trie_match_fuzzy);
	assert(leaf);
	assert(8 == matched);
	assert(trie_payload(leaf));
	rc = strcmp(trie_payload(leaf), "SEVENTY-EIGHT");
	assert(!rc);
	printf(" passed\n");
	fflush(stdout);

	printf("trie fuzzy match negative test...");
	fflush(stdout);
	matched = 0;
	leaf = trie_match(test_trie, (unsigned char *)"BLAH",
	    strlen("BLAH"), &matched, trie_match_fuzzy);
	assert(!leaf);
	assert(0 == matched);
	printf(" passed\n");
	fflush(stdout);

	printf("trie delete test...");
	fflush(stdout);
	rc = trie_delete(test_trie, (unsigned char *)"six", strlen("six"),
	    false);
	assert(1 == rc);
	printf(" passed\n");
	fflush(stdout);

	printf("trie delete negative test...");
	fflush(stdout);
	rc = trie_delete(test_trie, (unsigned char *)"six", strlen("six"),
	    false);
	assert(0 == rc);
	printf(" passed\n");
	fflush(stdout);

	printf("trie collapse test...");
	fflush(stdout);
	rc = trie_collapse(test_trie, false);
	assert(1 == rc);
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

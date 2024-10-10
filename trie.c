/*
 * Copyright 2008-2024 Bolton-Dormer Research Partnership
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
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

/* pass 1 to free the payload, too */
int
trie_collapse(trie *root, int free_payloads)
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

/* returns 0 if the key was not found,
 * or 1 if it was found and successfully
 * deleted
 */
int
trie_delete(trie *root, unsigned char *key, int free_payloads)
{
	int found;

	if (!(root && key))
		return 0;

	if (!(*key)) {
		if (root->payload) {
			if (free_payloads)
				free(root->payload);
			root->payload = NULL;
			return 1;
		} else
			return 0;
	} else {
		found = (root->leaves[*key]) ?
		    trie_delete(root->leaves[*key], key + 1, free_payloads) :
		    0;
		if (found) {
			if (!trie_find_first(root->leaves[*key])) {
				trie_collapse(root->leaves[*key],
				    free_payloads);
				root->leaves[*key] = NULL;
			}
		}
		return found;
	}
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
trie_add(trie *root, unsigned char *key, void *payload, int *status)
{
	if (!root)
		return NULL;

	if (*key) {
		if (!root->leaves[*key]) {
			root->leaves[*key] = trie_new();
			if (!root->leaves[*key]) {
				if (status)
					*status = ENOMEM;
				return NULL; /* couldn't get resource */
			}
		}
		return trie_add(root->leaves[*key], key + 1, payload, status);
	} else { /* end of key */
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
}

trie *
trie_payload(trie *root)
{
	return (root) ? root->payload : root;
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

	if (!root)
		return rc;

	for (i = lowfilt; i < hifilt; i++) {
		if (root->leaves[i]) {
			if (root->leaves[i]->payload) {
				rc = func(ctx,
				    (void *)root->leaves[i]->payload);
				if (!rc)
					return rc;
			}
			rc = trie_preorder(root->leaves[i], ctx, func, lowfilt,
			    hifilt);
			if (!rc)
				return rc;
		}
	}
	return rc;
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

	if (!root)
		return rc;

	for (i = hifilt - 1; i >= lowfilt; i--) {
		if (root->leaves[i]) {
			rc = trie_postorder(root->leaves[i], ctx, func, lowfilt,
			    hifilt);
			if (!rc)
				return rc;
			if (root->leaves[i]->payload) {
				rc = func(ctx,
				    (void *)root->leaves[i]->payload);
				if (!rc)
					return rc;
			}
		}
	}
	return rc;
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

/* matched is the number of characters in the key that matched
 * characters in the trie.
 * matched is zero if no characters matched, but the returned
 * pointer can still be non-NULL if a completion was found.
 * matched is undefined if the returned trie pointer is NULL.
 */
trie *
trie_match(trie *root, unsigned char *key, size_t *matched, int mode)
{
	trie *leaf;
	unsigned char blindk;
	size_t reclen = 0; /* recursed match len */
	size_t *matchlen;

	matchlen = (matched) ? matched : &reclen;

	*matchlen = 0;

	if (!(root && key))
		return NULL;

	if (!(*key)) /* end of supplied key, prefer exact match*/
		if (!root->payload && (mode & trie_keymatch_abbrev)) {
			/* try to complete an abbreviation */
			if (mode & trie_keymatch_ambiguous)
				return trie_find_first(root);
			else {
				leaf = trie_find_only(
				    root); /* returns root on multiple */
				return (leaf == root) ? NULL : leaf;
			}
		} else
			return (root->payload) ? root : NULL;
	else {
		leaf = (root->leaves[*key]) ?
		    trie_match(root->leaves[*key], key + 1, &reclen, mode) :
		    NULL;
		if ((!leaf) && (mode & trie_keymatch_caseblind) &&
		    isalpha(*key)) {
			blindk = isupper(*key) ? tolower(*key) : toupper(*key);
			leaf = (root->leaves[blindk]) ?
			    trie_match(root->leaves[blindk], key + 1, &reclen,
				mode) :
			    NULL;
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

#if 0
	/* the following code matches the exact case, cleanly */
    if (!(root && key))
	return NULL;

    if (!(*key))
	return (root->payload) ? root : NULL;
    else 
	return (root->leaves[*key]) ? 
	    trie_match(root->leaves[*key], key+1, trie_keymatch_exact) : NULL;
#endif
}

#ifdef TESTTRIE
int
main(int argc, char *argv[])
{
	FILE *keyfile;
	trie *test_trie;
	trie *leaf;
	unsigned char key[2048];
	unsigned char *keyp;
	unsigned char *nlptr;
	size_t count = 0;
	size_t len;

	if (argc <= 1) {
		printf("usage: %s keyfile\n", argv[0]);
		exit(1);
	}

	keyfile = fopen(argv[1], "r");

	if (!keyfile) {
		printf("couldn't open keyfile %s\n", argv[1]);
		exit(1);
	}

	test_trie = trie_new();

	if (!test_trie) {
		printf("couldn't allocate initial trie node\n");
	}

	while (fgets((char *)key, sizeof(key), keyfile)) {
		keyp = malloc(strlen((char *)key) + 1);
		if (!keyp) {
			printf(
			    "truncated load, couldn't allocate space for payload %s\n",
			    key);
			break;
		}
		if ((nlptr = (unsigned char *)strchr((char *)key, '\n')))
			*nlptr = (unsigned char)0;
		strcpy((char *)keyp, (char *)key);
		leaf = trie_add(test_trie, (unsigned char *)key, keyp, NULL);
		if (!leaf) {
			printf(
			    "truncated load, couldn't allocate space for trie leaf %s\n",
			    key);
			free(keyp);
			break;
		}
		count++;
		if (feof(keyfile))
			break;
	}

	fclose(keyfile);

	printf("loaded %lu keys\n", count);
	fflush(stdout);

	for (;;) {
		printf("enter key: ");
		fflush(stdout);
		if (fgets((char *)key, sizeof(key), stdin)) {
			nlptr = (unsigned char *)strchr((char *)key, '\n');
			if (nlptr)
				*nlptr = (unsigned char)0;
			printf("\nsearching %s...\n", key);
			len = 0;
			leaf = trie_match(test_trie, key, &len,
			    trie_keymatch_exact);
			printf("exact match: (%lu)%s\n", len,
			    (leaf) ? (char *)leaf->payload : "not found");
			len = 0;
			leaf = trie_match(test_trie, key, &len,
			    trie_keymatch_substring_first);
			printf("abbrev ambig substring match: (%lu) %s\n", len,
			    (leaf) ? (char *)leaf->payload : "not found");
			len = 0;
			len = 0;
			leaf = trie_match(test_trie, key, &len,
			    trie_keymatch_substring_first |
				trie_keymatch_caseblind);
			printf(
			    "caseblind abbrev ambig substring match: (%lu) %s\n",
			    len, (leaf) ? (char *)leaf->payload : "not found");
			leaf = trie_match(test_trie, key, &len,
			    trie_keymatch_substring_abbrev);
			printf("abbrev Unambig substring match: (%lu) %s\n",
			    len, (leaf) ? (char *)leaf->payload : "not found");
			/* additional match tests here */
			fflush(stdout);
		}

		if (feof(stdin))
			break;
	}

	trie_collapse(test_trie, 1);
}
#endif

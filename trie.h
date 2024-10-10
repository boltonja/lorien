/*
 * Copyright 2008-2024, Bolton-Dormer Research Partnership
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

/* trie.h - routines to implement a trie with a variety of search algorithms
 * Jillian Alana Bolton
 */

#ifndef _TRIE_H_
#define _TRIE_H_

#include <sys/types.h>

#define TRIE_SPAN 128

struct trie_node {
	struct trie_node *leaves[TRIE_SPAN];
	void *payload;
};

typedef struct trie_node trie;

enum trie_match_modes {		 /* for constructing bitmasks */
	trie_keymatch_exact = 0, /* special case, no options set means exact */
	trie_keymatch_ambiguous =
	    1, /* bit set, ambiguous.  clear, unambiguous */
	trie_keymatch_abbrev =
	    2, /* attempt to auto-complete abbreviated keys */
	trie_keymatch_substring =
	    4, /* match initial portion of key, for commands w/o lexical
		  separation from their parameters */
	trie_keymatch_caseblind = 8,
	/* not implemented, returned pointer is a NULL terminated array of
	 * completions which must be free'd by caller
	 */
	trie_keymatch_completions = 16,
	/* not implemented, match substrings anywhere w/in stored key */
	trie_keymatch_innersubstring = 32,
	/* derived masks. */
	/* first of any ambiguous abbreviation, consuming all of key */
	trie_keymatch_first = trie_keymatch_ambiguous | trie_keymatch_abbrev,
	/* first of any ambiguous abbreviation, in initial portion */
	trie_keymatch_substring_first = trie_keymatch_first |
	    trie_keymatch_substring,
	/* start of key is unambiguous abbrev */
	trie_keymatch_substring_abbrev = trie_keymatch_substring |
	    trie_keymatch_abbrev
};

#if __STDC__

trie *trie_new(void);

/* returns 0 if the key was not found,
 * or 1 if it was found and successfully
 * deleted
 */
int trie_delete(trie *root, unsigned char *key, int free_payloads);

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
 * trie_add() returns NULL and writes EEXIST into status.
 *
 */
trie *trie_add(trie *root, unsigned char *key, void *payload, int *status);

trie *trie_payload(trie *root);

/* given a root, find the first terminator */
trie *trie_find_first(trie *root);

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
int trie_preorder(trie *root, void *ctx, int (*func)(void *, void *),
    int lowfilt, int hifilt);

/* this is the post-order traversal.  aside from the direction, its details
 * are the same as trie_preorder() above.
 */
int trie_postorder(trie *root, void *ctx, int (*func)(void *, void *),
    int lowfilt, int hifilt);
/* matched is the number of characters in the key that matched
 * characters in the trie.
 * matched is zero if no characters matched, but the returned
 * pointer can still be non-NULL if a completion was found.
 * matched is undefined if the returned trie pointer is NULL.
 */

trie *trie_match(trie *root, unsigned char *key, size_t *matched, int mode);

/* pass 1 to free the payload, too */
int trie_collapse(trie *root, int free_payloads_too);

#else
trie *trie_new();

int trie_delete();

trie *trie_add();

trie *trie_payload();

trie *trie_find_first();

trie *trie_match();

int trie_collapse();

int trie_postorder();

int trie_preorder();

#endif /* __STD_C__ */
#endif /* _TRIE_H_ */

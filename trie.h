/*
 * Copyright 2008-2025, Bolton-Dormer Research Partnership
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

#include <stdbool.h>
#include <sys/types.h>

#define TRIE_SPAN 256

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

trie *trie_add(trie *root, unsigned char *key, size_t keysz, void *payload,
	       int *status);
int trie_collapse(trie *root, bool free_payloads);
int trie_delete(trie *root, unsigned char *key, size_t ksz, bool free_payloads);
trie *trie_find_first(trie *root);
trie *trie_get(trie *root, unsigned char *key, size_t ksz);
trie *trie_match(trie *root, unsigned char *key, size_t keysz, size_t *matched,
		 int mode);
trie *trie_new(void);
void *trie_payload(trie *root);
int trie_preorder(trie *root, void *ctx, int (*func)(void *, void *),
    int lowfilt, int hifilt);
int trie_postorder(trie *root, void *ctx, int (*func)(void *, void *),
    int lowfilt, int hifilt);

#endif /* _TRIE_H_ */

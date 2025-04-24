/*
 * Copyright 1990-1996 Chris Eleveld
 * Copyright 1992 Robert Slaven
 * Copyright 1992-2025 Jillian Alana Bolton
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

/* board.c - bulletin board capability for Lorien
 */

#include "board.h"
#include "msg.h"
#include "lorien.h"
#include "newplayer.h"
#include "parse.h"
#include "platform.h"
#include "trie.h"

/* add a new message to the board in memory */
int
msg_add(struct msg *msg) {
	struct trie_node *leaf;
	struct trie_node *root;

	if (!msg)
		return MSGERR_INVAL;

	if (!msg->parent) {
		if (!msg->board->threads) {
			msg->board->threads = trie_new();
			if (!msg->board->threads)
				return MSGERR_NOMEM;
		}
		root = msg->board->threads;
	} else {
		if (!msg->parent->threads) {
			msg->parent->threads = trie_new();
			if (!msg->parent->threads)
				return MSGERR_NOMEM;
		}
		root = msg->parent->threads;
	}
			
	leaf = trie_add(root, (void *)&msg->key, sizeof(msg->key), msg, NULL);
	if (!leaf)
		return MSGERR_NOMEM;

	return 0;
}

/* persist a new message to the db and add it to the board in memory */
int
msg_mk(struct msg *msg)
{
	int rc;
	struct timeval now;

	if (!msg)
		return MSGERR_INVAL;

	usleep(1); /* force usec to monotonically increase */
	rc = gettimeofday(&now, NULL);
	if (!rc)
		return MSGERR_INVAL;

	msg->key.created = now.tv_sec;
	msg->key.created_usec = now.tv_usec;

	if (!ldb_msg_put(&lorien_db, msg))
		return MSGERR_DBFAIL;

	rc = msg_add(msg);
	if (rc)
		ldb_msg_delete(&lorien_db, msg);

	return rc;
}

/* remove a single message from db
 * returns:
 *  0                on success
 *  MSGERR_DBFAIL    on database delete failure
 *  MSGERR_NOTFOUND  if msg is not in msglog
 *  MSGERR_THREADED  if msg contains a non-empty msglog (i.e, it has threads)
 *  MSGERR_CORRUPT   unable to delete the msg from the underlying container
 *
 */
int
msg_rm(struct msg *msg)
{
	struct trie_node *leaf;
	struct trie_node *root;
	int rc;

	if (!msg)
		return MSGERR_INVAL;

	if (msg->parent)
		root = msg->parent->threads;
	else
		root = msg->board->threads;

	leaf = trie_get(root, (void *)&msg->key, sizeof(msg->key));
	if (!leaf)
		return MSGERR_NOTFOUND;

	if (msg->threads) {
		leaf = trie_find_first(msg->threads);
		if (leaf)
			return MSGERR_THREADED;
		trie_collapse(msg->threads, false);
	}

	if (!ldb_msg_delete(&lorien_db, msg))
		return MSGERR_DBFAIL;

	rc = trie_delete(root, (void *)&msg->key, sizeof(msg->key), true);
	if (!rc)
		return MSGERR_CORRUPT;

	return 0;
}

struct msg *
msg_new(struct board *board, struct msg *parent, const char *owner,
	const char *subj, size_t subjsz, const char *text,
	size_t textsz)
{
	struct msg *msg;
	size_t sz;

	if (!board || !owner || !subj || !text)
		return NULL;

	sz = sizeof(*msg) + subjsz + textsz;
	msg = calloc(1, sz);
	if (!msg)
		return NULL;

	msg->subjsz = subjsz;
	msg->textsz = textsz;
	msg->subj = msg->data;
	msg->text = msg->data + subjsz;

	msg->board = board;
	msg->parent = parent;

	strlcpy(msg->owner, owner, sizeof(msg->owner));
	strlcpy(msg->subj, subj, subjsz);
	strlcpy(msg->text, text, textsz);

	return msg;
}

int
msg_free(struct msg *msg)
{
	struct trie_node *leaf;

	if (!msg)
		return MSGERR_NOTFOUND;

	if (msg->threads) {
		leaf = trie_find_first(msg->threads);
		if (leaf)
			return MSGERR_THREADED;
		trie_collapse(msg->threads, false);
	}

	return 0;
}

int
msg_scan_cb(struct ldb_msg *m)
{
	struct board *board;
	struct msg *parent;

	if (!m)
		return MSGERR_INVAL;

	board = board_get(m->board);
	if (!board)
		return MSGERR_NOTFOUND;

	if (m->parent_created) {
	}
}

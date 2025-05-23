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

/* msg.c - messages (mailbox, bulletin board, etc)
 *
 *    |board| -> |board| -> |board|
 *                   |
 *       (parent)  X | (threads)
 *                 | v
 *                |msg1| <-> |msg2| <-> |msg3|
 *                 ^ |
 *       (parent)  | | (threads)
 *                 | v
 *                 |msg4|
 *
 * (continues multilevel ad infinitum)
 *
 */

#ifdef TESTMSG
#include <assert.h>
#endif
#include <err.h>
#include <sysexits.h>

#include "board.h"
#include "lorien.h"
#include "msg.h"
#include "parse.h"
#include "platform.h"
#ifdef TESTMSG
#include "servsock_ssl.h"
#endif
#include "trie.h"

struct trie_node msgindex = { 0 };

static int
msgindex_add(struct msg *msg)
{
	struct trie_node *leaf;

	if (!msg)
		return MSGERR_INVAL;

	leaf = trie_add(&msgindex, (void *)&msg->key, sizeof(msg->key), msg,
	    NULL);
	if (!leaf)
		return MSGERR_NOMEM;

	return 0;
}

static int
msgindex_del(struct msg *msg)
{
	int rc;

	if (!msg)
		return MSGERR_INVAL;

	rc = trie_delete(&msgindex, (void *)&msg->key, sizeof(msg->key), false);

	return (rc == 1) ? 0 : MSGERR_NOTFOUND;
}

static struct msg *
msgindex_find(struct msgkey *key)
{
	struct trie_node *leaf;
	if (!key)
		return NULL;

	leaf = trie_get(&msgindex, (void *)key, sizeof(*key));
	if (!leaf)
		return NULL;

	return (void *)trie_payload(leaf);
}

/* add a new message to the board in memory */
int
msg_add(struct msg *msg)
{
	int rc;

	if (!msg || !msg->board)
		return MSGERR_INVAL;

	if (msg->parent && !msg->parent->board)
		return MSGERR_INVAL;

	rc = msgindex_add(msg);
	if (rc)
		return rc;

	if (!msg->parent)
		TAILQ_INSERT_TAIL(&msg->board->threads, msg, entries);
	else
		TAILQ_INSERT_TAIL(&msg->parent->threads, msg, entries);

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

	if (!msg->board)
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
	if (!msg)
		return MSGERR_INVAL;

	if (!msg->board)
		return MSGERR_INVAL;

	if (!msgindex_find(&msg->key))
		return MSGERR_NOTFOUND;

	if (!TAILQ_EMPTY(&msg->threads))
		return MSGERR_THREADED;

	if (!ldb_msg_delete(&lorien_db, msg))
		return MSGERR_DBFAIL;

	msgindex_del(msg);

	if (msg->parent)
		TAILQ_REMOVE(&msg->parent->threads, msg, entries);
	else
		TAILQ_REMOVE(&msg->board->threads, msg, entries);

	return 0;
}

struct msg *
msg_new(struct board *board, struct msg *parent, const char *owner,
    const char *subj, size_t subjsz, const char *text, size_t textsz)
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
	TAILQ_INIT(&msg->threads);

	strlcpy(msg->owner, owner, sizeof(msg->owner));
	strlcpy(msg->subj, subj, subjsz);
	strlcpy(msg->text, text, textsz);

	return msg;
}

int
msg_free(struct msg *msg)
{
	if (!msg)
		return MSGERR_INVAL;

	if (!TAILQ_EMPTY(&msg->threads))
		return MSGERR_THREADED;

	free(msg);
	return 0;
}

static int msgs_read_from_db = 0;

int
msg_scan_cb(struct ldb_msg *m)
{
	struct board *board;
	struct msg *parent;
	struct msgkey parentkey;
	struct msgkey key;
	struct msg *msg;
	char *subj, *text;
	size_t subjsz, textsz;
	int rc;

	if (!m)
		return MSGERR_INVAL;

	switch (m->board_type) {
	case LDB_BOARD_BULLETIN:
		board = board_get(m->board);
		break;
	case LDB_BOARD_CHANNEL:
	case LDB_BOARD_MBOX:
	default:
		err(EX_DATAERR, "message for unimplemented board type %d",
		    m->board_type);
	}

	if (!board)
		return MSGERR_NOTFOUND;

	if (m->parent_created || m->parent_created_usec) {
		parentkey.created = m->parent_created;
		parentkey.created_usec = m->parent_created_usec;

		parent = msgindex_find(&parentkey);
		if (!parent)
			return MSGERR_NOTFOUND;
	}

	key.created = m->key.created;
	key.created_usec = m->key.created_usec;
	if (msgindex_find(&key))
		return MSGERR_CORRUPT;

	subj = &m->data[0];
	text = &m->data[m->subjsz];
	subjsz = strnlen(subj, m->subjsz);
	textsz = strnlen(text, m->textsz);
	msg = msg_new(board, parent, m->owner, subj, subjsz, text, textsz);
	if (!msg)
		return MSGERR_NOMEM;

	rc = msgindex_add(msg);
	if (rc) {
		msg_free(msg);
		return rc;
	}

	rc = msg_add(msg);
	if (rc) {
		msgindex_del(msg);
		msg_free(msg);
		return rc;
	}

	return 0;
}

int
msg_read_db(void)
{
	msgs_read_from_db = 0;
	int rc;

	rc = ldb_msg_scan(&lorien_db, msg_scan_cb);
	if (rc != 0)
		err(EX_SOFTWARE, "failed to scan msgs from db");

	return msgs_read_from_db;
}

#ifdef TESTMSG

int
sendtoplayer(struct splayer *who, char *buf)
{
	printf("%s", buf);
	return 0;
}

int
main(void)
{
	int rc;
	struct servsock_handle h = { 0 };
	struct splayer p = { 0 };
	struct msg *mp;
	struct msg parent = { 0 };
	struct msgkey key = { 0 };
	struct msg child = { 0 };
	struct board *board1 = NULL;
	struct board *board2 = NULL;

	rc = msgindex_add(NULL);
	assert(MSGERR_INVAL == rc);

	rc = msgindex_del(NULL);
	assert(MSGERR_INVAL == rc);

	mp = msgindex_find(NULL);
	assert(NULL == mp);

	parent.key.created = 1;
	parent.key.created_usec = 1;

	rc = msgindex_add(&parent);
	assert(MSG_SUCCESS == rc);

	memcpy(&key, &parent.key, sizeof(key));

	mp = msgindex_find(&key);
	assert(&parent == mp);

	key.created_usec = 2;
	mp = msgindex_find(&key);
	assert(NULL == mp);

	memcpy(&child.key, &key, sizeof(child.key));

	rc = msgindex_del(&child);
	assert(MSGERR_NOTFOUND == rc);

	child.key.created_usec = 1;
	rc = msgindex_del(&child);
	assert(MSG_SUCCESS == rc);

	mp = msgindex_find(&child.key);
	assert(NULL == mp);

	rc = msgindex_del(&parent);
	assert(MSGERR_NOTFOUND == rc);

	h.sock = 1;
	p.h = &h;

	strlcpy(lorien_db.dbname, "./testmsg.db", sizeof(lorien_db.dbname));

	rc = ldb_open(&lorien_db);
	assert(!rc);

	rc = board_read_db();
	assert(0 == rc);

	rc = msg_read_db();
	assert(0 == rc);

	rc = board_add("announcements", "hermit", "important stuff for all",
	    LDB_BOARD_BULLETIN, time(NULL), true);
	assert(BOARD_SUCCESS == rc);

	rc = board_add("bug reports", "valkyrie", "please include good repro",
	    LDB_BOARD_BULLETIN, time(NULL), true);
	assert(BOARD_SUCCESS == rc);

	board1 = board_get("announcements");
	assert(NULL != board1);

	board2 = board_get("bug reports");
	assert(NULL != board2);

	struct msg *thread1 = NULL;
	struct msg *thread2 = NULL;
	// struct msg *t1a = NULL, *t1b = NULL;
	// struct msg *t2a = NULL, *t2b = NULL;;

	thread1 = msg_new(board1, NULL, "hermit", "new version", 11,
	    "its live see release notes for details", 38);
	assert(NULL != thread1);

	thread2 = msg_new(board2, NULL, "valkyrie", "unicode bug", 11,
	    "crappy bug report here", 22);
	assert(NULL != thread2);

	rc = msg_mk(thread1);
	assert(MSG_SUCCESS == rc);

	rc = msg_mk(thread2);
	assert(MSG_SUCCESS == rc);

	memcpy(&key, &thread1->key, sizeof(key));
	mp = msgindex_find(&key);
	assert(mp == thread1);

	memcpy(&key, &thread2->key, sizeof(key));
	mp = msgindex_find(&key);
	assert(mp == thread2);

	printf("all tests passed.\n");
}
#endif

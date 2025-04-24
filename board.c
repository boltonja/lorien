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

#include <assert.h>
#include <stdbool.h>

#include "board.h"
#include "lorien.h"
#include "newplayer.h"
#include "parse.h"
#include "platform.h"
#ifdef TESTBOARD
#include "servsock_ssl.h"
#endif

SLIST_HEAD(boardlist, board) boardhead = SLIST_HEAD_INITIALIZER(boardhead);

static int boards_read_from_db = 0;

int
board_read_cb(struct board *board)
{
	boards_read_from_db++;
	return board_add(board->name, board->owner, board->desc, board->type,
	    board->created, false);
}

int
board_read_db(void)
{
	boards_read_from_db = 0;
	(void)ldb_board_scan(&lorien_db, board_read_cb);
	return boards_read_from_db;
}

int
board_add(const char *name, const char *owner, const char *desc,
    ldb_board_type type, time_t created, bool save_board)
{
	struct board *curr = malloc(sizeof(struct board));

	if (!curr)
		return BOARDERR_NOMEM;

	switch (type) {
	case LDB_BOARD_BULLETIN:
	case LDB_BOARD_CHANNEL:
	case LDB_BOARD_MBOX:
		break;
	default:
		free(curr);
		return BOARDERR_INVALID;
	}

	TAILQ_INIT(&curr->threads);
	strlcpy(curr->name, name, sizeof(curr->name));
	strlcpy(curr->owner, owner, sizeof(curr->owner));
	strlcpy(curr->desc, desc, sizeof(curr->desc));
	curr->created = created;
	curr->type = type;

	if (save_board) {
		int rc = ldb_board_put(&lorien_db, curr);
		if (rc != 0) {
			free(curr);
			curr = NULL;
			return BOARDERR_DBFAIL;
		}
	}
	SLIST_INSERT_HEAD(&boardhead, curr, entries);

	return 0;
}

struct board *
board_get(const char *name)
{
	struct board *curr = NULL;
	int found = 0;

	SLIST_FOREACH(curr, &boardhead, entries)
		if (!strncmp(curr->name, name, sizeof(curr->name) - 1)) {
			found = 1;
			break;
		}

	return (found) ? curr : NULL;
}

int
board_remove(const char *name)
{
	struct board *curr;
	int rc;

	curr = board_get(name);
	if (!curr)
		return BOARDERR_NOTFOUND;

	if (!TAILQ_EMPTY(&curr->threads))
		return BOARDERR_NOTEMPTY;

	rc = ldb_board_delete(&lorien_db, curr);
	if (rc)
		return BOARDERR_NOTFOUND;

	SLIST_REMOVE(&boardhead, curr, board, entries);
	return 0;
}

parse_error
board_list(struct splayer *who)
{
	char sendbuf[OBUFSIZE];
	char timbuf[50];
	struct board *curr = NULL;
	char *nl;

	snprintf(sendbuf, sizeof(sendbuf), ">> Bulletin Boards:\r\n");
	sendtoplayer(who, sendbuf);

	SLIST_FOREACH(curr, &boardhead, entries) {
		ctime_r(&curr->created, timbuf);
		nl = index(timbuf, '\n');
		if (nl)
			*nl = (char)0;
		snprintf(sendbuf, sizeof(sendbuf),
		    ">> Board: %s\r\n>>   Created: %s\r\n>>   Owner: %s\r\n"
		    ">>   Description: %s\r\n",
		    curr->name, timbuf, curr->owner, curr->desc);
		sendtoplayer(who, sendbuf);
	}

	return PARSE_OK;
}

#ifdef TESTBOARD

const int testboards = 5;
const char *names[] = { "one", "two", "three", "four", "five" };
const ldb_board_type types[] = { LDB_BOARD_BULLETIN, LDB_BOARD_CHANNEL,
	LDB_BOARD_MBOX, LDB_BOARD_BULLETIN, LDB_BOARD_BULLETIN };
const char *owners[] = { "you", "me", "somebody else", "us", "them" };
const char *descs[] = { "red", "blue", "green", "purple", "yellow" };

int
sendtoplayer(struct splayer *who, char *buf)
{
	printf("%s", buf);
	return 0;
}

int
main(void)
{
	int i, rc;
	struct board *b;
	struct servsock_handle h = { 0 };
	struct splayer p = { 0 };

	b = board_get(NULL);
	assert(!b);

	b = board_get("moo");
	assert(!b);

	for (i = 0; i < testboards; i++) {
		rc = board_add(names[i], owners[i], descs[i], types[i],
		    time(NULL), false);
		assert(BOARD_SUCCESS == rc);
	}

	for (i = 0; i < testboards; i++) {
		b = board_get(names[i]);
		assert(b);
		assert(!strcmp(b->name, names[i]));
		assert(!strcmp(b->owner, owners[i]));
		assert(!strcmp(b->desc, descs[i]));
		assert(b->type == types[i]);
		assert(b->created != 0);
	}

	rc = board_add("moo", "beep", "indigo", 100, time(NULL), false);
	assert(rc == BOARDERR_INVALID);

	b = board_get("moo");
	assert(!b);

	h.sock = 1;
	p.h = &h;
	board_list(&p);

	for (b = SLIST_FIRST(&boardhead); b; b = SLIST_FIRST(&boardhead))
		if (b)
			SLIST_REMOVE_HEAD(&boardhead, entries);

	for (i = 0; i < testboards; i++) {
		b = board_get(names[i]);
		assert(!b);
	}

	strlcpy(lorien_db.dbname, "./testboard.db", sizeof(lorien_db.dbname));

	rc = ldb_open(&lorien_db);
	assert(!rc);

	rc = board_read_db();
	assert(0 == rc);

	for (i = 0; i < testboards; i++) {
		rc = board_add(names[i], owners[i], descs[i], types[i],
		    time(NULL), true);
		assert(BOARD_SUCCESS == rc);
	}

	b = board_get(names[0]);
	assert(b);
	assert(!strcmp(b->name, names[0]));

	for (b = SLIST_FIRST(&boardhead); b; b = SLIST_FIRST(&boardhead))
		if (b)
			SLIST_REMOVE_HEAD(&boardhead, entries);

	for (i = 0; i < testboards; i++) {
		b = board_get(names[i]);
		assert(!b);
	}

	rc = board_read_db();
	assert(testboards == rc);

	for (i = 0; i < testboards; i++) {
		b = board_get(names[i]);
		assert(b);
		assert(!strcmp(b->name, names[i]));
		assert(!strcmp(b->owner, owners[i]));
		assert(!strcmp(b->desc, descs[i]));
		assert(b->type == types[i]);
		assert(b->created != 0);
	}

	for (i = 0; i < testboards; i++) {
		rc = board_remove(names[i]);
		assert(BOARD_SUCCESS == rc);
		b = board_get(names[i]);
		assert(!b);
	}

	rc = board_read_db();
	assert(0 == rc);

	for (i = 0; i < testboards; i++) {
		b = board_get(names[i]);
		assert(!b);
	}

	rc = ldb_close(&lorien_db);
	assert(!rc);

	printf("all tests passed.\n");
}

#endif

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

#include <stdbool.h>

#include "board.h"
#include "lorien.h"
#include "newplayer.h"
#include "parse.h"
#include "platform.h"

SLIST_HEAD(boardlist, board) boardhead = SLIST_HEAD_INITIALIZER(boardhead);

static int boards_read_from_db = 0;

int
board_read_cb(struct board *board)
{
	boards_read_from_db++;
	return board_add(board->name, board->owner, board->desc, board->created,
	    false);
}

int
board_read_db(void)
{
	(void)ldb_board_scan(&lorien_db, board_read_cb);
	return boards_read_from_db;
}

int
board_add(const char *name, const char *owner, const char *desc, time_t created,
    bool save_board)
{
	struct board *curr = malloc(sizeof(struct board));

	if (!curr)
		return BOARDERR_NOMEM;

	TAILQ_INIT(&curr->threads);
	strlcpy(curr->name, name, sizeof(curr->name));
	strlcpy(curr->owner, owner, sizeof(curr->owner));
	strlcpy(curr->desc, desc, sizeof(curr->desc));
	curr->created = created;

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
	struct board *curr = NULL;

	snprintf(sendbuf, sizeof(sendbuf), ">> Bulletin Boards:\r\n");
	sendtoplayer(who, sendbuf);

	SLIST_FOREACH(curr, &boardhead, entries) {
		snprintf(sendbuf, sizeof(sendbuf),
		    ">> Owner: %s\r\n>> Board: %s\r\n>>        %s\r\n",
		    curr->owner, curr->name, curr->desc);
		sendtoplayer(who, sendbuf);
	}

	return PARSE_OK;
}

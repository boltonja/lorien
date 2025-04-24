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

/* board.h - bulletin board capability for Lorien.
 */
#ifndef _BOARD_H_
#define _BOARD_H_

#include <sys/queue.h>

#include <stdbool.h>

#include "db.h"
#include "msg.h"
#include "parse.h"

struct board {
	SLIST_ENTRY(board) entries;
	struct msglist threads;
	time_t created;
	ldb_board_type type;
	int flags;
	char name[LORIEN_V0174_NAME];
	char owner[LORIEN_V0174_NAME];
	char desc[LORIEN_V0178_DESC];
};

enum {
	BOARD_SUCCESS = 0,
	BOARDERR_DBFAIL,
	BOARDERR_NOMEM,
	BOARDERR_NOTEMPTY,
	BOARDERR_NOTFOUND,
};

int board_add(const char *name, const char *owner, const char *desc,
    time_t created, bool save_board);
struct board *board_get(const char *name);
parse_error board_list(struct splayer *who);
int board_read_db(void);
int board_remove(const char *s);

#endif

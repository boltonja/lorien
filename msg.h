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

/* msg.h - bulletin board capability for Lorien.
 */
#ifndef _MSG_H_
#define _MSG_H_

#include <sys/queue.h>

#include <stdbool.h>

#include "db.h"
#include "parse.h"
#include "trie.h"

struct msgkey {
	time_t created;
	int32_t created_usec;
};

struct board;

struct msg {
	TAILQ_ENTRY(msg) entries;
	TAILQ_HEAD(msglist, msg) threads;
	struct msgkey key;
	struct msg *parent;
	size_t textsz;
	size_t subjsz;
	ldb_board_type board_type;
	struct board *board;
	char owner[LORIEN_V0174_NAME];
	char *subj;
	char *text;
	char data[];
};

enum msgerr {
	MSG_SUCCESS = 0,
	MSGERR_CORRUPT,
	MSGERR_DBFAIL,
	MSGERR_INVAL,
	MSGERR_NOMEM,
	MSGERR_NOTFOUND,
	MSGERR_THREADED,
};

int msg_add(struct msg *msg);
int msg_free(struct msg *msg);
int msg_mk(struct msg *msg);
int msg_rm(struct msg *msg);
struct msg *msg_find(struct msgkey *key);
struct msg *msg_new(struct board *board, struct msg *parent, const char *owner,
    const char *subj, size_t subjsz, const char *text, size_t textsz);
#endif

/*
 * Copyright 2024,2025 Jillian Alana Bolton
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

#ifndef _LORIENDB_H_
#define _LORIENDB_H_

#include <lmdb.h>
#include <stdbool.h>
#include <stdint.h>

#define BUFSIZE	 2048	      /* the maximum line length to be recieved */
#define OBUFSIZE BUFSIZE + 80 /* bigger so formatting can occur. */
#define HNAMELEN 80	      /* maximum host name length */

/* sha512 hash encoded is 88 chars, plus 20 for the salt */
#define LORIEN_V0174_PASS 110
#define LORIEN_DBVERSION  0x00000174
#define LORIEN_V0174_NAME 50
#define LORIEN_V0174_CHAN 13
#define LORIEN_V0174_DESC 240

#define MAX_PASS LORIEN_V0174_PASS
#define MAX_NAME LORIEN_V0174_NAME
#define MAX_CHAN LORIEN_V0174_CHAN

typedef enum {
	LDB_BOARD,
	LDB_CHAN,
	LDB_MSG,
	LDB_PLAYER,
	LDB_MAX,
} ldb_type;

struct ldb_player {
	/* key */
	char name[LORIEN_V0174_NAME];

	/* data */
	char password[LORIEN_V0174_PASS]; /* BUG: encrypt with libssl */
	char host[LORIEN_V0174_NAME];
	int seclevel;
	int hilite;
	int privs;
	int wrap;
	int flags;
	int pagelen;	   /* for paginating, e.g., boards */
	time_t created;    /* creation time */
	time_t login;      /* NOT last seen, too many updates! */
};

struct ldb_chan {
	/* key */
	char name[LORIEN_V0174_CHAN];

	/* data */
	char owner[LORIEN_V0174_NAME];
	char desc[LORIEN_V0174_DESC];
	time_t created;
};

struct ldb_board {
	/* key */
	char name[LORIEN_V0174_NAME];

	/* data */
	char owner[LORIEN_V0174_NAME];
	time_t created;		    /* created */
	char desc[LORIEN_V0174_DESC];
};

struct ldb_msg {
	/* key */
	char board[LORIEN_V0174_NAME];
	char owner[LORIEN_V0174_NAME];
	char subj[LORIEN_V0174_NAME];

	/* data */
	time_t created;
	char text[BUFSIZE];
};

struct lorien_db {
	MDB_env *db;
	char dbname[LORIEN_V0174_NAME];
	MDB_dbi dbis[LDB_MAX];
};

extern struct lorien_db lorien_db;

extern const char *ldb_names[];

struct splayer;

int ldb_close(struct lorien_db *db);
int ldb_open(struct lorien_db *db);
int ldb_player_delete(struct lorien_db *db, struct splayer *player);
int ldb_player_get(struct lorien_db *db, const char *name, size_t namesz,
    struct splayer *player);
int ldb_player_put(struct lorien_db *db, struct splayer *player,
    bool nooverwrite);

#endif /* _LORIENDB_H_ */

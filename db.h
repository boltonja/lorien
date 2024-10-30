/*
 * Copyright 2024 Jillian Alana Bolton
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

#include "lorien.h"

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
	time_t playerwhen; /* creation time */
	time_t loginwhen;  /* NOT last seen, too many updates! */
};

struct ldb_chan {
	/* key */
	char name[MAX_CHAN + 1];
	/* data */
	time_t chanwhen;
};

struct ldb_board {
	/* key */
	char name[MAX_CHAN + 1];
	/* data */
	size_t boardquota;
	time_t boardwhen;		    /* created */
	char boardwhere[LORIEN_V0174_NAME]; /* name of db of many ldb_msg */
};

struct ldb_msg {
	/* key */
	time_t postwhen;
	/* data */
	char postwho[LORIEN_V0174_NAME]; /* from */
	char text[BUFSIZE];
};

struct lorien_db {
	MDB_env *db;
	char dbname[LORIEN_V0174_NAME];
	MDB_dbi dbis[LDB_MAX];
};

extern struct lorien_db lorien_db;

extern const char *ldb_names[];

int ldb_close(struct lorien_db *db);
int ldb_open(struct lorien_db *db);
int ldb_player_delete(struct lorien_db *db, struct splayer *player);
int ldb_player_get(struct lorien_db *db, const char *name, size_t namesz,
    struct splayer *player);
int ldb_player_put(struct lorien_db *db, struct splayer *player,
    bool nooverwrite);

#endif /* _LORIENDB_H_ */

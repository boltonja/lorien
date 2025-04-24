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

/* arbitrary limits informed by hysterical raisins (historical reasons) */
#define LORIEN_V0174_NAME 50
#define LORIEN_V0174_CHAN 13
#define LORIEN_V0178_DESC 240

/* sha512 hash encoded is 88 chars, plus 20 for the salt */
#define LORIEN_V0174_PASS 110

/* RFC 1035 says dns names are limited to 255 characters, we add a NUL */
#define LORIEN_V0178_BAN 256

#define MAX_PASS	 LORIEN_V0174_PASS
#define MAX_NAME	 LORIEN_V0174_NAME
#define MAX_CHAN	 LORIEN_V0174_CHAN

typedef enum {
	LDB_BOARD,
	LDB_UNUSED, /* formerly LDB_CHAN */
	LDB_MSG,
	LDB_PLAYER,
	LDB_BAN,
	LDB_MAX,
} ldb_type;

typedef enum {
	LDB_BOARD_BULLETIN, /* board is a bulletin board */
	LDB_BOARD_CHANNEL,  /* board is a persistent channel */
	LDB_BOARD_MBOX,	    /* board is a player mailbox */
} ldb_board_type;

typedef enum {
	LDB_BOARD_M_THREADED = 1, /* this board is threaded */
	LDB_BOARD_M_PERSIST = 2,  /* this boards messages persist on media */
} ldb_board_mask;

struct ldb_player {
	/* key */
	char name[LORIEN_V0174_NAME];

	/* data */
	char password[LORIEN_V0174_PASS];
	char host[LORIEN_V0174_NAME];
	int seclevel;
	int hilite;
	int privs;
	int wrap;
	int flags;
	int pagelen;	/* for paginating, e.g., boards */
	time_t created; /* creation time */
	time_t login;	/* NOT last seen, too many updates! */
};

struct ldb_board_key {
	char name[LORIEN_V0174_NAME];
	ldb_board_type type;
};

struct ldb_board {
	/* key */
	struct ldb_board_key key;

	/* data */
	time_t created; /* created */
	ldb_board_mask flags;
	char owner[LORIEN_V0174_NAME];
	char desc[LORIEN_V0178_DESC];
};

struct ldb_msg_key {
	time_t created;
	int32_t created_usec;
};

struct ldb_msg {
	/* key */
	struct ldb_msg_key key;

	/* metadata - key of in-thread parent */
	time_t parent_created;
	int32_t parent_created_usec;

	/* metadata - payload size, key of containing board, channel, or mbox */
	ldb_board_type board_type;
	size_t subjsz;
	size_t textsz;
	char board[LORIEN_V0174_NAME];
	char owner[LORIEN_V0174_NAME];

	/* data */
	char data[];
};

struct ldb_ban {
	/* key */
	char pattern[LORIEN_V0178_BAN];

	/* data */
	int flags;
	char owner[LORIEN_V0174_NAME];
	time_t created;
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

struct ban_item;

int ldb_ban_delete(struct lorien_db *db, struct ban_item *ban);
int ldb_ban_put(struct lorien_db *db, struct ban_item *ban);
int ldb_ban_scan(struct lorien_db *db, int (*banfunc)(struct ban_item *));

struct board;

int ldb_board_delete(struct lorien_db *db, struct board *board);
int ldb_board_put(struct lorien_db *db, struct board *board);
int ldb_board_scan(struct lorien_db *db, int (*boardfunc)(struct board *));

struct msg;

int ldb_msg_delete(struct lorien_db *db, struct msg *msg);
int ldb_msg_put(struct lorien_db *db, struct msg *msg);
int ldb_msg_scan(struct lorien_db *db, int (*msgfunc)(struct ldb_msg *));

#endif /* _LORIENDB_H_ */

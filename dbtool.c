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

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <time.h>

#include "ban.h"
#include "board.h"
#include "db.h"
#include "lorien.h"
#include "newplayer.h"
#include "security.h"
#include "servsock_ssl.h"

const char usage[] = "usage:\n"
		     "\tdbtool player (add|auth|update) <player> <password>\n"
	             "\tdbtool player get <player>\n"
	             "\tdbtool player list\n"
	             "\tdbtool player level <player> <level>\n"
	"\tdbtool ban (add|del) <pattern>\n"
	"\tdbtool ban list\n"
	"\tdbtool board add <name> <description>\n"
	"\tdbtool board del <name>\n"
	"\tdbtool board list\n";

size_t MAXCONN;
time_t lorien_boot_time;

void
handle_player(int argc, char *argv[], int argindex)
{
	struct splayer p;
	int rc;

	if (!strcmp("auth", argv[argindex])) {
		char *name = argv[argindex + 1];
		rc = ldb_player_get(&lorien_db, name, strlen(name), &p);
		if (rc == MDB_NOTFOUND) {
			errno = rc;
			warn(">> player %s not found", name);
		} else if (rc == 0) {
			rc = ckpasswd(p.password, argv[argindex + 2]);
			if (rc < 0)
				err(EX_IOERR, "can't hash password");
			else if (rc > 0)
				printf("passwords do not match\n");
			else
				printf("password accepted\n");
		} else {
			errno = rc;
			err(EX_IOERR, ">> can't read player %s", name);
		}

	} else if (!strcmp("get", argv[argindex])) {
		char *name = argv[argindex + 1];
		rc = ldb_player_get(&lorien_db, name, strlen(name), &p);
		if (rc == MDB_NOTFOUND) {
			errno = rc;
			warn(">> player %s not found", name);
		} else if (rc == 0) {
			assert(!strcmp(p.name, name));
			printf("player %s level %d password %s", p.name,
			       p.seclevel, p.password);
		} else {
			errno = rc;
			err(EX_IOERR, ">> can't read player %s", name);
		}
	} else if (!strcmp("list", argv[argindex])) {
		MDB_txn *txn;
		MDB_cursor *cursor;
		MDB_val key, data;

		rc = mdb_txn_begin(lorien_db.db, NULL, 0, &txn);
		if (rc != 0) {
			errno = rc;
			err(EX_IOERR, "can't start transaction");
		}
		rc = mdb_cursor_open(txn, lorien_db.dbis[LDB_PLAYER], &cursor);
		if (rc != 0) {
			errno = rc;
			err(EX_IOERR, "can't open cursor");
		}

		while (
		    (rc = mdb_cursor_get(cursor, &key, &data, MDB_NEXT)) == 0) {
			printf("key: %lu %.*s, data: %lu %.*s\n", key.mv_size,
			    (int)key.mv_size, (char *)key.mv_data, data.mv_size,
			    (int)data.mv_size, (char *)data.mv_data);
		}
		mdb_cursor_close(cursor);
		mdb_txn_abort(txn);
	} else if (!strcmp("add", argv[argindex])) {
		char *name = argv[argindex + 1];
		char *password = argv[argindex + 2];

		if (argc != 5) {
			errno = EINVAL;
			err(EX_USAGE, usage);
		}

		memset(&p, 0, sizeof(p));
		playerinit(&p, time((time_t *)0), "0.0.0.0", "0.0.0.0");

		strlcpy(p.name, name, sizeof(p.name));

		rc = mkpasswd(p.password, sizeof(p.password), password);
		if (rc) {
			errno = EINVAL;
			err(EX_IOERR, "can't hash password");
		}

		rc = ldb_player_put(&lorien_db, &p, true);
		if (rc == MDB_KEYEXIST) {
			errno = rc;
			warn("player %s already exists", name);
		} else if (rc != 0) {
			errno = rc;
			err(EX_IOERR, "cannot create player %s", name);
		}
	} else if (!strcmp("level", argv[argindex])) {
		char *name = argv[argindex + 1];
		char *level = argv[argindex + 2];
		int levnum = atoi(level);

		if (argc != 5) {
			errno = EINVAL;
			err(EX_USAGE, usage);
		}

		if ((levnum < JOEUSER) || (levnum >= NUMLVL)) {
			errno = EINVAL;
			err(EX_USAGE, "level is out of range");
		}

		playerinit(&p, 0, "1.1.1.1", "8.8.8.8");

		rc = ldb_player_get(&lorien_db, name,
		    strnlen(name, sizeof(p.name) - 1), &p);
		if (rc != 0) {
			errno = rc;
			err(EX_IOERR, "can't read player %s", name);
		}

		p.seclevel = levnum;

		rc = ldb_player_put(&lorien_db, &p, false);
		if (rc != 0) {
			errno = rc;
			err(EX_IOERR, "cannot update player %s", name);
		}
	} else if (!strcmp("update", argv[argindex])) {
		char *name = argv[argindex + 1];
		char *password = argv[argindex + 2];

		if (argc != 5) {
			errno = EINVAL;
			err(EX_USAGE, usage);
		}

		playerinit(&p, 0, "1.1.1.1", "8.8.8.8");

		rc = ldb_player_get(&lorien_db, name,
		    strnlen(name, sizeof(p.name) - 1), &p);
		if (rc != 0) {
			errno = rc;
			err(EX_IOERR, "can't read player %s", name);
		}

		rc = mkpasswd(p.password, sizeof(p.password), password);
		if (rc) {
			errno = EINVAL;
			err(EX_IOERR, "can't hash password");
		}

		rc = ldb_player_put(&lorien_db, &p, false);
		if (rc != 0) {
			errno = rc;
			err(EX_IOERR, "cannot update player %s", name);
		}
	} else {
		errno = EINVAL;
		err(EX_USAGE, usage);
	}
}

int
print_ban_cb(struct ban_item *ban)
{
	char buf[50];

	/* ctime_r(3) includes a newline */
	printf("ban: %s owner: %s created: %s", ban->pattern, ban->owner,
	       ctime_r(&ban->created, buf));
	return 1;
}

void
handle_ban(int argc, char *argv[], int argindex)
{
	struct ban_item ban = { 0 };
	int rc = 0;

	argc--;

	if (!strcmp("add", argv[argindex])) {
		if (argc != 3) {
			errno = EINVAL;
			err(EX_USAGE, usage);
		}
		strlcpy(ban.pattern, argv[++argindex], sizeof(ban.pattern));
		strlcpy(ban.owner, "dbtool", sizeof(ban.owner));
		ban.created = time((time_t *) NULL);
		rc = ldb_ban_put(&lorien_db, &ban);
		if (rc == MDB_KEYEXIST) {
			errno = rc;
			warn("ban %s already exists", ban.pattern);
		} else if (rc != 0) {
			errno = rc;
			err(EX_IOERR, "cannot create ban %s", ban.pattern);
		}
	} else if (!strcmp("delete", argv[argindex])) {
		if (argc != 3) {
			errno = EINVAL;
			err(EX_USAGE, usage);
		}
		strlcpy(ban.pattern, argv[++argindex], sizeof(ban.pattern));
		rc = ldb_ban_delete(&lorien_db, &ban);
		if (rc == MDB_NOTFOUND) {
			errno = rc;
			warn(">> ban %s not found", ban.pattern);
		} else if (rc != 0) {
			errno = rc;
			err(EX_IOERR, ">> can't delete ban %s", ban.pattern);
		}
	} else if (!strcmp("list", argv[argindex])) {
		if (argc != 2) {
			errno = EINVAL;
			err(EX_USAGE, usage);
		}
		ldb_ban_scan(&lorien_db, print_ban_cb);
	} else {
		errno = EINVAL;
		err(EX_USAGE, usage);
	}
}

int
print_board_cb(struct board *board)
{
	char buf[50];

	/* ctime_r(3) includes a newline */
	printf("board: %s owner: %s created: %sdesc: %s\n",
	       board->name, board->owner, ctime_r(&board->created, buf),
	       board->desc);
	return 1;
}

void
handle_board(int argc, char *argv[], int argindex)
{
	struct board board = { 0 };
	int rc = 0;

	argc--;

	if (!strcmp("add", argv[argindex])) {
		if (argc != 4) {
			errno = EINVAL;
			err(EX_USAGE, usage);
		}
		strlcpy(board.name, argv[++argindex], sizeof(board.name));
		strlcpy(board.owner, "dbtool", sizeof(board.owner));
		strlcpy(board.desc, argv[++argindex], sizeof(board.desc));
		board.created = time((time_t *) NULL);
		board.type = LDB_BOARD_BULLETIN;
		rc = ldb_board_put(&lorien_db, &board);
		if (rc == MDB_KEYEXIST) {
			errno = rc;
			warn("board %s already exists", board.name);
		} else if (rc != 0) {
			errno = rc;
			err(EX_IOERR, "cannot create board %s", board.name);
		}
	} else if (!strcmp("delete", argv[argindex])) {
//BUG: must require board type arg
		if (argc != 3) {
			errno = EINVAL;
			err(EX_USAGE, usage);
		}
		strlcpy(board.name, argv[++argindex], sizeof(board.name));
		rc = ldb_board_delete(&lorien_db, &board);
		if (rc == MDB_NOTFOUND) {
			errno = rc;
			warn(">> board %s not found", board.name);
		} else if (rc != 0) {
			errno = rc;
			err(EX_IOERR, ">> can't delete board %s", board.name);
		}
	} else if (!strcmp("list", argv[argindex])) {
		if (argc != 2) {
			errno = EINVAL;
			err(EX_USAGE, usage);
		}
		ldb_board_scan(&lorien_db, print_board_cb);
	} else {
		errno = EINVAL;
		err(EX_USAGE, usage);
	}
}

int
main(int argc, char *argv[])
{
	int rc;
	int argindex = 1;
	struct servsock_handle ssh = { 0 };

	printf("argc %d\n", argc);
	initplayerstruct();
	players->h = &ssh;
	ssh.sock = 1; /* stdout */

	if (argc < 2) {
		errno = EINVAL;
		err(EX_USAGE, usage);
	}

	strncpy(lorien_db.dbname, "./lorien.db", sizeof(lorien_db.dbname) - 1);
	lorien_db.dbname[sizeof(lorien_db.dbname) - 1] = (char)0;

	rc = ldb_open(&lorien_db);
	if (rc != 0) {
		errno = rc;
		err(EX_IOERR, "can't open db");
	}

	if (!strcmp("player", argv[argindex]))
		handle_player(argc, argv, ++argindex);
	else if (!strcmp("ban", argv[argindex]))
		handle_ban(argc, argv, ++argindex);
	else if (!strcmp("board", argv[argindex]))
		handle_board(argc, argv, ++argindex);
	else {
		errno = EINVAL;
		err(EX_USAGE, usage);
	}

	ldb_close(&lorien_db);
	exit(EXIT_SUCCESS);
}

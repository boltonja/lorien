#include <assert.h>
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <time.h>

#include "db.h"
#include "lorien.h"
#include "newplayer.h"

const char usage[] = "usage:\n"
		     "\tdbtool (update|add|get|list) <player> <password>\n";

size_t MAXCONN;
time_t lorien_boot_time;

int
main(int argc, char *argv[])
{
	struct splayer p;
	int rc;
	int argindex = 1;

	printf("argc %d\n", argc);
	initplayerstruct();
	players->s = 1; /* stdout */

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

	if (strcmp("player", argv[argindex])) {
		errno = EINVAL;
		err(EX_USAGE, usage);
	}

	argindex++;

	if (!strcmp("get", argv[argindex])) {
		char *name = argv[argindex + 1];
		rc = ldb_player_get(&lorien_db, name, strlen(name), &p);
		if (rc == MDB_NOTFOUND) {
			errno = rc;
			warn(">> player %s not found", name);
		} else if (rc == 0) {
			assert(!strcmp(p.name, name));
			printf("player %s password %s", p.name, p.password);
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

		if (argc != 4) {
			errno = EINVAL;
			err(EX_USAGE, usage);
		}

		memset(&p, 0, sizeof(p));
		playerinit(&p, time((time_t *)0), "0.0.0.0", "0.0.0.0");

		strncpy(p.name, name, sizeof(p.name) - 1);
		p.name[sizeof(p.name) - 1] = (char)0;

		strncpy(p.password, password, sizeof(p.password) - 1);
		p.password[sizeof(p.password) - 1] = (char)0;

		rc = ldb_player_put(&lorien_db, &p, true);
		if (rc == MDB_KEYEXIST) {
			errno = rc;
			warn("player %s already exists", name);
		} else if (rc != 0) {
			errno = rc;
			err(EX_IOERR, "cannot create player %s", name);
		}
	} else if (!strcmp("update", argv[argindex])) {
		char *name = argv[argindex + 1];
		char *password = argv[argindex + 2];

		if (argc != 4) {
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

		strncpy(p.password, password, sizeof(p.password) - 1);
		p.password[sizeof(p.password) - 1] = (char)0;

		rc = ldb_player_put(&lorien_db, &p, false);
		if (rc != 0) {
			errno = rc;
			err(EX_IOERR, "cannot update player %s", name);
		}
	} else {
		errno = EINVAL;
		err(EX_USAGE, usage);
	}

	ldb_close(&lorien_db);
}

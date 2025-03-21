/*
 * Copyright 1992-2025 Jillian Alana Bolton
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

#include <sys/stat.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <lmdb.h>
#include <string.h>
#include <sysexits.h>

#include "db.h"
#include "lorien.h"

/* It's OK for a player and a channel and a board to each be named "Player"
 * But we don't want duplicate player records, channel records, or board
 * records. For this reason, each type has a separate lmdb database inside
 * the loriendb folder.
 *
 * But we want the abstraction to be that there is one "loriendb" with
 * separate tables (board, chan, player)
 *
 */

struct lorien_db lorien_db = { 0 };

const char *ldb_names[] = {
	"board",
	"chan",
	"message",
	"player",
	(char *)0,
};

static
time_t htobetime(time_t bt) {
	errno = EINVAL;

	if (sizeof(time_t) == 8)
		return (time_t) htobe64((uint64_t) bt);
	else if (sizeof(time_t) == 4)
		return (time_t)htobe32((uint32_t) bt);

	err(EX_DATAERR, "time_t format is unrecognized");
	return 0; /* NOTREACHED */
}

static
time_t betimetoh(time_t ht) {
	errno = EINVAL;

	if (sizeof(time_t) == 8)
		return (time_t) be64toh((uint64_t) ht);
	else if (sizeof(time_t) == 4)
		return (time_t)be32toh((uint32_t) ht);

	err(EX_DATAERR, "time_t format is unrecognized");
	return 0; /* NOTREACHED */
}

static void
player_from_media(struct splayer *player, struct ldb_player *ldbp)
{
	strlcpy(player->name, ldbp->name, sizeof(player->name));
	strlcpy(player->password, ldbp->password, sizeof(player->password));
	strlcpy(player->host, ldbp->host, sizeof(player->host));

	player->seclevel = be32toh(ldbp->seclevel);
	player->hilite = be32toh(ldbp->hilite);
	player->privs = be32toh(ldbp->privs);
	player->wrap = be32toh(ldbp->wrap);
	player->flags = be32toh(ldbp->flags);
	player->pagelen = be32toh(ldbp->pagelen);
	player->playerwhen = betimetoh(ldbp->created);
	player->cameon = betimetoh(ldbp->login);
}

static void
player_to_media(struct ldb_player *ldbp, struct splayer *player)
{
	memset(ldbp, 0, sizeof(*ldbp));
	strlcpy(ldbp->name, player->name, sizeof(ldbp->name));
	strlcpy(ldbp->password, player->password, sizeof(ldbp->password));
	strlcpy(ldbp->host, player->host, sizeof(ldbp->host));

	ldbp->seclevel = htobe32(player->seclevel);
	ldbp->hilite = htobe32(player->hilite);
	ldbp->privs = htobe32(player->privs);
	ldbp->wrap = htobe32(player->wrap);
	ldbp->flags = htobe32(player->flags);
	ldbp->pagelen = htobe32(player->pagelen);
	ldbp->created = htobetime(player->playerwhen);
	ldbp->login = htobetime(player->cameon);
}

int
ldb_close(struct lorien_db *db)
{
	if (!db || !db->db)
		return EINVAL;
	mdb_env_close(db->db);
	return 0;
}

/* db name is, e.g., "./lorien.db" */
int
ldb_open(struct lorien_db *db)
{
	int rc;
	MDB_txn *txn;

	/* if the env is already open */
	if (!db || db->db)
		return EINVAL;

	rc = mdb_env_create(&db->db);
	if (rc != 0)
		return rc;

	rc = mkdir(db->dbname, 0700);
	if (!rc)
		warn("can't create %s", db->dbname);

	rc = mdb_env_set_maxdbs(db->db, LDB_MAX);

	rc = mdb_env_open(db->db, (const char *)db->dbname, 0, 0600);
	if (rc != 0)
		return rc;

	rc = mdb_txn_begin(db->db, NULL, 0, &txn);
	if (rc != 0)
		return rc;

	for (int i = 0; i < LDB_MAX; i++) {
		rc = mdb_dbi_open(txn, ldb_names[i], MDB_CREATE, &db->dbis[i]);
		if (rc != 0) {
			return rc;
		}
	}

	rc = mdb_txn_commit(txn);
	return rc;
}

int
ldb_player_delete(struct lorien_db *db, struct splayer *player)
{
	int rc;
	MDB_txn *txn;
	MDB_val key, data;

	struct ldb_player ldbp = { 0 };

	if (!db || !db->db || !player)
		return EINVAL;

	rc = mdb_txn_begin(db->db, NULL, 0, &txn);
	if (rc != 0)
		return rc;

	player_to_media(&ldbp, player);
	key.mv_data = ldbp.name;
	key.mv_size = strnlen(ldbp.name, sizeof(ldbp.name));
	data.mv_data = &ldbp;
	data.mv_size = sizeof(ldbp);

	rc = mdb_del(txn, db->dbis[LDB_PLAYER], &key, &data);
	if (rc != 0) {
		mdb_txn_abort(txn);
		return rc;
	}

	rc = mdb_txn_commit(txn);
	return rc;
}

int
ldb_player_get(struct lorien_db *db, const char *name, size_t namesz,
    struct splayer *player)
{
	int rc;
	MDB_txn *txn;
	MDB_val key, data;

	struct ldb_player ldbp = { 0 };

	if (!db || !db->db || !player)
		return EINVAL;

	rc = mdb_txn_begin(db->db, NULL, MDB_RDONLY, &txn);
	if (rc != 0)
		return rc;

	key.mv_data = (void *)name;
	key.mv_size = strnlen(name, namesz);

	rc = mdb_get(txn, db->dbis[LDB_PLAYER], &key, &data);
	if (rc != 0) {
		mdb_txn_abort(txn);
		return rc;
	}

	if (data.mv_size != sizeof(ldbp)) {
		mdb_txn_abort(txn);
		return EIO;
	}

	rc = mdb_txn_commit(txn);

	player_from_media(player, (struct ldb_player *)data.mv_data);

	return rc;
}

/* when adding records, pass nooverwrite = true */
int
ldb_player_put(struct lorien_db *db, struct splayer *player, bool nooverwrite)
{
	int rc;
	MDB_txn *txn;
	MDB_val key, data;

	struct ldb_player ldbp = { 0 };

	if (!db || !db->db || !player)
		return EINVAL;

	rc = mdb_txn_begin(db->db, NULL, 0, &txn);
	if (rc != 0)
		return rc;

	player_to_media(&ldbp, player);
	key.mv_data = ldbp.name;
	key.mv_size = strnlen(ldbp.name, sizeof(ldbp.name));
	data.mv_data = &ldbp;
	data.mv_size = sizeof(ldbp);

	rc = mdb_put(txn, db->dbis[LDB_PLAYER], &key, &data,
	    (nooverwrite) ? MDB_NOOVERWRITE : 0);
	if (rc != 0) {
		mdb_txn_abort(txn);
		return rc;
	}

	rc = mdb_txn_commit(txn);
	return rc;
}

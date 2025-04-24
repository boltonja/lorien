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

#include "ban.h"
#include "board.h"
#include "db.h"
#include "lorien.h"
#include "platform.h"

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
	"unused",
	"message",
	"player",
	"ban",
	(char *)0,
};

static time_t
htobetime(time_t bt)
{
	errno = EINVAL;

	if (sizeof(time_t) == 8)
		return (time_t)htobe64((uint64_t)bt);
	else if (sizeof(time_t) == 4)
		return (time_t)htobe32((uint32_t)bt);

	err(EX_DATAERR, "time_t format is unrecognized");
	return 0; /* NOTREACHED */
}

static time_t
betimetoh(time_t ht)
{
	errno = EINVAL;

	if (sizeof(time_t) == 8)
		return (time_t)be64toh((uint64_t)ht);
	else if (sizeof(time_t) == 4)
		return (time_t)be32toh((uint32_t)ht);

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

static void
ban_from_media(struct ban_item *ban, struct ldb_ban *ldbb)
{
	strlcpy(ban->pattern, ldbb->pattern, sizeof(ban->pattern));
	strlcpy(ban->owner, ldbb->owner, sizeof(ban->owner));

	ban->created = betimetoh(ldbb->created);
	ban->flags = be32toh(ldbb->flags);
}

static void
ban_to_media(struct ldb_ban *ldbb, struct ban_item *ban)
{
	memset(ldbb, 0, sizeof(*ldbb));
	strlcpy(ldbb->pattern, ban->pattern, sizeof(ldbb->pattern));
	strlcpy(ldbb->owner, ban->owner, sizeof(ldbb->owner));

	ldbb->created = htobetime(ban->created);
	ldbb->flags = htobe32(ban->flags);
}

static void
board_from_media(struct board *board, struct ldb_board *ldbb)
{
	strlcpy(board->name, ldbb->key.name, sizeof(board->name));
	strlcpy(board->owner, ldbb->owner, sizeof(board->owner));
	strlcpy(board->desc, ldbb->desc, sizeof(board->desc));

	board->created = betimetoh(ldbb->created);
	board->type = be32toh(ldbb->key.type);
	board->flags = be32toh(ldbb->flags);
}

static void
board_to_media(struct ldb_board *ldbb, struct board *board)
{
	memset(ldbb, 0, sizeof(*ldbb));
	strlcpy(ldbb->key.name, board->name, sizeof(ldbb->key.name));
	strlcpy(ldbb->owner, board->owner, sizeof(ldbb->owner));
	strlcpy(ldbb->desc, board->desc, sizeof(ldbb->desc));

	ldbb->created = htobetime(board->created);
	ldbb->key.type = htobe32(board->type);
	ldbb->flags = htobe32(board->flags);
}

static void
msg_to_media(struct ldb_msg *ldm, struct msg *msg)
{
	time_t p_created = msg->parent ? msg->parent->key.created : 0;
	int32_t p_created_us = (p_created) ? msg->parent->key.created_usec : 0;
	char *text = &ldm->data[msg->subjsz];
	char *subj = &ldm->data[0];

	ldm->key.created = htobetime(msg->key.created);
	ldm->key.created_usec = htobe32(msg->key.created_usec);
	ldm->parent_created = htobetime(p_created);
	ldm->parent_created_usec = htobe32(p_created_us);
	ldm->board_type = htobe32(msg->board_type);
	ldm->subjsz = htobe64(msg->subjsz);
	ldm->textsz = htobe64(msg->textsz);

	/* paranoia for the delete case */
	if (msg->board)
		strlcpy(ldm->board, msg->board->name, sizeof(ldm->board));
	else
		memset(ldm->board, 0, sizeof(ldm->board));
	strlcpy(ldm->owner, msg->owner, sizeof(ldm->owner));
	strlcpy(subj, msg->subj, msg->subjsz);
	strlcpy(text, msg->text, msg->textsz);
}

static void
msg_from_media(struct ldb_msg *out, struct ldb_msg *in)
{
	char *text, *itext;
	char *subj, *isubj;

	out->key.created = betimetoh(in->key.created);
	out->key.created_usec = be32toh(in->key.created_usec);
	out->parent_created = betimetoh(in->key.created);
	out->parent_created_usec = be32toh(in->parent_created_usec);
	out->board_type = be32toh(in->board_type);
	out->subjsz = be64toh(in->subjsz);
	out->textsz = be64toh(in->textsz);

	text = &out->data[out->subjsz];
	subj = &out->data[0];
	itext = &in->data[out->subjsz];
	isubj = &in->data[0];

	strlcpy(out->board, in->board, sizeof(out->board));
	strlcpy(out->owner, in->board, sizeof(out->board));
	strlcpy(subj, isubj, sizeof(out->subjsz));
	strlcpy(text, itext, sizeof(out->textsz));
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

int
ldb_ban_delete(struct lorien_db *db, struct ban_item *ban)
{
	int rc;
	MDB_txn *txn;
	MDB_val key, data;

	struct ldb_ban ldbb = { 0 };
	if (!db || !db->db || !ban)
		return EINVAL;

	rc = mdb_txn_begin(db->db, NULL, 0, &txn);
	if (rc != 0)
		return rc;

	ban_to_media(&ldbb, ban);
	key.mv_data = ldbb.pattern;
	key.mv_size = strnlen(ldbb.pattern, sizeof(ldbb.pattern));
	data.mv_data = &ldbb;
	data.mv_size = sizeof(ldbb);

	rc = mdb_del(txn, db->dbis[LDB_BAN], &key, &data);
	if (rc != 0) {
		mdb_txn_abort(txn);
		return rc;
	}

	rc = mdb_txn_commit(txn);

	return rc;
}

int
ldb_ban_scan(struct lorien_db *db, int (*banfunc)(struct ban_item *))
{
	int rc;
	MDB_txn *txn;
	MDB_val key, data;
	MDB_cursor *cursor;

	struct ban_item ban = { 0 };

	if (!db || !db->db || !banfunc)
		return EINVAL;

	rc = mdb_txn_begin(db->db, NULL, 0, &txn);
	if (rc != 0)
		return rc;

	rc = mdb_cursor_open(txn, db->dbis[LDB_BAN], &cursor);
	if (rc != 0)
		goto errtxn;

	rc = mdb_cursor_get(cursor, &key, &data, MDB_FIRST);
	if (rc != 0)
		goto errcurs;

	do {
		if (data.mv_size != sizeof(struct ldb_ban)) {
			rc = EBADMSG;
			goto errcurs;
		}

		ban_from_media(&ban, (struct ldb_ban *)data.mv_data);
		if (!banfunc(&ban)) {
			rc = ENOMEM;
			goto errcurs;
		}

		rc = mdb_cursor_get(cursor, &key, &data, MDB_NEXT);
	} while (rc == 0);

	/* read only transaction, ok to abort on success */
errcurs:
	mdb_cursor_close(cursor);
errtxn:
	mdb_txn_abort(txn);
	return rc;
}

int
ldb_ban_put(struct lorien_db *db, struct ban_item *ban)
{
	int rc;
	MDB_txn *txn;
	MDB_val key, data;

	struct ldb_ban ldbb = { 0 };

	if (!db || !db->db || !ban)
		return EINVAL;

	rc = mdb_txn_begin(db->db, NULL, 0, &txn);
	if (rc != 0)
		return rc;

	ban_to_media(&ldbb, ban);
	key.mv_data = ldbb.pattern;
	key.mv_size = strnlen(ldbb.pattern, sizeof(ldbb.pattern));
	data.mv_data = &ldbb;
	data.mv_size = sizeof(ldbb);

	rc = mdb_put(txn, db->dbis[LDB_BAN], &key, &data, MDB_NOOVERWRITE);
	if (rc != 0) {
		mdb_txn_abort(txn);
		return rc;
	}

	rc = mdb_txn_commit(txn);
	return rc;
}

int
ldb_board_delete(struct lorien_db *db, struct board *board)
{
	int rc;
	MDB_txn *txn;
	MDB_val key, data;

	struct ldb_board ldbb = { 0 };
	if (!db || !db->db || !board)
		return EINVAL;

	rc = mdb_txn_begin(db->db, NULL, 0, &txn);
	if (rc != 0)
		return rc;

	board_to_media(&ldbb, board);
	key.mv_data = ldbb.name;
	key.mv_size = strnlen(ldbb.name, sizeof(ldbb.name));
	data.mv_data = &ldbb;
	data.mv_size = sizeof(ldbb);

	rc = mdb_del(txn, db->dbis[LDB_BOARD], &key, &data);
	if (rc != 0) {
		mdb_txn_abort(txn);
		return rc;
	}

	rc = mdb_txn_commit(txn);

	return rc;
}

int
ldb_board_scan(struct lorien_db *db, int (*boardfunc)(struct board *))
{
	int rc;
	MDB_txn *txn;
	MDB_val key, data;
	MDB_cursor *cursor;

	struct board board = { 0 };

	if (!db || !db->db || !boardfunc)
		return EINVAL;

	rc = mdb_txn_begin(db->db, NULL, 0, &txn);
	if (rc != 0)
		return rc;

	rc = mdb_cursor_open(txn, db->dbis[LDB_BOARD], &cursor);
	if (rc != 0)
		goto errtxn;

	rc = mdb_cursor_get(cursor, &key, &data, MDB_FIRST);
	if (rc != 0)
		goto errcurs;

	do {
		if (data.mv_size != sizeof(struct ldb_board)) {
			rc = EBADMSG;
			goto errcurs;
		}

		board_from_media(&board, (struct ldb_board *)data.mv_data);
		if (!boardfunc(&board)) {
			rc = ENOMEM;
			goto errcurs;
		}

		rc = mdb_cursor_get(cursor, &key, &data, MDB_NEXT);
	} while (rc == 0);

	/* read only transaction, ok to abort on success */
errcurs:
	mdb_cursor_close(cursor);
errtxn:
	mdb_txn_abort(txn);
	return rc;
}

int
ldb_board_put(struct lorien_db *db, struct board *board)
{
	int rc;
	MDB_txn *txn;
	MDB_val key, data;

	struct ldb_board ldbb = { 0 };

	if (!db || !db->db || !board)
		return EINVAL;

	rc = mdb_txn_begin(db->db, NULL, 0, &txn);
	if (rc != 0)
		return rc;

	board_to_media(&ldbb, board);
	key.mv_data = ldbb.name;
	key.mv_size = strnlen(ldbb.name, sizeof(ldbb.name));
	data.mv_data = &ldbb;
	data.mv_size = sizeof(ldbb);

	rc = mdb_put(txn, db->dbis[LDB_BOARD], &key, &data, MDB_NOOVERWRITE);
	if (rc != 0) {
		mdb_txn_abort(txn);
		return rc;
	}

	rc = mdb_txn_commit(txn);
	return rc;
}

int
ldb_msg_scan(struct lorien_db *db, int (*msgfunc)(struct ldb_msg *))
{
	int rc;
	MDB_txn *txn;
	MDB_val key, data;
	MDB_cursor *cursor;

	struct ldb_msg *ldm = NULL;
	size_t ldmsz = 0;

	if (!db || !db->db || !msgfunc)
		return EINVAL;

	rc = mdb_txn_begin(db->db, NULL, 0, &txn);
	if (rc != 0)
		return rc;

	rc = mdb_cursor_open(txn, db->dbis[LDB_MSG], &cursor);
	if (rc != 0)
		goto errtxn;

	rc = mdb_cursor_get(cursor, &key, &data, MDB_FIRST);
	while (rc == 0) {
		if (data.mv_size <= sizeof(*ldm)) {
			rc = EBADMSG;
			break;
		}

		if (ldm == NULL) {
			ldm = malloc(data.mv_size);
			ldmsz = data.mv_size;
		} else if (data.mv_size > ldmsz) {
			ldm = realloc(ldm, data.mv_size);
			ldmsz = data.mv_size;
		}

		if (ldm == NULL) {
			rc = ENOMEM;
			break;
		}

		memset(ldm, 0, ldmsz);
		msg_from_media(ldm, (struct ldb_msg *)data.mv_data);
		if (!msgfunc(ldm)) {
			rc = ENOMEM;
			break;
		}

		rc = mdb_cursor_get(cursor, &key, &data, MDB_NEXT);
	}

	/* read only transaction, ok to abort on success */
	if (ldm)
		free(ldm);
	mdb_cursor_close(cursor);
errtxn:
	mdb_txn_abort(txn);
	return rc;
}

int
ldb_msg_delete(struct lorien_db *db, struct msg *msg)
{
	size_t sz;
	struct ldb_msg *ldm;
	MDB_txn *txn;
	MDB_val key, data;
	int rc;

	if (!msg || !msg->textsz || !msg->subjsz || !msg->subj || !msg->text)
		return EINVAL;

	sz = sizeof(struct ldb_msg) + msg->subjsz + msg->textsz;
	ldm = calloc(1, sz);
	if (!ldm)
		return ENOMEM;

	msg_to_media(ldm, msg);

	rc = mdb_txn_begin(db->db, NULL, 0, &txn);
	if (rc != 0) {
		free(ldm);
		return rc;
	}

	key.mv_data = &ldm->key;
	key.mv_size = sizeof(ldm->key);
	data.mv_data = ldm;
	data.mv_size = sz;

	rc = mdb_del(txn, db->dbis[LDB_MSG], &key, &data);
	if (rc != 0) {
		mdb_txn_abort(txn);
		free(ldm);
		return rc;
	}

	rc = mdb_txn_commit(txn);
	free(ldm);
	return rc;
}

int
ldb_msg_put(struct lorien_db *db, struct msg *msg)
{
	size_t sz;
	struct ldb_msg *ldm;
	MDB_txn *txn;
	MDB_val key, data;
	int rc;

	if (!msg || !msg->textsz || !msg->subjsz || !msg->subj || !msg->text)
		return EINVAL;

	sz = sizeof(struct ldb_msg) + msg->subjsz + msg->textsz;
	ldm = calloc(1, sz);
	if (!ldm)
		return ENOMEM;

	msg_to_media(ldm, msg);

	rc = mdb_txn_begin(db->db, NULL, 0, &txn);
	if (rc != 0) {
		free(ldm);
		return rc;
	}

	key.mv_data = &ldm->key;
	key.mv_size = sizeof(ldm->key);
	data.mv_data = ldm;
	data.mv_size = sz;

	rc = mdb_put(txn, db->dbis[LDB_MSG], &key, &data, MDB_NOOVERWRITE);
	if (rc != 0) {
		mdb_txn_abort(txn);
		free(ldm);
		return rc;
	}

	rc = mdb_txn_commit(txn);
	free(ldm);
	return rc;
}

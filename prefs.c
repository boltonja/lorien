/*
 * Copyright 1990-1996 Chris Eleveld
 * Copyright 1992 Robert Slaven
 * Copyright 1992-2024 Jillian Alana Bolton
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

#include "lorien.h"
#include "newplayer.h"
#include "platform.h"
#include "prefs.h"
#include "trie.h"

static int
conversiondiag(void *pplayer, char *b)
{
#ifndef CONVERT
	if (pplayer)
		sendtoplayer(pplayer, b);
#else
	fprintf(stderr, "%s", b);
#endif
	return 0;
}

#ifdef CONVERT
#define sendtoplayer(p, b) conversiondiag(p, b)
#endif

static char dbuf[OBUFSIZE];

void
prefs_meta_ntoh(struct metadata *md)
{
	md->version = ntohl(md->version);
}

void
prefs_meta_hton(struct metadata *md)
{
	md->version = htonl(md->version);
}

void
prefs_ntoh(struct splayer *p)
{
	p->seclevel = ntohl(p->seclevel);
	p->hilite = ntohl(p->hilite);
	p->privs = ntohl(p->privs);
	p->wrap = ntohl(p->wrap);
	p->flags = ntohl(p->flags) & ~PLAYER_DONT_SAVE_MASK;
	p->free = ntohl(p->free);
	/* the 64-bit time_t implementations will find the low order bits
	 * in the right spot in any file due to the layout of the player
	 * struct.
	 * the 32-bit implementations will find all 32-bits they can
	 {	 * handle in the right spot by virtue of same, but
	 * will not have the proper time when 32-bit time_t overflows
	 */
	p->cameon = ntohl(p->cameon);
}

void
prefs_hton(struct splayer *p)
{
	p->seclevel = htonl(p->seclevel);
	p->hilite = htonl(p->hilite);
	p->privs = htonl(p->privs);
	p->wrap = htonl(p->wrap);
	p->flags = htonl(p->flags & ~PLAYER_DONT_SAVE_MASK);
	p->free = htonl(p->free);
	p->cameon = htonl(
	    p->cameon); /* see note in prefs_ntoh() about 64-bit */
}

/* prefs_get_metadata()
 *
 * To make the math easy, metadata is overlaid onto
 * a player struct and stored in the first record.
 *
 * returns -1 on failure, writes status with errno
 *
 * on success,
 *    writes 0 into status
 *    returns the number of bytes of metadata read
 *    leaves pointer positioned at first data record.
 */
int
prefs_get_metadata(int fd, struct metadata *metadata, int *status)
{
	int rc;
	struct splayer rec0;

	if (!status) {
		return -1;
	}
	if (!metadata) {
		*status = EACCES;
		return -1;
	}
	if (fd == -1) {
		*status = EBADF;
		return -1;
	}

	rc = lseek(fd, 0, SEEK_SET);
	if (rc == -1) {
		*status = errno;
		return rc;
	} else {
		rc = read(fd, &rec0, v174reclen(rec0));
		if (rc == -1)
			*status = errno;
		if (rc == 0 || rc < v174reclen(rec0)) { /* premature EOF */
			memset(metadata, 0, sizeof(struct metadata));
			memset(&rec0, 0, sizeof(rec0));
			metadata->version = LORIEN_VCURR;
			prefs_meta_hton(metadata);
			memcpy(&rec0, metadata, sizeof(struct metadata));

			*status = 0;
			rc = lseek(fd, 0, SEEK_SET);
			if (rc == -1)
				*status = errno;
			else {
				rc = write(fd, &rec0, v174reclen(rec0));
				if (rc == -1)
					*status = errno;
			}
			return rc;
		} else {
			*status = 0;
			memcpy(metadata, &rec0, sizeof(struct metadata));
			prefs_meta_ntoh(metadata);
			rc = sizeof(struct metadata);
		}
	}

	return rc;
}

/* prefs_open()
 *
 * gets the version number of the file from metadata in
 * the 0th record
 * seeks to the first player record
 * returns fd if file can be opened
 * on success of all operations,
 *     writes 0 into status
 *     writes saved metatdata into the metadata argument
 *     leaves file pointer set at first data record.
 *     returns the fd.
 * on first failure,
 *     writes the errno value in the status argument
 *     metadata argument is unchanged
 *     attempts to reposition the file pointer to point
 *        to the metadata record at offset 0, which is
 *        possibly incomplete.
 *        returns the fd if open succeeded, or -1.
 */
int
prefs_open(char *file, struct metadata *metadata, int *status)
{
	int fd;

#if defined(USE_32_BIT_TIME_T)
	if (sizeof(time_t) != 4) {
		fprintf(stderr,
		    "time_t not 32 bits on this system, see Makefile!\n");
		fflush(stderr);
		abort();
	}
#elif defined(USE_64_BIT_TIME_T)
	if (sizeof(time_t) != 8) {
		fprintf(stderr,
		    "time_t not 64 bits on this system, see Makefile!\n");
		fflush(stderr);
		abort();
	}
#else
	fprintf(stderr, "time_t size is unknown to prefsdb\n");
	fprintf(stderr, "time_t not 64 bits on this system, see Makefile!\n");
	fflush(stderr);
	abort();
#endif

	if (!status) {
		return -1;
	}
	if (!metadata) {
		*status = EACCES;
		return -1;
	}
	if (fd == -1) {
		*status = EBADF;
		return -1;
	}

#ifndef _MSC_VER
	fd = open(file, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
#else
	fd = open(file, O_RDWR | O_CREAT, S_IREAD | S_IWRITE);
#endif

	if (fd == -1) {
		*status = errno;
	} else {
		if (prefs_get_metadata(fd, metadata, status) == -1)
			lseek(fd, 0,
			    SEEK_SET); /* best effort, error is discarded */
	}

	return fd;
}

int
prefs_close(int fd)
{
	if (close(fd) == -1)
		return errno;
	else
		return 0;
}

/* prefs_reposition()
 * repositions to the start of the current record
 * and returns the offset.
 *
 * on failure, returns -1 and writes errno into
 * status
 */
int
prefs_reposition(int fd, int *status)
{
	int rc;
	struct splayer rec;
	int reclen = v174reclen(rec);

	if (!status)
		return -1;
	if (fd == -1) {
		*status = EBADF;
		return -1;
	}

	rc = lseek(fd, 0, SEEK_CUR); /* find out where we are */

	if (rc == -1)
		*status = errno;
	else if (rc % reclen) {
		/* somehow we got into the middle of a record.
		 * rewind to the beginning of the record.
		 */
		rc /= reclen;
		rc = lseek(fd, rc, SEEK_SET);
		if (rc == -1)
			*status = errno;
	}
	return rc;
}

/* prefs_curr()
 *
 * read the current record and its offset into the
 * player structure.
 *
 * assumes the file pointer is pointing at a valid record
 * on error
 *    file pointer is undefined
 *    writes errno into status
 *    returns -1
 * on success
 *    returns the number of bytes read.
 *    zeroes status
 *    file pointer points to next record
 *    player->dboffset contains the lseek(2)
 *       offset relative to SEEK_SET.
 */
int
prefs_curr(int fd, struct splayer *player, int *status)
{
	int rc;
	struct splayer rec;
	int reclen = v174reclen(rec);
	int offs;

	if (!status) {
		return -1;
	}
	if (!player) {
		*status = EACCES;
		return -1;
	}
	if (fd == -1) {
		*status = EBADF;
		return -1;
	}

	rc = prefs_reposition(fd, status);

	if (rc == -1)
		return rc;
	else
		offs = rc;

	rc = read(fd, &rec, reclen);
	if (rc == -1)
		*status = errno;
	else {
		*status = 0;
		if (rc == 0)
			return 0; /* EOF */
		else {
			prefs_ntoh(&rec);
			memcpy(player, &rec, reclen);
			player->dboffset = offs;
			rc = reclen;
		}
	}
	return rc;
}

int
prefs_compute_offset(int numrecords)
{
	struct splayer rec;
	int reclen = v174reclen(rec);

	return numrecords * reclen;
}

/* prefs_seek() seeks to absolute offset
 * and returns the offset or -1
 */
int
prefs_seek(int fd, int offset, int *status)
{
	int rc;

	if (!status)
		return -1;

	rc = lseek(fd, offset, SEEK_SET);
	if (rc == -1)
		*status = errno;
	else
		*status = 0;

	return rc;
}

/* prefs_next() seeks to the next record
 * returns the offset or -1 on fail.
 */
int
prefs_next(int fd, int *status)
{
	int rc;
	struct splayer rec;
	int reclen = v174reclen(rec);

	if (!status) {
		return -1;
	}
	if (fd == -1) {
		*status = EBADF;
		return -1;
	}

	if (!status || fd == -1)
		return -1;

	rc = prefs_reposition(fd, status);

	if (rc == -1)
		return rc;

	rc += reclen;
	return prefs_seek(fd, rc, status);
}

/* prefs_prev() seeks to the previous record
 * returns the offset or -1 on fail.
 */
int
prefs_prev(int fd, int *status)
{
	int rc;
	struct splayer rec;
	int reclen = v174reclen(rec);

	if (!status) {
		return -1;
	}
	if (fd == -1) {
		*status = EBADF;
		return -1;
	}

	rc = prefs_reposition(fd, status);

	if (rc == -1)
		return rc;

	rc -= reclen;
	return prefs_seek(fd, rc, status);
}

/* prefs_write() writes the specified player to the db
 *    the dboffset field of the struct must be the
 *    offset of the player record or ZERO (0)
 *
 *    if the dboffset field is 0, the record is
 *    appended to the file.
 *
 *    on success, the offset of the record is
 *       updated in player->dboffset.
 *       returns the number of bytes written
 *       writes zero into status
 *    on failure, writes errno into status
 *       and returns -1;
 *       status contains EPERM if the specified
 *          dboffset contains a record whose name does
 *          not match
 *       status contains ESRCH if the specified offset
 *          cannot be positioned.
 *       status contains E2BIG if the specified offset
 *          is at or past EOF.
 */
int
prefs_write(int fd, struct splayer *player, int *status)
{
	struct splayer rec;
	int rc;
	int endoffs;

	if (!status) {
		return -1;
	}
	if (!player) {
		*status = EACCES;
		return -1;
	}
	if (fd == -1) {
		*status = EBADF;
		return -1;
	}

	endoffs = lseek(fd, 0, SEEK_END);
	if (endoffs == -1) {
		*status = errno;
		return -1;
	}
	if (player->dboffset) {
		if (player->dboffset >= endoffs) {
			*status = E2BIG;
			return -1;
		}
		rc = lseek(fd, player->dboffset, SEEK_SET);
		if (rc == -1) {
			*status = errno;
			return -1;
		}
		rc = prefs_reposition(fd, status);
		if (rc != player->dboffset && player->dboffset) {
			*status = ESRCH;
			return -1;
		}
		/* verify the names match */
		rc = prefs_curr(fd, &rec, status);
		/* if the record isn't free, require the names to match */
		if (!rec.free) {
			if (strcmp(rec.name, player->name)) {
				*status = EPERM;
				return -1;
			}
		}
		rc = prefs_prev(fd, status);
		if (rc == -1)
			return rc;
	}
	/* else append to file, already positioned */

	memcpy(&rec, player, sizeof(rec));
	prefs_hton(&rec);
	rc = write(fd, &rec, v174reclen(rec));
	if (rc == -1)
		*status = errno;
	else {
#ifdef CONVERT
		printf("wrote %s (%d bytes) @%lld\n", player->name, rc,
		    player->dboffset ? player->dboffset : endoffs);
#endif
		*status = 0;
		if (!(player->dboffset))
			player->dboffset = endoffs;
	}
#ifdef CONVERT
	printf("write:  rc = %d\n", rc);
#endif
	return rc;
}

/* prefs_cache_insert() inserts a record into the cache
 *
 * it will select which list based on the "free" field
 * of the specified record.
 *
 * it marks the corresponding cache invalid if it can't
 * add the record, unless the record already exists.
 * if the record exists, the cache is not marked invalid,
 * and status will contain EEXIST.
 *
 * it will attempt to reclaim the free-list memory in
 * favor of the index.
 *
 * returns a pointer to the struct prefs_index entry on success,
 * or NULL on failure.
 *
 * puts status from trie_add in status on failure
 *
 */

struct prefs_index *
prefs_cache_insert(struct prefs_cache *cache, struct splayer *prefs,
    int *status)
{
	struct prefs_index *idxp;
	trie *leaf;

	if (!status)
		return NULL;

	if (!cache || !prefs) {
		*status = EACCES;
		return NULL;
	}

	idxp = (struct prefs_index *)malloc(sizeof(struct prefs_index));
	if (!idxp) { /* attempt to reclaim */
		trie_collapse(cache->freelist, 1);
		cache->state &= ~PC_FREE_VALID;
		if (prefs->free)
			return NULL;

		idxp = (struct prefs_index *)malloc(sizeof(struct prefs_index));
		if (!idxp) {
			*status = ENOMEM;
			cache->state &= ~PC_INDEX_VALID;
			return NULL;
		}
	}

	if (!prefs->free) {
		strncpy(idxp->name, prefs->name, LORIEN_V0174_NAME);
		idxp->name[LORIEN_V0174_NAME - 1] = 0;
		strncpy(idxp->password, prefs->password, LORIEN_V0174_PASS);
		idxp->password[LORIEN_V0174_PASS - 1] = 0;
		idxp->free = 0;
	} else {
		if (!(cache->state & PC_FREE_VALID)) {
			*status = ENOMEM;
			return NULL; /* we have dumped the free list due to low
					mem */
		}
		/* record is inactive, add to free list */
		idxp->free = 1;
		snprintf(idxp->name, sizeof(idxp->name), "freefreefree%lld",
		    prefs->dboffset);
		idxp->dboffset = prefs->dboffset;
	}

	leaf = trie_add((idxp->free) ? cache->freelist : cache->index,
	    (unsigned char *)idxp->name, idxp, status);
	if (*status == EEXIST) {
		free(idxp);
		return NULL;
	}
	if (!leaf) { /* out of mem, try reclaiming free list */
		if (cache->state & PC_FREE_VALID) {
			cache->state &= ~PC_FREE_VALID;
			trie_collapse(cache->freelist, 1);
		}
		if (prefs->free) { /* dont try to add to reclaimed free list */
			*status = ENOMEM;
			return NULL;
		}

		leaf = trie_add(cache->index, (unsigned char *)idxp->name, idxp,
		    status);
		if (!leaf) { /* reclaim failed */
			free(idxp);
			trie_collapse(cache->index, 1);
			/* free list already collapsed */
			cache->state &= ~PC_INDEX_VALID;
			return NULL;
		}
	}
	return idxp;
}

/* prefs_cache_find()
 *
 * performs cached read-through lookups of records in the preferences file.
 *
 * if the cache is valid, it is used.
 * if the cache is not valid, it is rebuilt.
 * if the rebuild fails, the file is read sequentially until the desired
 * record or EOF is found.
 *
 * returns -1 on failure.
 * status is presently undefined.
 */
int
prefs_cache_find(struct prefs_cache *cache, char *name, struct splayer *prefs,
    int *status)
{
	size_t trie_match_len;
	struct prefs_index *idxp = NULL;
	trie *leaf = NULL;
	int rc = 0;

	if (!cache || !prefs)
		return -1;

	if (!(cache->state & PC_INDEX_VALID))
		rc = prefs_cache_reload(NULL, cache);

	if (rc == -1) { /* reload failed, read-through sequentially */
		/* start at first data record, immediately after metadata */
		rc = prefs_seek(cache->fd, prefs_compute_offset(1), status);
		if (rc != prefs_compute_offset(1)) {
			snprintf(dbuf, sizeof(dbuf),
			    ">> can't position file pointer\n");
			fprintf(stderr, "%s", dbuf);
			return -1;
		}
		while (prefs_curr(cache->fd, prefs, status) ==
		    v174reclen(*prefs)) {
			if (prefs->free)
				continue;
			if (!strcmp(prefs->name, name)) {
				return prefs->dboffset;
			}
		}
	} else { /* cache is valid, proceed with search */
		leaf = trie_match(cache->index, (unsigned char *)prefs->name,
		    &trie_match_len, trie_keymatch_exact);

		idxp = (leaf) ? (struct prefs_index *)leaf->payload : NULL;

		if (idxp) {
			rc = prefs_seek(cache->fd, idxp->dboffset, status);
			if (rc != -1)
				rc = prefs_curr(cache->fd, prefs, status);
			if (rc != -1)
				return prefs->dboffset;
		}
	}
	return -1; /* not found */
}

/* prefs_cache_write()
 *
 * performs cached write-through of the preferences file.
 *
 * If the dboffset of the specified record is zero (0),
 * the record is added to the file (at end or using a reclaimed
 * freelist entry.)
 *
 * If dboffset is non-zero, it must be the known offset of the
 * existing record in order to avoid corruption.
 *
 * (***) If the cache is valid, built-in paranoia prevents
 * corruption by refusing to update existing records whose
 * dboffset doesn't match the cache, and will force updating
 * of records if one is cached even when the supplied prefs
 * has zero dboffset.
 *
 * if the non-zero offset doesn't match the cache it writes ESRCH
 * into status.
 *
 * returns -1 on failure.
 *
 */
int
prefs_cache_write(struct splayer *pplayer, struct prefs_cache *cache,
    struct splayer *prefs, int *status)
{
	struct prefs_index *idxp;
	size_t trie_match_len;
	int offsetsav = prefs->dboffset;
	int rc;
	trie *leaf;

	if (!status)
		return -1;

	if (!cache || !prefs) {
		*status = EACCES;
		return -1;
	}

	if (cache->fd == -1) {
		*status = EBADF;
		return -1;
	}

	if (!(cache->state & PC_INDEX_VALID))
		rc = prefs_cache_reload(pplayer, cache);

	leaf = trie_match(cache->index, (unsigned char *)prefs->name,
	    &trie_match_len, trie_keymatch_exact);

	idxp = (leaf) ? (struct prefs_index *)leaf->payload : NULL;

	if (idxp && (idxp->dboffset != prefs->dboffset)) {
		if (prefs->dboffset) {
			*status = ESRCH;
			return -1;
		} else /* we thought it was new, but it already exits? */
			offsetsav = prefs->dboffset = idxp->dboffset;
	}

	rc = prefs_write(cache->fd, prefs, status);

	if (rc == -1) {
		snprintf(dbuf, sizeof(dbuf),
		    ">> Can't write preferences, errno %d\n", *status);
		sendtoplayer(pplayer, dbuf);
		return rc;
	}

	/* add to cache, but only if its valid.
	 * (if it's invalid, adding will fail, just as reloading
	 * the cache failed above)
	 */
	if (!offsetsav && (cache->state & PC_INDEX_VALID)) {
		idxp = prefs_cache_insert(cache, prefs, status);

		if (!idxp) {
			if (*status == EEXIST)
				snprintf(dbuf, sizeof(dbuf),
				    ">> Wrote %s, already cached?\r\n",
				    prefs->name);
			else
				snprintf(dbuf, sizeof(dbuf),
				    ">> Wrote %s but can't add to cache.\r\n",
				    prefs->name);

			sendtoplayer(pplayer, dbuf);
		}
	}
	return rc;

#ifdef PARANOID_CACHE
#error "this code isn't finished"
	/* this code is meant to recover from a corrupted
	 * cache or a corrupted dboffset, but it hasn't been
	 * finished because it might be better to abort.
	 */
	if (cache->state & PC_INDEX_VALID) {
		leaf = trie_match(cache->index, prefs->name, &status,
		    trie_keymatch_exact);
		if ((!leaf && prefs->dboffset) ||
		    (leaf &&
			prefs->dboffset !=
			    ((struct prefs_index)leaf->payload)->dboffset)) {
			/* the record and cache disagree about its
			 * location in the file.  someone is lying!
			 */
			cache->state = PC_INVALID;
			trie_collapse(cache->index, 1);
			trie_collapse(cache->freelist, 1);
			rc = prefs_cache_reload(pplayer, cache);
			if (rc == -1) {
				*status = ENODEV;
				return -1;
			}
			leaf = trie_match(cache->index, prefs->name, &status,
			    trie_keymatch_exact);
			if (!leaf && (cache->state & PC_INDEX_VALID)) {
				*status = ESRCH;
				return -1;
			} else {
				*status = EXDEV;
				return -1;
			}
		}
	}
#endif
}

/* prefs_delete() deletes the specified player
 * if and only if player->dboffset is non-zero.
 *
 * the dboffset must contain the offset of the player
 *
 * on failure, returns -1 and updates status
 * on success, marks the player "free" in the db
 *
 * if the specified record was zero or nonexistent
 *    writes ENOENT into status and returns -1
 */
int
prefs_delete(int fd, struct splayer *player, int *status)
{
	struct splayer rec;

	if (!status) {
		return -1;
	}
	if (!player) {
		*status = EACCES;
		return -1;
	}
	if (fd == -1) {
		*status = EBADF;
		return -1;
	}
	if (!player->dboffset) {
		*status = ENOENT;
		return -1;
	}

	memcpy(&rec, player, sizeof(rec));
	rec.free = 1;
	memset(rec.host, 0, sizeof(rec.host));
	memset(rec.onfrom, 0, sizeof(rec.onfrom));
	memset(rec.numhost, 0, sizeof(rec.numhost));
	memset(rec.password, 0, sizeof(rec.password));
	strcpy(rec.host, "DELETED");
	strcpy(rec.onfrom, "DELETED");
	strcpy(rec.numhost, "DELETED");
	strcpy(rec.password, "DELETED");

	return prefs_write(fd, &rec, status);
}

/* for use by import routine */
static struct p_word v173rec;
static struct splayer v174rec;

/* releases the current cache and re-reads the file
 *
 * this routine always releases(frees) the freelist.
 *
 * if the cache cannot be re-read (due to resources)
 * the cache is not released (freed) and -1 is returned.
 *
 * if the file is not open, -1 is returned.
 *
 * the index and free lists need not be allocated nor populated,
 * they are initialized if necesary.
 *
 * this routine will succeed even if it cannot populate the new free
 * list, in which case it will empty the free new list and mark it
 * invalid.
 *
 * on success returns 0;
 */
int
prefs_cache_reload(struct splayer *pplayer, struct prefs_cache *cache)
{
	int rc;
	struct splayer curr;
	struct prefs_cache newcache;
	struct prefs_index *idxp;
	trie *leaf = NULL;
	int freerecs = 0;
	int activerecs = 0;
	int status;

	if (!cache || (cache->fd == -1))
		return -1;

	newcache.index = trie_new();
	if (!newcache.index) {
		snprintf(dbuf, sizeof(dbuf), ">> can't initialize index\n");
		conversiondiag(pplayer, dbuf);

		return -1;
	}

	newcache.fd = cache->fd;
	newcache.freelist = trie_new();
	if (!newcache.freelist) {
		snprintf(dbuf, sizeof(dbuf), ">> can't initialize freelist\n");
		conversiondiag(pplayer, dbuf);

		trie_collapse(newcache.index, 1);
		return -1;
	}

	newcache.state = PC_INDEX_VALID | PC_FREE_VALID;

	/* free up some heap space to increase our chances of
	 * success in low memory situations (at the risk of
	 * temporarily losing the ability to reclaim free
	 * records in the file.
	 */

	leaf = trie_find_first(cache->freelist);

	if (leaf) /* if no entries, state is unchanged */
		cache->state &= ~PC_FREE_VALID;

	trie_collapse(cache->freelist, 1);

	/* start at first data record, immediately after metadata */
	rc = prefs_seek(newcache.fd, prefs_compute_offset(1), &status);
	if (rc != prefs_compute_offset(1)) {
		snprintf(dbuf, sizeof(dbuf),
		    ">> can't position file pointer\n");
		conversiondiag(pplayer, dbuf);

		trie_collapse(newcache.index, 1);
		trie_collapse(newcache.freelist, 1);
		return -1;
	}

	/* read in the file */
	while (prefs_curr(newcache.fd, &curr, &rc) == v174reclen(curr)) {
		if (curr.free)
			freerecs++;
		else
			activerecs++;

		idxp = prefs_cache_insert(&newcache, &curr, &status);
		if (!idxp) {
			if (status == EEXIST) {
				snprintf(dbuf, sizeof(dbuf),
				    ">> %s is duplicate record at %lld\n",
				    curr.name, curr.dboffset);
				conversiondiag(pplayer, dbuf);

				continue;
			}

			snprintf(dbuf, sizeof(dbuf),
			    ">> can't insert cache entry %s in %s\n", curr.name,
			    (curr.free) ? "freelist." : "active list");
			conversiondiag(pplayer, dbuf);

			trie_collapse(newcache.freelist, 1);
			newcache.state &= ~PC_FREE_VALID;
			if (!curr.free) {
				trie_collapse(newcache.index, 1);
				return -1;
			}
		}
	}

	trie_collapse(cache->index, 1);
	trie_collapse(cache->freelist, 1);

	memcpy(cache, &newcache, sizeof(newcache));
	memset(&newcache, 0, sizeof(newcache));

	snprintf(dbuf, sizeof(dbuf),
	    ">> cached %d active, %d free records, free list is %s\n",
	    activerecs, freerecs,
	    cache->state & PC_FREE_VALID ? "valid." : "invalid.");
	conversiondiag(pplayer, dbuf);

	return 0;
}

/* prefs_cache_load()
 *
 * opens the named file, and pre-caches the records
 * into the active and free lists.
 *    assumes cache->fd is invalid
 *    if the file doesn't exist, it is initialized.
 *    on success, leaves the file descriptor cache->fd open
 *
 * returns -1 on fail
 */
int
prefs_cache_load(struct splayer *pplayer, struct prefs_cache *cache, char *fn)
{
	int status;
	int rc;

	if (!cache || !fn)
		return -1;

	cache->fd = prefs_open(fn, &cache->md, &status);
	if (cache->fd == -1) {
		snprintf(dbuf, sizeof(dbuf),
		    ">> can't open dst file %s, errno %d\n", fn, status);
		conversiondiag(pplayer, dbuf);

		return -1;
	}

	rc = prefs_cache_reload(pplayer, cache);

	if (rc == -1) {
		/* diag messages already issued by prefs_cache_reload() */
		prefs_close(cache->fd);
		return -1;
	}

	return 0;
};

/* prefs_cache_unload() deallocates the cache and closes the file
 * returns -1 if the argument is not an open cache, or NULL
 */
int
prefs_cache_unload(struct splayer *pplayer, struct prefs_cache *cache)
{
	if (!cache || (cache->fd == -1)) {
		return -1;
	}

	trie_collapse(cache->index, 1);
	trie_collapse(cache->freelist, 1);
	prefs_close(cache->fd);
	cache->index = cache->freelist = NULL;
	cache->fd = -1;
	return 0;
}

struct prefs_cache lorien_db_cache = { -1, { 0 }, (trie *)NULL, (trie *)NULL,
	PC_INVALID };

/* prefs_import_from_v173()
 *
 * takes the name of an old format password file (srcname)
 * and the name of a new or existing new format file (dstname)
 * and opens them, creating the new file if necessary and
 *
 * indexes the existing file, adding free entries to the free
 * list and normal entries by name.
 *
 * copies records, one at a time, from the old file into the
 * new file, skipping duplicates (via index) and re-using
 * free entries in the new file (via free list).
 *
 * the free list and index are merged, free list entries
 * use the reserved name space "freefreefreennnnnnnnnn"
 * where "nnnnnnnnnn" is the dboffset.
 *
 *
 * if the cache is not valid, it will be reloaded.  if reload fails,
 * the import will not commence, and -1 is returned.
 *
 * this routine will import one more record than it can cache,
 * and it will mark the index cache invalid in that case.
 * it does not attempt to recover by deallocating the free list.
 */

int
prefs_import_from_v173(char *srcname, char *dstname, struct splayer *pplayer)
{
	FILE *srcfile;
	//    int dstfd;
	int rc = 0;
	int status;
	size_t trie_match_len;
	trie *leaf;
	struct prefs_index *idxp;
	int imported = 0;

	memset(&v174rec, 0, sizeof(v174rec)); /* sets free = 0, dboffset = 0 */

	srcfile = fopen(srcname, "rb");

	if (!srcfile) {
		snprintf(dbuf, sizeof(dbuf), ">> can't open source file %s\n",
		    srcname);
		conversiondiag(pplayer, dbuf);

		return -1;
	}

#ifdef CONVERT
	rc = prefs_cache_load(pplayer, &lorien_db_cache, dstname);
#else
	if (!(lorien_db_cache.state & PC_INDEX_VALID)) {
		rc = prefs_cache_reload(pplayer, &lorien_db_cache);
#endif
	if (rc == -1) {
		snprintf(dbuf, sizeof(dbuf), ">> can't validate index cache\n");
#ifdef CONVERT
		if (lorien_db_cache.fd != -1)
			prefs_close(lorien_db_cache.fd);
#else
			if (pplayer)
#endif
		sendtoplayer(pplayer, dbuf);

		fclose(srcfile);
		return -1;
	}
#ifndef CONVERT
}
#endif

/* start at first data record, immediately after metadata */
rc = prefs_seek(lorien_db_cache.fd, prefs_compute_offset(1), &status);
if (rc != prefs_compute_offset(1)) {
	snprintf(dbuf, sizeof(dbuf), ">> can't position file pointer\n");
	conversiondiag(pplayer, dbuf);

#ifdef CONVERT
	prefs_close(lorien_db_cache.fd);
#endif
	fclose(srcfile);
	return -1;
}

while ((rc = fread(&v173rec, sizeof(v173rec), 1, srcfile)) == 1) {
	/* most db's prior to 174 will be the in-memory package file
	 * which had the same format as the old disk package,
	 * but didn't use the free flag.
	 * if you have a really really old db file that uses the free flag,
	 * uncomiment this next line
	 */
	/* if (v173rec.free) continue; */

	strncpy(v174rec.name, v173rec.name, sizeof(v174rec.name));
	v174rec.name[sizeof(v174rec.name) - 1] = (char)0;
	strncpy(v174rec.password, v173rec.pword, sizeof(v174rec.password));
	v174rec.password[sizeof(v174rec.password) - 1] = (char)0;
	v174rec.dboffset = 0;

	leaf = trie_match(lorien_db_cache.index, (unsigned char *)v174rec.name,
	    &trie_match_len, trie_keymatch_exact);

	if (leaf) {
		snprintf(dbuf, sizeof(dbuf),
		    ">> player %s exists in destination, skipping\n",
		    v174rec.name);
		conversiondiag(pplayer, dbuf);

		continue;
	}

	/* try first to use a record on the free list */
	leaf = trie_find_first(lorien_db_cache.freelist);
	if (leaf) {
		idxp = (struct prefs_index *)leaf->payload;
		if (idxp->free) {
			snprintf(dbuf, sizeof(dbuf),
			    ">> free entry found, reclaiming %d for %s\n",
			    idxp->dboffset, v174rec.name);
			conversiondiag(pplayer, dbuf);

			v174rec.dboffset = idxp->dboffset;
			if (!trie_delete(lorien_db_cache.freelist,
				(unsigned char *)idxp->name, 1)) {
				/* this is probably bad, corruption of the trie?
				 */
				snprintf(dbuf, sizeof(dbuf),
				    ">> can't dealloc free list entry %s@%d\n",
				    idxp->name, idxp->dboffset);
				conversiondiag(pplayer, dbuf);
			}
		}
	}

	rc = prefs_write(lorien_db_cache.fd, &v174rec, &status);
	if (rc != v174reclen(v174rec)) {
		snprintf(dbuf, sizeof(dbuf),
		    ">> unable to write record %s, errno %d\n", v173rec.name,
		    status);
		conversiondiag(pplayer, dbuf);

#ifdef CONVERT
		trie_collapse(lorien_db_cache.index, 1);
		trie_collapse(lorien_db_cache.freelist, 1);
#endif
		fclose(srcfile);
		return -1;
	}
	imported++;
	idxp = malloc(sizeof(struct prefs_index));
	if (idxp) {
		strcpy(idxp->name, v174rec.name);
		strcpy(idxp->password, v174rec.password);
		idxp->free = 0;
		idxp->dboffset = v174rec.dboffset;
		leaf = trie_add(lorien_db_cache.index,
		    (unsigned char *)idxp->name, idxp, &rc);
		if (!leaf) {
			lorien_db_cache.state &= ~PC_INDEX_VALID;
			/* bug:  look for a freelist entry?
			 * it is no big issue if we have to re-use
			 * a free-list index entry (thereby leaving a free
			 * record temporarily unreclaimable) in order to
			 * be able to index this record.
			 */
			snprintf(dbuf, sizeof(dbuf),
			    ">> can't alloc index entry for new %s,%d\n",
			    idxp->name, idxp->dboffset);
			conversiondiag(pplayer, dbuf);
			break;
		}
	} else {
		lorien_db_cache.state &= ~PC_INDEX_VALID;
		/* bug:  look for freelist entry and retry? see above */
		snprintf(dbuf, sizeof(dbuf),
		    ">> couldn't alloc index payload for new %s\n",
		    v174rec.name);
		conversiondiag(pplayer, dbuf);
		break;
	}
}
snprintf(dbuf, sizeof(dbuf), ">> imported %d records, index cache is %s\n",
    imported, (lorien_db_cache.state & PC_INDEX_VALID) ? "valid." : "invalid.");
#ifndef CONVERT
trie_collapse(lorien_db_cache.index, 1);
trie_collapse(lorien_db_cache.freelist, 1);
#endif
conversiondiag(pplayer, dbuf);
fclose(srcfile);
return imported;
}

#ifdef CONVERT
int
main(int argc, char *argv[])
{
	if (argc != 3) {
		printf(
		    "you must supply source and destination filenames\n\t%s src dst\n",
		    argv[0]);
		exit(1);
	}

	prefs_import_from_v173(argv[1], argv[2], NULL);

	exit(0);
}
#endif

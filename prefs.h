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

#ifndef _PREFS_H_
#define _PREFS_H_

#include "lorien.h"
#include "trie.h"

struct p_word {
	char name[LORIEN_V0173_NAME];
	char pword[LORIEN_V0173_PASS];
	int free;
};

struct prefs_index {
	char name[LORIEN_V0174_NAME];
	char password[LORIEN_V0174_PASS];
	int dboffset;
	int free;
};

struct metadata {
	int version;
};

typedef enum prefs_state {
	PC_INVALID = 0,
	PC_INDEX_VALID = 1,
	PC_FREE_VALID = 2,
	PC_VALID = 3
} prefs_state;

struct prefs_cache {
	int fd;
	struct metadata md;
	trie *index; /* index of active records and cache for passwords/names */
	trie *freelist; /* index of free records in the file */
	prefs_state state;
};

extern struct prefs_cache lorien_db_cache;

#define v174reclen(p) (((char *)&((p).dboffset)) - ((char *)&(p)))

void prefs_meta_ntoh(struct metadata *md);

void prefs_meta_hton(struct metadata *md);

void prefs_ntoh(struct splayer *p);

void prefs_hton(struct splayer *p);

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
int prefs_get_metadata(int fd, struct metadata *metadata, int *status);

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
int prefs_open(char *file, struct metadata *metadata, int *status);

int prefs_close(int fd);

/* prefs_reposition()
 * repositions to the start of the current record
 * and returns the offset.
 *
 * on failure, returns -1 and writes errno into
 * status
 */
int prefs_reposition(int fd, int *status);

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

int prefs_curr(int fd, struct splayer *player, int *status);

int prefs_compute_offset(int numrecords);

/* prefs_seek() seeks to absolute offset
 * and returns the offset or -1
 */
int prefs_seek(int fd, int offset, int *status);

/* prefs_next() seeks to the next record
 * returns the offset or -1 on fail.
 */

int prefs_next(int fd, int *status);
/* prefs_prev() seeks to the previous record
 * returns the offset or -1 on fail.
 */
int prefs_prev(int fd, int *status);

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
int prefs_write(int fd, struct splayer *player, int *status);

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
int prefs_delete(int fd, struct splayer *player, int *status);

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
 */

int prefs_import_from_v173(char *srcname, char *dstname,
    struct splayer *pplayer);
/* unimplemented:  prefs_defrag(srcname, dstname, pplayer) */

int prefs_cache_reload(struct splayer *pplayer, struct prefs_cache *cache);

int prefs_cache_find(struct prefs_cache *cache, char *name,
    struct splayer *prefs, int *status);

int prefs_cache_write(struct splayer *pplayer, struct prefs_cache *cache,
    struct splayer *prefs, int *status);

#endif /* _PREFS_H_ */

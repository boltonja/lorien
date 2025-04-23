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

#define _BAN_C_ 1
/* ban.c - banlist capability for Lorien
 */

#include <stdbool.h>

#include "ban.h"
#include "lorien.h"
#include "newplayer.h"
#include "parse.h"
#include "platform.h"

SLIST_HEAD(banlist, ban_item) banhead = SLIST_HEAD_INITIALIZER(banhead);

static int bans_read_from_db = 0;

int
ban_read_cb(struct ban_item *ban)
{
	bans_read_from_db++;
	return ban_add(ban->pattern, ban->owner, ban->created, false);
}

int
ban_read_db(void)
{
	(void)ldb_ban_scan(&lorien_db, ban_read_cb);
	return bans_read_from_db;
}

bool
ban_findsite(char *s)
{
	struct ban_item *target = NULL;
	struct ban_item *curr = NULL;

	SLIST_FOREACH(curr, &banhead, entries)
		if (strstr(s, curr->pattern)) {
			target = curr;
			break;
		}

	return target ? true : false;
}

int
ban_add(const char *s, const char *owner, time_t created, bool save_ban)
{
	struct ban_item *curr = malloc(sizeof(struct ban_item));

	if (curr) {
		strlcpy(curr->pattern, s, sizeof(curr->pattern));
		strlcpy(curr->owner, owner, sizeof(curr->owner));
		curr->created = created;

		if (save_ban) {
			int rc = ldb_ban_put(&lorien_db, curr);
			if (rc != 0) {
				free(curr);
				curr = NULL;
				goto out;
			}
		}
		SLIST_INSERT_HEAD(&banhead, curr, entries);
	}

out:
	return (curr) ? 1 : 0;
}

int
ban_remove(const char *s)
{
	struct ban_item *curr = NULL;
	int found = 0;

	SLIST_FOREACH(curr, &banhead, entries)
		if (!strncmp(curr->pattern, s, sizeof(curr->pattern) - 1)) {
			found = 1;
			break;
		}

	if (found) {
		int rc = ldb_ban_delete(&lorien_db, curr);
		if (!rc)
			SLIST_REMOVE(&banhead, curr, ban_item, entries);
		else
			found = 0;
	}

	return found;
}

parse_error
ban_list(struct splayer *who)
{
	char sendbuf[OBUFSIZE];
	struct ban_item *curr = NULL;
	char displayname[16];

	snprintf(sendbuf, sizeof(sendbuf),
		 ">> added by        pattern\r\n"
		 ">> --------------- -------------------\r\n");
	sendtoplayer(who, sendbuf);

	SLIST_FOREACH(curr, &banhead, entries) {
		strlcpy(displayname, curr->owner, sizeof(displayname));
		snprintf(sendbuf, sizeof(sendbuf), ">> %15s %s\r\n",
			 displayname, curr->pattern);
		sendtoplayer(who, sendbuf);
	}

	return PARSE_OK;
}

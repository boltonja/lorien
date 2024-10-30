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

#define _BAN_C_ 1
/* ban.c - banlist capability for Lorien
 */

#include "ban.h"
#include "lorien.h"
#include "newplayer.h"
#include "parse.h"
#include "platform.h"

struct ban_item *bantail;
struct ban_item *banlist = null(struct ban_item);

void
ban_read_blockfile(void)
{
	FILE *banfile;
	char *tmp;
	char s[BUFSIZE];

	banfile = fopen(BLOCKFILE, "r");
	if (banfile) {
		while (!feof(banfile)) {
			fgets(s, BUFSIZE - 1, banfile);
			if (!feof(banfile)) {
				if ((tmp = (char *)strstr(s, "\r")))
					*tmp = (char)0;
				if ((tmp = (char *)strstr(s, "\n")))
					*tmp = (char)0;
				ban_add(s);
			}
		}
		fclose(banfile);
	}
}

struct ban_item *
ban_findsite(char *s)
{
	struct ban_item *tmp;

	tmp = banlist;

	while (tmp) {
		if (strstr(s, tmp->site) && !tmp->free)
			return tmp;
		tmp = tmp->next;
	};
	return null(struct ban_item);
}

struct ban_item *
ban_findfree()
{
	struct ban_item *tmp = banlist;

	while (tmp) {
		bantail = tmp;
		if (tmp->free)
			return tmp;
		tmp = tmp->next;
	};
	return null(struct ban_item);
}

struct ban_item *
ban_alloc()
{
	struct ban_item *tmp;

	tmp = new (struct ban_item);
	if (tmp) {
		tmp->next = null(struct ban_item);
		tmp->free = 0;
	}

	return tmp;
}

struct ban_item *
ban_init()
{
	banlist = ban_alloc();
	return banlist;
}

int
ban_add(char *s)
{
	struct ban_item *curr;

	if ((curr = ban_findsite(s)))
		return 1;

	if (!banlist)
		curr = ban_init();
	else if ((curr = ban_findfree()))
		curr->free = 0;
	else {
		curr = ban_alloc();
		bantail->next = curr;
	}

	if (curr) {
		strncpy(curr->site, s, sizeof(curr->site) - 1);
		curr->site[sizeof(curr->site) - 1] = 0;
	}

	return (curr) ? 1 : 0;
}

char *
ban_remove(char *s)
{
	struct ban_item *tmp;
	tmp = ban_findsite(s);

	if (tmp) {
		tmp->free = 1;
		return tmp->site;
	}
	return (char *)0;
}

parse_error
ban_list(struct splayer *who)
{
	char sendbuf[OBUFSIZE];
	struct ban_item *tmp = banlist;

	snprintf(sendbuf, sizeof(sendbuf),
	    ">> The following sites are banned:\r\n");
	sendtoplayer(who, sendbuf);
	while (tmp) {
		if (!tmp->free) {
			snprintf(sendbuf, sizeof(sendbuf), ">> %s\r\n",
			    tmp->site);
			sendtoplayer(who, sendbuf);
		}
		tmp = tmp->next;
	}
	return PARSE_OK;
}

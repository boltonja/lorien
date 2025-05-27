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

/* channel.h - channel capability for Lorien.
 */
#ifndef _CHANNEL_H_
#define _CHANNEL_H_

#include <sys/queue.h>

#include <stdbool.h>

#include "db.h"
#include "parse.h"

struct channel {
	char name[MAX_CHAN];
	char owner[LORIEN_V0174_NAME];
	char desc[LORIEN_V0178_DESC];
	time_t created;

	/* stuff that doesn't need to go in the db goes below here */

	bool secure;
	int refcnt;
	bool persistent;
	SLIST_ENTRY(channel) entries;
};

struct channel *channel_add(const char *name);
int channel_count(const struct channel *channel);
void channel_del(struct channel *channel);
int channel_deref(struct channel *channel);
struct channel *channel_find(const char *name);
struct channel *channel_getmain(void);
char *channel_getname(const struct channel *channel, char *buf, int buflen);
void channel_init(void);
parse_error channel_list(struct splayer *splayer);
void channel_persist(struct channel *channel, bool persists);
bool channel_persists(const struct channel *channel);
int channel_ref(struct channel *channel);
void channel_rename(struct channel *channel, const char *name);
void channel_secure(struct channel *channel, bool secure);
bool channel_secured(const struct channel *channel);
#endif

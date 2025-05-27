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

#define _CHANNEL_C_ 1
/* channel.c - channels for Lorien
 */

#include <sys/queue.h>

#include <err.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>

#include "channel.h"
#include "log.h"
#include "newplayer.h"

SLIST_HEAD(chanlist, channel) channelhead = SLIST_HEAD_INITIALIZER(channelhead);

bool
channel_persists(const struct channel *chan)
{
	return chan->persistent;
}

bool
channel_secured(const struct channel *chan)
{
	return chan->secure;
}

void
channel_secure(struct channel *chan, bool secure)
{
	chan->secure = secure;
}

void
channel_init(void)
{
	struct channel *newc;

	if (SLIST_EMPTY(&channelhead)) {
		newc = calloc(1, sizeof(*newc));
		if (!newc) {
			errno = ENOMEM;
			err(EX_UNAVAILABLE, "cannot create main channel");
		}
		SLIST_INSERT_HEAD(&channelhead, newc, entries);
		strlcpy(newc->name, DEFCHAN, sizeof(newc->name));
		newc->persistent = true;
	}
}

struct channel *
channel_find(const char *name)
{
	struct channel *curr;

	SLIST_FOREACH(curr, &channelhead, entries)
		if (!strncmp(curr->name, name, MAX_CHAN))
			return curr;

	return (struct channel *)0;
}

struct channel *
channel_add(const char *name)
{
	struct channel *newchan = calloc(1, sizeof(*newchan));

	if (newchan) {
		strlcpy(newchan->name, name, MAX_CHAN);
		SLIST_INSERT_AFTER(SLIST_FIRST(&channelhead), newchan, entries);
	}
	return newchan;
}

void
channel_rename(struct channel *channel, const char *name)
{
	strlcpy(channel->name, name, sizeof(channel->name));
}

char *
channel_getname(const struct channel *channel, char *buf, int buflen)
{
	strlcpy(buf, channel->name, buflen);
	return buf;
}

struct channel *
channel_getmain(void)
{
	return SLIST_FIRST(&channelhead);
}

void
channel_del(struct channel *channel)
{
	if ((channel == SLIST_FIRST(&channelhead)) || channel->persistent)
		return;

	SLIST_REMOVE(&channelhead, channel, channel, entries);

	free(channel);
}

int
channel_count(const struct channel *channel)
{
	return channel->refcnt;
};

int
channel_ref(struct channel *channel)
{
	return ++channel->refcnt;
}

int
channel_deref(struct channel *channel)
{
	return --channel->refcnt;
}

parse_error
channel_list(struct splayer *pplayer)
{
	struct channel *curr;

	snprintf(sendbuf, sendbufsz, ">> %-13s %-10s %-6s %-6s\r\n", "Channel",
	    "# Users", "Secure", "Persists");
	sendtoplayer(pplayer, sendbuf);
	sendtoplayer(pplayer, ">> -------------------------------\r\n");
	SLIST_FOREACH(curr, &channelhead, entries) {
		snprintf(sendbuf, sendbufsz, ">> %-13s %-10d %-6s %-6s\r\n",
		    curr->name, curr->refcnt, (curr->secure) ? "Yes" : "No",
		    (curr->persistent) ? "Yes" : "No");
		sendtoplayer(pplayer, sendbuf);
	}
	return PARSE_OK;
}

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

/* ban.h - banlist capability for Lorien.
 */
#ifndef _BAN_H_
#define _BAN_H_

#include <sys/queue.h>
#include <stdbool.h>

#include "db.h"
#include "parse.h"

struct ban_item {
	SLIST_ENTRY(ban_item) entries;
	char pattern[LORIEN_V0178_BAN];
	char owner[LORIEN_V0174_NAME];
	time_t created;
};

int ban_add(const char *s, const char *owner, time_t created, bool save_ban);
bool ban_findsite(char *s);
parse_error ban_list(struct splayer *who);
int ban_read_db(void);
int ban_remove(char *s);

#endif

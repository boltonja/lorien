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

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "commands.h"
#include "log.h"
#include "lorien.h"
#include "newplayer.h"
#include "parse.h"
#include "trie.h"

#ifdef LT
#undef LT
#endif

#ifdef GT
#undef GT
#endif

#define LT(c1, c2) ((unsigned char)c1 < (unsigned char)c2)
#define GT(c1, c2) ((unsigned char)c1 > (unsigned char)c2)

struct parse_key *
parser_search_table(struct parse_context *context, char *pattern,
    size_t *matched)
{
	trie *leaf;

	leaf = trie_match(context->index, (unsigned char *)pattern,
	    strlen(pattern), matched, trie_keymatch_substring_first);

	return (leaf) ? leaf->payload : NULL;
}

int
parser_add_to_context(struct parse_context *context, struct parse_key *key)
{
	if (!(context && key && context->commands && context->index))
		return 0;

	if (trie_add(context->index, (unsigned char *)key->token,
		strlen(key->token), key, NULL)) {
		context->numentries++;
		return 1;
	} else
		return 0;
}

struct parse_context *
parser_new_dyncontext(struct command *commands)
{
	struct parse_context *ctxp;

	ctxp = malloc(sizeof(struct parse_context));

	if (ctxp) {
		if ((ctxp->index = trie_new())) {
			ctxp->numentries = 0;
			ctxp->isdynamic = true;
			ctxp->commands = commands;
		} else {
			free(ctxp);
			ctxp = NULL;
		}
	}

	return ctxp;
}

/* parser_collapse_dyncontext()
 * never call this on statically allocated
 * contexts that point to statically allocated parse keys
 * because it will attempt to free each of the keys
 */
int
parser_collapse_dyncontext(struct parse_context *context)
{
	if (!context || !context->isdynamic)
		return 0;

	trie_collapse(context->index, true);
	memset(context, 0, sizeof(*context));
	free(context);

	return 0;
}

int
parser_count_table_entries(struct parse_key *table)
{
	struct parse_key *cur;

	if (!table)
		return 0;

	for (cur = table; cur && cur->cmd; cur++)
		/* EMPTY */;

	return cur - table;
}

/* if out of resource, the resulting trie is collapsed, but
 * the caller must determine whether to free the context
 * (if said context is dynamic)
 */
int
parser_init_context(struct parse_context *context, struct parse_key *table,
    struct command *commands, bool isdynamic)
{
	struct parse_key *tp;

	if (!(context && table && commands))
		return 0;

	if (!(context->index = trie_new()))
		return 0;

	for (tp = table; tp->token[0]; tp++) {
		if (!trie_add(context->index, (unsigned char *)tp->token,
			strlen(tp->token), tp, NULL /* status */)) {
			trie_collapse(context->index, isdynamic);
			context->index = NULL;
			return 0;
		}
	}

	context->numentries = parser_count_table_entries(table);
	context->commands = commands;
	context->isdynamic = isdynamic;

	return 1;
}

int
parser_execute(struct splayer *pplayer, char *buf,
    struct parse_context *context)
{
	struct parse_key *entry;
	int rc;
	size_t matched;

	entry = parser_search_table(context, buf, &matched);
	if ((!entry) && (*buf == '.' || *buf == '/')) {
		return PARSERR_NOTFOUND;
	}

	if (!matched) { /* none matched and first character not '.' nor '/' */
		if (PLAYER_HAS(SCREAM, pplayer)) { /* handle the ever-popular
						      but annoying... */
			if (*buf != '>') { /* > will redirect to channel. */
				yell(pplayer, buf);
				return PARSE_OK;
			}
			/* redirect to channel */
			buf++; /* skip the '>' */
		}
		snprintf(sendbuf, sendbufsz, "(%d, %s) %s\r\n",
		    player_getline(pplayer), pplayer->name, buf);
		sendall(sendbuf, pplayer->chnl, 0);
		return PARSE_OK;
	}

	if (!entry)
		return PARSERR_NOTFOUND;

	/* the check against level exceeding max is paranoia */
	if ((pplayer->seclevel < context->commands[entry->cmd].seclevel) ||
	    (pplayer->seclevel > NUMLVL)) {
		sendtoplayer(pplayer, NO_PERM);
		return PARSERR_SUPPRESS;
	}

	char (*p)(struct splayer *pplayer);
	char (*p2)(struct splayer *pplayer, char *buf);
	char (*p3)(struct splayer *pplayer, char *buf, speechmode mode);

	p = (void *)context->commands[entry->cmd].func;
	p2 = (void *)context->commands[entry->cmd].func;
	p3 = (void *)context->commands[entry->cmd].func;

	switch (context->commands[entry->cmd].class) {
	case cmd_class_0:
		switch (context->commands[entry->cmd].numargs) {
		case 2:
			rc = (parse_error)p2(pplayer, buf + matched);
			break;
		case 1:
			rc = (parse_error)p(pplayer);
			break;
		default:
			rc = PARSERR_NUMARGS;
		}
		break;
	case cmd_class_1:
		switch (context->commands[entry->cmd].numargs) {
		case 3:
			rc = (parse_error)p3(pplayer, buf + matched,
			    SPEECH_NORMAL);
			break;
		default:
			rc = PARSERR_NUMARGS;
		}
		break;
	case cmd_class_2:
		/* Not yet implemented */
	default:
		return PARSERR_NOCLASS;
	}

	return (rc == PARSERR_SUPPRESS) ? PARSE_OK : rc;
}

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

#ifndef _PARSE_H_
#define _PARSE_H_

#include "lorien.h"
#include "trie.h"

#define bad_comm_prompt \
	">> error:  Unrecognized command.  Type /? for help.\r\n"
#define ambiguous_comm_prompt \
	">> error:  Ambiguous command.  Type /? for help.\r\n"
#define IVCMD_SYN   ">> error:  Invalid command syntax type /? for help.\r\n"
#define NO_PERM	    ">> error:  permission denied.\r\n"
#define DEAD_MSG    ">> fatal:  You've been killed.\r\n"
#define BEEPS_MSG   ">> /p beeps enabled.\r\n"
#define NOBEEPS_MSG ">> /p beeps disabled.\r\n"
#define NAME_MSG    ">> Name changed.\r\n"
#define YELL_MSG \
	">> error:  You are in hush mode.  You cannot send or receive yells.\r\n"
#define MESSAGE_MSG   ">> Arrival and departure messages enabled.\r\n"
#define NOMESSAGE_MSG ">> Arrival and departure message disabled.\r\n"
#define NO_CHAN_MSG \
	">> error:  Channel does not exist and cannot be created.\r\n"
#define NO_CHAN_CHANGE_MSG ">> warning:  Channel unchanged.\r\n"
#define HIGHLIGHT_MSG	   ">> /p Highlights enabled: %s \r\n"
#define HUSH_MSG	   ">> You are now in hush mode.  Yells will be suppressed.\r\n"
#define UNHUSH_MSG	   ">> You are no longer in hush mode.\r\n"
#define ECHO_MSG	   ">> /p echoing enabled.\r\n"
#define NOECHO_MSG	   ">> /p echoing disabled.\r\n"
#define EXIT_MSG	   ">> Hope you enjoyed your stay.\r\n"
#define BEEPS_NEWLINE	   "\007\r\n"
#define NOBEEPS_NEWLINE	   "\r\n"
#define IS_SECURE_MSG	   ">> error:  That channel is secure.\r\n"
#define SECURE_MSG	   ">> Channel secured by (%d) %s.\r\n"
#define UNSECURE_MSG	   ">> Channel unsecured by (%d) %s.\r\n"
#define NO_SECURE_MSG	   ">> error:  You cannot secure the main channel.\r\n"
#define NO_SECURE_PERSIST \
	">> error:  You cannot secure persistent channels.\r\n"
#define NO_LEVEL_MSG ">> Your security level will not show.\r\n"
#define LEVEL_MSG    ">> Your security level will show.\r\n"
#define MINWRAP_MSG  ">> error: Minimum wrap width is %d.\r\n"
#define WRAP_MSG     ">> VT200-style Auto-wrap enabled for %d columns.\r\n"
#define NO_WRAP_MSG  ">> Auto-wrap disabled.\r\n"
#define SCREAM_MSG \
	">> Yell mode enabled.  '>' escapes lines to your channel.\r\n"
#define NOSCREAM_MSG ">> Yell mode disabled.\r\n"
#define BAN_NOTFOUND ">> error:  pattern matches 0 ban list entries.\r\n"
#define PARSE_ARGS \
	">> parse tables corrupt, wrong number of arguments for command type.\n"
#define PARSE_CLASS \
	">> parse tables corrupt, class of selected command is unknown.\n"

enum {
	CMD_ADDCHANNEL,
	CMD_ADDPLAYER,
	CMD_BANADD,
	CMD_BANDEL,
	CMD_BANLIST,
	CMD_BEEPS,
	CMD_BROADCAST,
	CMD_BROADCAST2,
	CMD_DELPLAYER,
	CMD_DEMOTE,
	CMD_DOING,
	CMD_ECHO,
	CMD_FINGER,
	CMD_FORCE,
	CMD_GAG,
	CMD_GRANT,
	CMD_HELP,
	CMD_HILITE,
	CMD_HUSH,
	CMD_JOIN,
	CMD_KILL,
	CMD_KILLALL,
	CMD_MESSAGES,
	CMD_MODPLAYER,
	CMD_NAME,
	CMD_PARSER,
	CMD_PASSWORD,
	CMD_POSE,
	CMD_POST,
	CMD_PRINTPOWER,
	CMD_PROMOTE,
	CMD_PURGELOG,
	CMD_RESTOREPARSER,
	CMD_QUIT,
	CMD_READ,
	CMD_SCREAM,
	CMD_SECURE,
	CMD_SETMAIN,
	CMD_SETMAX,
	CMD_SHOWINFO,
	CMD_SHOWLEVEL,
	CMD_SHUTDOWN,
	CMD_STAGEPOSE,
	CMD_TUNE,
	CMD_UPTIME,
	CMD_WHISPER,
	CMD_WHO,
	CMD_WHO2,
	CMD_WRAP,
	CMD_YELL,
	CMD_ZZZMAXPLUS1
};

enum {
	cmd_class_0 = 0, /* arg1=player, arg2=buffer */
	cmd_class_1,	 /* arg1=player, arg2=buffer, arg3=SPEECH_nnnn */
	cmd_class_2
};

typedef enum parse_errors {
	PARSE_OK = 0,
	PARSERR_NOTFOUND = -1,	/* currently means not found or ambiguous */
	PARSERR_AMBIGUOUS = -2, /* not yet returned by parser */
	PARSERR_SUPPRESS = -3, /* returned by handlers if they have issued their
				  own diag messages */
	PARSERR_NUMARGS =
	    -4, /* the number of args for the selected command doesn't match any
		   known format for this cmd class */
	PARSERR_NOCLASS = -5 /* the class of the selected command is unknown */
} parse_error;

struct command {
	int cmd;   /* from the enum above */
	int class; /* only supported is 0:  arg1=player, arg2=buffer */
	int numargs;
	int seclevel; /* minimum seclevel required to invoke command */
	parse_error (
	    *func)(); /* the handler, returns PARSERR codes from enumeration or
			 PARSE_SUPPRESS if it handled error itself */
	char *name;   /* for matching while dynamically defining parse tables */
};

#define CMD_DECLS(cmd, class, num, lev, func) \
	{ cmd, class, num, lev, func, #cmd }
#define CMD_DECL(cmd, class, num, func) \
	{ cmd, class, num, JOEUSER, (void *)func, #cmd }

enum { PARSE_KEY_MAX = 50 };

struct parse_key { /* maps text tokens to commands */
	char token[PARSE_KEY_MAX];
	int cmd; /* from the enum above */
};

struct parse_context {
	int isdynamic;
	size_t numentries;
	/*     struct parse_key *table; */
	trie *index;
	struct command *commands;
};

int parser_execute(struct splayer *pplayer, char *buf,
    struct parse_context *context);

int parser_add_to_context(struct parse_context *context, struct parse_key *key);

struct parse_context *parser_new_dyncontext(struct command *commands);

/* parser_collapse_dyncontext()
 * never call this on statically allocated
 * contexts that point to statically allocated parse keys
 * because it will attempt to free each of the keys
 */
int parser_collapse_dyncontext(struct parse_context *context);

int parser_init_context(struct parse_context *context, struct parse_key *table,
    struct command *commands, int isdynamic);

int parser_count_table_entries(struct parse_key *table);

struct parse_key *parser_search_table(struct parse_context *, char *pattern,
    size_t *matched);
#endif /* _PARSE_H_ */

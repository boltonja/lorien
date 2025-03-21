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

/* lorien.h
Constants and type declarations for for Lorien.

Chris Eleveld      (The Insane Hermit)
Robert Slaven      (The Celestial Knight/Tigger)
Jillian Alana Bolton  (Creel)
Dave Mott          (Energizer Rabbit)
*/

#ifndef _LORIEN_H_
#define _LORIEN_H_

#include <sys/types.h>
#include <sys/select.h>

#include <time.h>

#include "db.h"
#include "utility.h"
#include "ring.h"

#ifndef _LORIEN_C_
extern size_t MAXCONN;
#else
size_t MAXCONN;
#endif

/* two constants for strcontains() */
#define EXACT	 1
#define SUB	 2
#define REGEX	 4 /* not yet implemented */

#define VERSION	 "1.7.6"      /* the version number. */
#define MAXARGS	 4	      /* the maximum number of args on a cmd line */

/* These are the security levels.
 *
 * Unfortunately, it is too easy for this commentary to lose synchronization
 * with the code.
 *
 * The canonical documentation on which levels can do what is the parser
 * table in commands.c.  The security levels are not run-time configurable,
 * even though the parser commands themselves are.  The parser will refuse
 * to execute a command if the user lacks the requisite security clearance.
 *
 * However, in addition to security levels, there are permissions flags
 * in the player struct which can prevent players who have the requisite
 * clearance from performing certain actions.  See the CAN_* constants
 * for details.
 *
 * There are a few checks that alter the scope of what certain commands
 * can do.  These have been catalogued in the command table in commands.c,
 * but if there is any doubt as to its correctness, most checks for security
 * check <= or >= so when you encounter them, they will say something about
 * the minimum level required to invoke a command.
 *
 * There are also comparisons of relative level, so that admins
 * of a given level may not perform power commands on admins of equal
 * level or higher.
 *
 * As a special case, see JOEUSER below.
 *
 * The security levels were named/designed/divided originally (in Lorien)
 * according to the following "model."
 *
 * JOEUSER - misnamed, is actually "in jail" and cannot speak.
 *           further, their speech is echoed back to them as if they are
 * speaking or yelling.  also, they have some restrictions on which commands
 *           they are allowed to use and some commands only pretend to work.
 *
 * BABYCO - any normal connected client, can use all user commands
 *           but is subject to permissions in the player struct (see flags)
 * ... levels in between gain additional commands
 * SUPREME - can perform any command
 * ARCHMAGE - can do what a sysop does, and demote/kick a misbehaving sysop.
 *
 */

enum {
	JOEUSER,  /* actually in jail, see above */
	BABYCO,	  /* default level for new connections, can do all non-admin
		     commands */
	COSYSOP,  /* lowest level that can add and remove bans */
	SYSOP,	  /* can perform some commands */
	SUPREME,  /* can perform any command */
	ARCHMAGE, /* can perform any command, can disconnect/demote SUPREME or
		     lower */
	NUMLVL	  /* the number of security levels */
};

typedef enum { SPEECH_NORMAL, SPEECH_YELL, SPEECH_PRIVATE } speechmode;

#define WELCOMEFILE "lorien.welcome"
#define BLOCKFILE   "lorien.block"
#define HELPFILE    "lorien.help"
#define LOGFILE	    "lorien.log"
#define PASSFILE    "lorien.pass"

#define LINE \
	"-------------------------------------------------------------------------------\r\n"

#ifndef USE_CONFIG_H
#ifndef DEFCHAN
#define DEFCHAN "main"
#endif
#ifndef DEFAULT_NAME
#define DEFAULT_NAME "A nameless newbie"
#endif
#else
#include "config.h"
#endif

extern char *player_flags_names[16];
extern char *player_privs_names[16];

/* values for flags */
enum player_flags {
	PLAYER_SHOW = 1,
	PLAYER_VRFY = 2,
	PLAYER_BEEPS = 4,
	PLAYER_MSG = 8,
	PLAYER_HUSH = 16,
	PLAYER_ECHO = 32,
	PLAYER_LEAVING = 64,
	PLAYER_WRAP = 128,
	PLAYER_INFO = 256,
	PLAYER_SCREAM = 512,
	PLAYER_SPAMMING = 1024,
	PLAYER_DEFAULT = PLAYER_SHOW | PLAYER_INFO | PLAYER_MSG,
	PLAYER_MAX_FLAG_BIT = 10,
	PLAYER_DONT_SAVE_MASK = PLAYER_LEAVING | PLAYER_SCREAM | PLAYER_SPAMMING
};

/* macros for flags */
#define PLAYER_CLR(f, p) (p->flags &= ~PLAYER_##f)
#define PLAYER_XOR(f, p) (p->flags ^= PLAYER_##f)
#define PLAYER_SET(f, p) (p->flags |= PLAYER_##f)
#define PLAYER_HAS(f, p) (p->flags & PLAYER_##f)

/* values for the privs */
enum player_privs {
	CANYELL = 1,
	CANWHISPER = 2,
	CANNAME = 4,
	CANCHANNEL = 8,
	CANQUIT = 16,
	CANCAPS = 32,
	CANPLAY = 64, /* if has set name */
	CANBOARD = 128,
	CANDEFAULT = CANYELL | CANWHISPER | CANNAME | CANCHANNEL | CANQUIT |
	    CANCAPS | CANBOARD,
	CAN_MAX_FLAG_BIT = 7 /* 0 - n */
};

/* status values */
#define NONE	0
#define MESSAGE 1
#define COMMAND 2

#define USAGE \
	"USAGE: lorien [-l file] [-d] portnumber\nusually just: lorien -d 2525\n"

#define null(p) (p *)0
#define new(p)	(p *)malloc(sizeof(p))

#define chan	struct channel_struct
extern time_t lorien_boot_time;

struct channel_struct {
	char name[MAX_CHAN + 1]; /* 11th is null character */
	int secure;
	int visited;
	int count;
	int persistent;
	int quota; /* the maximum size of the bulletin board, in entries */
	chan *next;
	struct ring_buffer *bulletins;
};

/* types for who list */
#define ALL		  (chan *)-3
#define CHANNEL		  (chan *)-1
#define ARRIVAL		  (chan *)-2
#define YELL		  (chan *)-4
#define INFORMATIONAL	  (chan *)-6
#define DEPARTURE	  (chan *)-5

struct splayer {
	int seclevel; /* how powerful are they? */
	int hilite;   /* a mask.  */
	int privs;
	int wrap;		 /* wrap column */
	int flags;		 /* hush, etc. */
	int pagelen;		 /* for paginating */
	char name[MAX_NAME];	 /* the player's name */
	char onfrom[MAX_NAME];	 /* where they are on from */
	char host[MAX_NAME];	 /* where they are really on from */
	char numhost[MAX_NAME];	 /* numeric address as text decimal octets */
	char password[MAX_PASS]; /* the players password */
	/* stuff that doesn't need to go in the db goes below here */
	/* for the code to work, dboffset should be the first item in the non-db
	 * stuff */
	time_t cameon;	   /* last login time */
	time_t playerwhen; /* creation date */
	time_t idle;	   /* number of seconds since player did anything */
	chan *chnl;
	struct splayer *next, *prev; /* the next player in the linked list */
	fd_set gags;
	int spamming;	    /* if the player is probably an e-mail spambot */
	char pbuf[BUFSIZE]; /* player buffer for accumulating text in char mode
			     */
	int dotspeeddial;   /* line number of the last person .p'd to */
	int s;		    /* also the line number */
	int port;	    /* remote port number */
};

int die(int exitstat);

#endif

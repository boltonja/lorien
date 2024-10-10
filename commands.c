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

/* parse.c
Routines for parsing command lines from players and handling
commands.

Chris Eleveld      (The Insane Hermit)
Robert Slaven      (The Celestial Knight/Tigger)
Jillian Alana Bolton  (Creel)
Dave Mott          (Energizer Rabbit)

*/
/* take the following out of comment to enable passwords. */

#include "ban.h"
#include "commands.h"
#include "files.h"
#include "help.h"
#include "log.h"
#include "lorien.h"
#include "newpass.h"
#include "newplayer.h"
#include "parse.h"
#include "platform.h"
#include "prefs.h"
#include "security.h"
#include "utility.h"

/* sorted list, binary search, ambiguous runs first match. */
struct parse_key default_parse_table[] = {
	{ "`", CMD_STAGEPOSE },
	{ ":", CMD_POSE },
	{ ";", CMD_POSE },
	{ "/+", CMD_PROMOTE },
	{ "/-", CMD_DEMOTE },
	{ "/?", CMD_HELP },
	{ "/Broadcast", CMD_BROADCAST },
	{ "/Changeprivs", CMD_GRANT },
	{ "/DeletePlayer", CMD_DELPLAYER },
	{ "/EnablePlayer", CMD_ADDPLAYER },
	{ "/Gag", CMD_GAG },
	{ "/Hilite", CMD_HILITE },
	{ "/Infotoggle", CMD_SHOWINFO },
	{ "/K", CMD_KILLALL },
	{ "/M", CMD_SETMAIN },
	{ "/Main", CMD_SETMAIN },
	{ "/ModifyMAXCONN", CMD_SETMAX },
	{ "/ModPlayer", CMD_MODPLAYER },
	{ "/Parser", CMD_PARSER },
	{ "/Password", CMD_PASSWORD },
	{ "/Purgelog", CMD_PURGELOG },
	{ "/Restoreparser", CMD_RESTOREPARSER },
	{ "/Who", CMD_WHO2 },
	{ "/Yellmode", CMD_SCREAM },
	{ "/a", CMD_YELL }, /* announce */
	{ "/addchannel", CMD_ADDCHANNEL },
	{ "/addplayer", CMD_ADDPLAYER },
	{ "/announce", CMD_YELL },
	{ "/b", CMD_BEEPS },
	{ "/ban", CMD_BANLIST },
	{ "/banadd", CMD_BANADD },
	{ "/bandel", CMD_BANDEL },
	{ "/banlist", CMD_BANLIST },
	{ "/beeps", CMD_BEEPS },
	{ "/channel", CMD_TUNE },
	{ "/doing", CMD_DOING },
	{ "/echo", CMD_ECHO },
	{ "/finger", CMD_FINGER },
	{ "/force", CMD_FORCE },
	{ "/gag", CMD_GAG },
	{ "/h", CMD_HUSH },
	{ "/help", CMD_HELP },
	{ "/hilite", CMD_HILITE },
	{ "/hush", CMD_HUSH },
	{ "/invisible", CMD_BROADCAST2 },
	{ "/kill", CMD_KILL },
	{ "/level", CMD_SHOWLEVEL },
	{ "/messages", CMD_MESSAGES },
	{ "/name", CMD_NAME },
	{ "/onfrom", CMD_DOING },
	{ "/p", CMD_WHISPER },
	{ "/plev", CMD_PRINTPOWER },
	{ "/post", CMD_POST },
	{ "/private", CMD_WHISPER },
	{ "/quit", CMD_QUIT },
	{ "/r", CMD_WRAP },
	{ "/read", CMD_READ },
	{ "/secure", CMD_SECURE },
	{ "/shutdown", CMD_SHUTDOWN },
	{ "/tune", CMD_TUNE },
	{ "/uptime", CMD_UPTIME },
	{ "/who", CMD_WHO },
	{ "/wrap", CMD_WRAP },
	{ "/yell", CMD_YELL },
	{ "", 0 },
};

struct splayer *pplayer;
char *command;

parse_error
setmain(struct splayer *pplayer, char *buf)
{
	buf = (char *)skipspace(buf);
	if (!*buf) {
		sendtoplayer(pplayer, ">> Invalid command syntax.\r\n");
		return PARSERR_SUPPRESS;
	} else {
		strncpy(channels->name, buf, MAX_CHAN);
		channels->name[MAX_CHAN - 1] = 0;
		snprintf(sendbuf, sizeof(sendbuf),
		    ">> main channel now \"%s\"\r\n", channels->name);
		sendall(sendbuf, ARRIVAL, pplayer->s);
		return PARSE_OK;
	}
}

parse_error
modPlayer(struct splayer *pplayer, char *buf)
{
	char lbuf[BUFSIZE];
	char *name;
	char *element;
	char *value;
	int correctargs = 0;
	struct splayer prefs;
	struct splayer *tplayer = NULL;
	struct prefs_index *idxp = NULL;
	int status, rc;
	size_t matched = 0;

	strncpy(lbuf, buf, sizeof(lbuf));
	lbuf[BUFSIZE - 1] = (char)0;
	name = skipspace(lbuf);
	if (*name) {
		element = strchr(name + 1, ' ');
		if (element) {
			*element = 0;
			element = skipspace(element + 1);
			if (*element) {
				value = strchr(element + 1, ' ');
				if (value) {
					*value = 0;
					value = skipspace(value + 1);
					if (*value)
						correctargs = 1;
				}
			}
		}
	}

	if (!correctargs) {
		return PARSERR_NUMARGS;
	}

	rc = prefs_cache_find(&lorien_db_cache, name, &prefs, &status);

	if (rc == -1) {
		snprintf(sendbuf, sizeof(sendbuf),
		    ">> Player %s is not registered.\r\n", name);
		sendtoplayer(pplayer, sendbuf);
		return PARSERR_SUPPRESS;
	} else if (rc < -1) {
		sendtoplayer(pplayer,
		    ">> Preferences database could not be read.\r\n");
		return PARSERR_SUPPRESS;
	}

	/* is target player logged on? */
	for (tplayer = players->next; tplayer; tplayer++)
		if (!strcmp(tplayer->name, name) && PLAYER_HAS(VRFY, pplayer))
			break;

	/* should we consider not permitting people to use registered names
	 * without logging in successfully?
	 * if no logged-on (verified) players are in code, tplayer will be NULL
	 */

	idxp = NULL;
	if (lorien_db_cache.state & PC_INDEX_VALID) {
		trie *t = trie_match(lorien_db_cache.index,
		    (unsigned char *)name, &matched, trie_keymatch_exact);
		if (t)
			idxp = t->payload;
	}

	if (tplayer && (tplayer->dboffset != prefs.dboffset)) { /* paranoia */
		snprintf(sendbuf, sizeof(sendbuf),
		    ">> Player %s in-core record offset doesn't match\r\n",
		    name);
		sendtoplayer(pplayer, sendbuf);
		return PARSERR_SUPPRESS;
	}
	if (idxp && (idxp->dboffset != prefs.dboffset)) { /* more paranoia */
		snprintf(sendbuf, sizeof(sendbuf),
		    ">> Player %s cached offset doesn't match\r\n", name);
		sendtoplayer(pplayer, sendbuf);
		return PARSERR_SUPPRESS;
	}

	switch (*element) {
	case 's': /* seclevel */
		if (pplayer->seclevel >
		    ((tplayer) ? tplayer->seclevel : prefs.seclevel)) {
			prefs.seclevel = atoi(value);
			if ((prefs.seclevel > 0) && (prefs.seclevel < NUMLVL)) {
				if (tplayer) /* if logged in, make in-core match
					      */
					tplayer->seclevel = prefs.seclevel;
			} else {
				snprintf(sendbuf, sizeof(sendbuf),
				    ">> %d is not a valid security level..\n",
				    prefs.seclevel);
				sendtoplayer(pplayer, sendbuf);
				return PARSERR_SUPPRESS;
			}
		}
		break;
	case 'p': /* password */;
		break;
		if (pplayer->seclevel >
		    ((tplayer) ? tplayer->seclevel : prefs.seclevel)) {
			prefs.seclevel = atoi(value);
			if (strlen(value) < MAX_PASS) {
				if (tplayer) /* if logged in, make in-core match
					      */
					strcpy(tplayer->password, value);
				strcpy(prefs.password, value);
				if (idxp)
					strcpy(idxp->password,
					    value); /* update cached entry */
			} else {
				snprintf(sendbuf, sizeof(sendbuf),
				    ">> %s is longer than %d characters\n",
				    value, MAX_PASS - 1);
				sendtoplayer(pplayer, sendbuf);
				return PARSERR_SUPPRESS;
			}
		}
	default:
		return PARSERR_AMBIGUOUS;
	}

	/* prefs write */
	rc = prefs_cache_write(pplayer, &lorien_db_cache, &prefs, &status);

	if (rc == -1) {
		snprintf(sendbuf, sizeof(sendbuf),
		    ">> Can't write record for %s\n", name);
		sendtoplayer(pplayer, sendbuf);
		return PARSERR_SUPPRESS;
	}
	return PARSE_OK;
}

parse_error
messages(struct splayer *pplayer)
{
	PLAYER_XOR(MSG, pplayer);
	if (PLAYER_HAS(MSG, pplayer))
		snprintf(sendbuf, sizeof(sendbuf), MESSAGE_MSG);
	else
		snprintf(sendbuf, sizeof(sendbuf), NOMESSAGE_MSG);
	sendtoplayer(pplayer, sendbuf);
	return PARSE_OK;
}

parse_error
setmax(struct splayer *pplayer, char *buf)
{
	int line_number;
	parse_error rc = PARSERR_SUPPRESS;

	buf = (char *)skipspace(buf);
	line_number = atoi(buf);
	if (line_number)
		MAXCONN = settablesize(line_number);
	else
		MAXCONN = gettablesize();
	snprintf(sendbuf, sizeof(sendbuf), ">> fd limit now %lu.\r\n", MAXCONN);
	rc = PARSE_OK;

	sendtoplayer(pplayer, sendbuf);
	return rc;
}

/* password code starts here. */
parse_error
enablePassword(struct splayer *pplayer, char *buf)
{
	parse_error rc = PARSERR_SUPPRESS;

	if (!enablepass(buf)) {
		snprintf(sendbuf, sizeof(sendbuf), IVCMD_SYN);
	} else {
		snprintf(sendbuf, sizeof(sendbuf),
		    ">> Password enabled for player.\r\n");
		PLAYER_SET(VRFY, pplayer);
		rc = PARSE_OK;
	};
	sendtoplayer(pplayer, sendbuf);

	return rc;
}

parse_error
bulletin_read(struct splayer *pplayer, char *buf)
{
	return PARSERR_NOTFOUND;
}

parse_error
bulletin_post(struct splayer *pplayer, char *buf)
{
	if (!(pplayer->flags & CANBOARD)) {
		snprintf(sendbuf, sizeof(sendbuf), NO_PERM);
		sendtoplayer(pplayer, sendbuf);
		return PARSERR_SUPPRESS;
	}

	return PARSERR_NOTFOUND;
}

parse_error
changePassword(struct splayer *pplayer, char *buf)
{
	parse_error rc = PARSERR_SUPPRESS;

	if (!PLAYER_HAS(VRFY, pplayer)) {
		snprintf(sendbuf, sizeof(sendbuf), bad_comm_prompt);
	} else {
		if (!newpass(pplayer->name, buf)) {
			snprintf(sendbuf, sizeof(sendbuf), IVCMD_SYN);
		} else {
			snprintf(sendbuf, sizeof(sendbuf),
			    ">> Password changed on Lorien.\r\n");
			rc = PARSE_OK;
		};
	};
	sendtoplayer(pplayer, sendbuf);
	return rc;
}

parse_error
delete_player(struct splayer *pplayer, char *buf)
{
	parse_error rc = PARSERR_SUPPRESS;

	if (pfree(buf)) {
		passwrite();
		snprintf(sendbuf, sizeof(sendbuf),
		    ">> Player deleted from database.\r\n");
		rc = PARSE_OK;
	} else {
		snprintf(sendbuf, sizeof(sendbuf),
		    ">> Name not found in database.\r\n");
	};
	sendtoplayer(pplayer, sendbuf);
	return rc;
}

parse_error
set_name(struct splayer *pplayer, char *buf)
{
	char *buf2;
	struct p_word pstruct;

	if (!(pplayer->privs & CANNAME)) {
		sendtoplayer(pplayer, NO_PERM);
		return PARSERR_SUPPRESS;
	}
	buf = (char *)skipspace(buf);
	buf2 = strchr(buf, '=');

#ifdef PASSWORDS_ENABLED
	if (buf2 != (char *)0) {
		buf2[0] = 0x0;
		buf2++;
		strncpy(pstruct.name, buf, MAX_NAME - 1);
		if (!plookup(&pstruct)) {
			snprintf(sendbuf, sizeof(sendbuf),
			    ">> Name not found in database.\r\n");
			sendtoplayer(pplayer, sendbuf);
		} else {
			if (strcmp(pstruct.pword, buf2)) {
				snprintf(sendbuf, sizeof(sendbuf),
				    ">> Invalid password.\r\n");
				sendtoplayer(pplayer, sendbuf);
			} else {
				PLAYER_SET(VRFY, pplayer);
				setname(pplayer->s, buf);
				snprintf(sendbuf, sizeof(sendbuf),
				    ">> Name changed.\r\n");
				sendtoplayer(pplayer, sendbuf);

				if (!(pplayer->privs & CANPLAY)) {
					/* let everyone else know who's here. */
					snprintf(sendbuf, sizeof(sendbuf),
					    ">> New arrival on line %d from %s.\r\n",
					    pplayer->s, pplayer->host);
					sendall(sendbuf, ARRIVAL, 0);
					pplayer->privs |= CANPLAY;
					pplayer->chnl = channels;
					channels->count++;
				};
			};
		};
	} else {
		strncpy(pstruct.name, buf, MAX_NAME - 1);
		if (!plookup(&pstruct)) {
#endif
			setname(pplayer->s, buf);
			PLAYER_CLR(VRFY, pplayer);
			snprintf(sendbuf, sizeof(sendbuf),
			    ">> Name changed.\r\n");
			sendtoplayer(pplayer, sendbuf);
			if (!(pplayer->privs & CANPLAY)) {
				/* let everyone else know who's here. */
				snprintf(sendbuf, sizeof(sendbuf),
				    ">> New arrival on line %d from %s.\r\n",
				    pplayer->s, pplayer->host);
				sendall(sendbuf, ARRIVAL, 0);
				pplayer->privs |= CANPLAY;
				pplayer->chnl = channels;
				channels->count++;
			};
#ifdef PASSWORDS_ENABLED
		} else {
			snprintf(sendbuf, sizeof(sendbuf),
			    ">> That name is reserved.  Please use another.\r\n");
			sendtoplayer(pplayer, sendbuf);
		};
	};
#endif
	return PARSE_OK;
}

/* password code ends here. */

void
pose_it(struct splayer *pplayer, char *buf)
{
	if (*buf == ',' || *buf == '\'')
		snprintf(sendbuf, sizeof(sendbuf), "(%d) %s%s\r\n", pplayer->s,
		    pplayer->name, buf);
	else
		snprintf(sendbuf, sizeof(sendbuf), "(%d) %s %s\r\n", pplayer->s,
		    pplayer->name, buf);
	sendall(sendbuf, pplayer->chnl, 0);
}

parse_error
stagepose(struct splayer *pplayer, char *buf, speechmode mode)
{
	static char fmt_posespace[40] = "(%d) [to %s] %s %s\r\n";
	static char fmt_posenospace[40] = "(%d) [to %s] %s%s\r\n";
	static char fmt_yellposespace[40] = "(*%d*) [to %s] %s %s\r\n";
	static char fmt_yellposenospace[40] = "(*%d*) [to %s] %s%s\r\n";
	static char fmt_yellstage[40] = "(*%d*) %s [to %s] %s\r\n";
	static char fmt_stage[40] = "(%d) %s [to %s] %s\r\n";
	char *fmtstring;
	int linenum;
	struct splayer *who, *next;

	if (isdigit(buf[0])) {
		linenum = atoi(buf);
		buf = (char *)skipdigits(buf);

		who = lookup(linenum);
		if (who == (struct splayer *)0) {
			snprintf(sendbuf, sizeof(sendbuf),
			    ">> error:  Player %d does not exist.\r\n",
			    linenum);
			sendtoplayer(pplayer, sendbuf);
		} else {
			buf = (char *)skipspace(buf);
			if (*buf == ':' || *buf == ';') {
				buf++;
				buf = (char *)skipspace(buf);
				if (*buf == ',' || *buf == '\'')
					switch (mode) {
					case SPEECH_PRIVATE:
					case SPEECH_NORMAL:
						fmtstring = fmt_posenospace;
						break;
					case SPEECH_YELL:
						fmtstring = fmt_yellposenospace;
						break;
					}
				else
					switch (mode) {
					case SPEECH_PRIVATE:
					case SPEECH_NORMAL:
						fmtstring = fmt_posespace;
						break;
					case SPEECH_YELL:
						fmtstring = fmt_yellposespace;
						break;
					}
				snprintf(sendbuf, sizeof(sendbuf), fmtstring,
				    pplayer->s, who->name, pplayer->name, buf);
				if (pplayer->seclevel > JOEUSER)
					sendall(sendbuf, pplayer->chnl, 0);
				else
					sendtoplayer(pplayer, sendbuf);
			} else {
				buf = (char *)skipspace(buf);
				fmtstring = (mode == SPEECH_YELL) ?
				    fmt_yellstage :
				    fmt_stage;
				snprintf(sendbuf, sizeof(sendbuf), fmtstring,
				    pplayer->s, pplayer->name, who->name, buf);
				if (pplayer->seclevel > JOEUSER) {
					if (mode == SPEECH_YELL) {
						who = players->next;
						while (who) {
							next = who->next;
							sendtoplayer(who,
							    sendbuf);
							who = next;
						}
					} else
						sendall(sendbuf, pplayer->chnl,
						    0);
				} else
					sendtoplayer(pplayer, sendbuf);
			}
		}
	} else {
		snprintf(sendbuf, sizeof(sendbuf), bad_comm_prompt);
		sendtoplayer(pplayer, sendbuf);
	}

	return PARSE_OK;
}

parse_error
force(struct splayer *pplayer, char *buf)
{
	parse_error rc = PARSERR_SUPPRESS;

	if (isdigit(buf[0])) {
		int linenum;
		struct splayer *who;

		linenum = atoi(buf);
		buf = (char *)skipdigits(buf);

		who = lookup(linenum);
		if (who == (struct splayer *)0) {
			snprintf(sendbuf, sizeof(sendbuf),
			    ">> error:  Player %d does not exist. \r\n",
			    linenum);
			sendtoplayer(pplayer, sendbuf);
		} else {
			if (who->seclevel < pplayer->seclevel) {
				buf = (char *)skipspace(buf);

				strcpy(who->pbuf, buf);
				processinput(who);
				rc = PARSE_OK;
			} else {
				snprintf(sendbuf, sizeof(sendbuf),
				    ">> %s has at least as much authority as you.\r\n",
				    who->name);
				sendtoplayer(pplayer, sendbuf);
				snprintf(sendbuf, sizeof(sendbuf),
				    ">> %s just tried to force you...\r\n",
				    pplayer->name);
				sendtoplayer(who, sendbuf);
			}
		}
	} else {
		snprintf(sendbuf, sizeof(sendbuf), bad_comm_prompt);
		sendtoplayer(pplayer, sendbuf);
	};
	return rc;
}

parse_error
gag_Player(struct splayer *pplayer, char *buf)
{
	int line_number;
	struct splayer *who;
	parse_error rc = PARSERR_SUPPRESS;

	line_number = atoi(buf);
	who = lookup(line_number);

	if (!who) {
		snprintf(sendbuf, sizeof(sendbuf),
		    ">> error:  Player %d does not exist!\r\n", line_number);
	} else if ((line_number >= 1) && (line_number < MAXCONN)) {
		rc = PARSE_OK;
		if (FD_ISSET(line_number, &pplayer->gags)) {
			FD_CLR(line_number, &pplayer->gags);
			snprintf(sendbuf, sizeof(sendbuf),
			    ">> Player %d ungagged.\r\n", line_number);
		} else {
			FD_SET(line_number, &pplayer->gags);
			snprintf(sendbuf, sizeof(sendbuf),
			    ">> Player %d gagged.\r\n", line_number);
		}
	} else {
		snprintf(sendbuf, sizeof(sendbuf), bad_comm_prompt);
	};

	sendtoplayer(pplayer, sendbuf);
	return rc;
}

parse_error
change_onfrom(struct splayer *pplayer, char *buf)
{
	buf = (char *)skipspace(buf);
	if (!*buf)
		sendtoplayer(pplayer, bad_comm_prompt);
	else {
		strncpy(pplayer->onfrom, buf, MAX_NAME - 1);
		pplayer->onfrom[MAX_NAME - 1] = (char)0x0;
		snprintf(sendbuf, sizeof(sendbuf),
		    ">> Host information changed to %s\r\n", pplayer->onfrom);
		sendtoplayer(pplayer, sendbuf);
	}
	return PARSE_OK;
}

parse_error
finger(struct splayer *pplayer, char *instring)
{
	struct splayer *who;
	char maskbuf[BUFSIZE];
	int currfd, gagcount;
	int line;
	char *buf;

	buf = (char *)skipspace(instring);
	if (!(*buf)) {
		return wholist3(pplayer);
	} else if (!isdigit(*buf)) {
		sendtoplayer(pplayer, bad_comm_prompt);
		return PARSERR_SUPPRESS;
	}

	line = atoi(buf);
	who = lookup(line);

	if (!who) {
		snprintf(sendbuf, sizeof(sendbuf),
		    ">> error:  Player %d does not exist!\r\n", line);
		sendtoplayer(pplayer, sendbuf);
		return PARSERR_SUPPRESS;
	}

	snprintf(sendbuf, sizeof(sendbuf), ">> Name: %s\r\n", who->name);
	sendtoplayer(pplayer, sendbuf);
	snprintf(sendbuf, sizeof(sendbuf), ">> Channel: %s %s\r\n",
	    who->chnl ? (who->chnl->name) : "***none***",
	    who->chnl ? ((who->chnl->secure) ? "(Secured)" : "") : "");
	sendtoplayer(pplayer, sendbuf);
#ifdef ONFROM_ANY
	snprintf(sendbuf, sizeof(sendbuf), ">> Doing: %s\r\n", who->onfrom);
	sendtoplayer(pplayer, sendbuf);
#endif

	snprintf(sendbuf, sizeof(sendbuf), ">> On From: %s (%s:%d)\r\n",
	    who->host, who->numhost, who->port);
	sendtoplayer(pplayer, sendbuf);

	if ((pplayer->seclevel > BABYCO) || (pplayer == who)) {
		snprintf(sendbuf, sizeof(sendbuf), ">> Gags: ");
		for (currfd = 1, gagcount = 0; currfd <= MAXCONN; currfd++) {
			if (FD_ISSET(currfd, &who->gags)) {
				gagcount++;
				snprintf(maskbuf, sizeof(maskbuf), "%d ",
				    currfd);
				strncat(sendbuf, maskbuf,
				    (sizeof(sendbuf) - 1) - strlen(sendbuf));
				if (gagcount && !(gagcount % 8)) {
					strcat(maskbuf, "\r\n");
					sendtoplayer(pplayer, sendbuf);
					snprintf(sendbuf, sizeof(sendbuf),
					    ">>       ");
				}
			}
		}
		if (!(gagcount)) {
			strcat(sendbuf, "None\r\n");
			sendtoplayer(pplayer, sendbuf);
		} else if ((gagcount % 8)) {
			strcat(sendbuf, "\r\n");
			sendtoplayer(pplayer, sendbuf);
		}
	}

	snprintf(sendbuf, sizeof(sendbuf), ">> Toggles enabled: %s\r\n",
	    mask2string32(who->flags, PLAYER_MAX_FLAG_BIT, maskbuf,
		sizeof(maskbuf), player_flags_names, ", "));
	sendtoplayer(pplayer, sendbuf);
	snprintf(sendbuf, sizeof(sendbuf), ">> Toggles disabled: %s\r\n",
	    mask2string32(~(who->flags), PLAYER_MAX_FLAG_BIT, maskbuf,
		sizeof(maskbuf), player_flags_names, ", "));
	sendtoplayer(pplayer, sendbuf);
	snprintf(sendbuf, sizeof(sendbuf), ">> Hilites: %s\r\n",
	    (who->hilite) ? mask2string(who->hilite, maskbuf, hi_types, ", ") :
			    "None");
	sendtoplayer(pplayer, sendbuf);

	snprintf(sendbuf, sizeof(sendbuf), ">> Idle: %s\tWrap width: %d\r\n",
	    idlet(who->idle), who->wrap);
	sendtoplayer(pplayer, sendbuf);

	snprintf(sendbuf, sizeof(sendbuf), ">> Privileges granted: %s\r\n",
	    mask2string32(who->privs, CAN_MAX_FLAG_BIT, maskbuf,
		sizeof(maskbuf), player_privs_names, ", "));
	sendtoplayer(pplayer, sendbuf);
	snprintf(sendbuf, sizeof(sendbuf), ">> Privileges revoked: %s\r\n",
	    mask2string32(~(who->privs), CAN_MAX_FLAG_BIT, maskbuf,
		sizeof(maskbuf), player_privs_names, ", "));
	sendtoplayer(pplayer, sendbuf);

	snprintf(sendbuf, sizeof(sendbuf), ">> End of info\r\n");
	sendtoplayer(pplayer, sendbuf);
	return PARSE_OK;
}

parse_error
change_channel(struct splayer *pplayer, char *buf)
{
	chan *oldc;
	chan *newc;
	struct splayer *who;
	int linenumber;

	if (!(pplayer->privs & CANCHANNEL)) {
		sendtoplayer(pplayer, NO_PERM);
		return PARSERR_SUPPRESS;
	}
	if (isdigit(*buf) && (pplayer->seclevel >= SYSOP)) {
		/* we want to change someone elses channel */

		linenumber = atoi(buf);

		buf = (char *)skipdigits(buf);

		if (!*buf) {
			sendtoplayer(pplayer, bad_comm_prompt);
			return PARSERR_SUPPRESS;
		}

		buf = (char *)skipspace(buf);
		if (!*buf) {
			sendtoplayer(pplayer, bad_comm_prompt);
			return PARSERR_SUPPRESS;
		}

		who = lookup(linenumber);

		if (!who) {
			snprintf(sendbuf, sizeof(sendbuf),
			    ">> error:  Player %d does not exist!\r\n",
			    linenumber);
			sendtoplayer(pplayer, sendbuf);
			return PARSERR_SUPPRESS;
		}

		if (who->seclevel >= pplayer->seclevel) {
			snprintf(sendbuf, sizeof(sendbuf),
			    ">> %s has at least as much authority as you!\r\n",
			    who->name);
			sendtoplayer(pplayer, sendbuf);
			snprintf(sendbuf, sizeof(sendbuf),
			    ">> %s just tried to change your channel.\r\n",
			    pplayer->name);
			sendtoplayer(who, sendbuf);
			return PARSERR_SUPPRESS;
		}

		strcpy(who->pbuf, "/c");
		strcat(who->pbuf, buf);
		processinput(who);
		snprintf(sendbuf, sizeof(sendbuf),
		    ">> (%d) %s placed on channel %s.\r\n", who->s, who->name,
		    who->chnl->name);
		sendtoplayer(pplayer, sendbuf);

	}
#ifdef NO_NUMBER_CHANNELS
	else if (isdigit(buf[1]))
		sendtoplayer(pplayer, bad_comm_prompt);
#endif
	else {
		buf = (char *)skipspace(buf);

		if (!strlen(buf)) {
			newc = channels;

			snprintf(sendbuf, sizeof(sendbuf),
			    ">> %-13s %-10s %-6s %-6s\r\n", "Channel",
			    "# Users", "Secure", "Persists");
			sendtoplayer(pplayer, sendbuf);
			sendtoplayer(pplayer,
			    ">> -------------------------------\r\n");

			while (newc) {
				snprintf(sendbuf, sizeof(sendbuf),
				    ">> %-13s %-10d %-6s %-6s\r\n", newc->name,
				    newc->count, (newc->secure) ? "Yes" : "No",
				    (newc->persistent) ? "Yes" : "No");
				sendtoplayer(pplayer, sendbuf);
				newc = newc->next;
			}
			return PARSE_OK;
		}

		if (pplayer->seclevel < BABYCO) {
			sendtoplayer(pplayer, ">> Channel changed.\r\n");
			return PARSE_OK;
		};

		newc = findchannel(buf);

		if (newc == pplayer->chnl) {
			sendtoplayer(pplayer, NO_CHAN_CHANGE_MSG);
			return PARSE_OK;
		}

		if (!newc)
			newc = newchannel(buf);

		if (!newc) {
			sendtoplayer(pplayer, NO_CHAN_MSG);
			return PARSERR_SUPPRESS;
		}

		if (newc->secure) {
			sendtoplayer(pplayer, IS_SECURE_MSG);
			return PARSERR_SUPPRESS;
		}

		oldc = pplayer->chnl;
		oldc->count--;

		if (oldc->count <= 0) {
			remove_channel(oldc);
			oldc = (chan *)0;
		}

		newc->count++;

		snprintf(sendbuf, sizeof(sendbuf), ">> (%d) %s has joined.\r\n",
		    pplayer->s, pplayer->name);
		sendall(sendbuf, newc, 0);

		pplayer->chnl = newc;

		snprintf(sendbuf, sizeof(sendbuf), ">> Channel changed.\r\n");
		sendtoplayer(pplayer, sendbuf);

		snprintf(sendbuf, sizeof(sendbuf),
		    ">> (%d) %s has wandered off.\r\n", pplayer->s,
		    pplayer->name);
		if (oldc)
			sendall(sendbuf, oldc, 0);
	}

	return PARSE_OK;
}

parse_error
whisper(struct splayer *pplayer, char *buf)
{
	char buf2[2048];
	char hbuf[40];
	char hbuf2[40];
	static char formatposespace[20] = "%s(%d,p) %s %s%s%s";
	static char formatposenospace[20] = "%s(%d,p) %s%s%s%s";
	char *fmtstring;
	int linenum;
	struct splayer *who;

	if (!(pplayer->privs & CANWHISPER)) {
		sendtoplayer(pplayer, NO_PERM);
		return PARSERR_SUPPRESS;
	}
	if (pplayer->seclevel > JOEUSER) {
		if (isdigit(*buf)) {
			pplayer->dotspeeddial = linenum = atoi(buf);
			buf = (char *)skipdigits(buf);
		} else {
			linenum = pplayer->dotspeeddial;
			if (isspace(*buf))
				buf++;
		}

		if (linenum) {
			who = lookup(linenum);
			if (who == (struct splayer *)0) {
				snprintf(sendbuf, sizeof(sendbuf),
				    ">> error:  Player %d does not exist.\r\n",
				    linenum);
				sendtoplayer(pplayer, sendbuf);
			} else {
				buf = (char *)skipspace(buf);
				if (*buf == ':' || *buf == ';') {
					buf++;
					buf = (char *)skipspace(buf);
					if (*buf == ',' || *buf == '\'')
						fmtstring = formatposenospace;
					else
						fmtstring = formatposespace;
					snprintf(sendbuf, sizeof(sendbuf),
					    fmtstring,
					    (who->hilite) ?
						expand_hilite(who->hilite,
						    hbuf) :
						"",
					    pplayer->s, pplayer->name, buf,
					    (who->hilite) ?
						expand_hilite(0L, hbuf2) :
						"",
					    PLAYER_HAS(BEEPS, who) ?
						BEEPS_NEWLINE :
						NOBEEPS_NEWLINE);
					snprintf(buf2, sizeof(buf2),
					    ">> Pose sent to %s : %s %s\r\n",
					    who->name, pplayer->name, buf);
				} else {
					buf = (char *)skipspace(buf);
					snprintf(sendbuf, sizeof(sendbuf),
					    "%s(%d,p %s) %s%s%s",
					    (who->hilite) ?
						expand_hilite(who->hilite,
						    hbuf) :
						"",
					    pplayer->s, pplayer->name, buf,
					    (who->hilite) ?
						expand_hilite(0L, hbuf2) :
						"",
					    PLAYER_HAS(BEEPS, who) ?
						BEEPS_NEWLINE :
						NOBEEPS_NEWLINE);
					snprintf(buf2, sizeof(buf2),
					    ">> /p sent to %s : %s\r\n",
					    who->name, buf);
				}
				if (!isgagged(who, pplayer->s))
					sendtoplayer(who, sendbuf);

				if (!PLAYER_HAS(ECHO, pplayer)) {
					sendtoplayer(pplayer,
					    ">> /p sent.\r\n");
				} else {
					sendtoplayer(pplayer, buf2);
				}
			}
		} else {
			snprintf(sendbuf, sizeof(sendbuf), bad_comm_prompt);
			sendtoplayer(pplayer, sendbuf);
		}
	} else {
		snprintf(sendbuf, sizeof(sendbuf), ">> /p sent.\r\n");
		sendtoplayer(pplayer, sendbuf);
	}
	return PARSE_OK;
}

parse_error
change_privs(struct splayer *pplayer, char *buf)
{
	int line;
	struct splayer *who;

	buf = (char *)skipspace(buf);

	if (!isdigit(*buf)) {
		snprintf(sendbuf, sizeof(sendbuf),
		    ">> Bad player number in command.\r\n");
		sendtoplayer(pplayer, sendbuf);
		return PARSERR_SUPPRESS;
	}

	line = atoi(buf);

	who = lookup(line);

	if (!who) {
		snprintf(sendbuf, sizeof(sendbuf),
		    ">> error:  Player %d does not exist!\r\n", line);
		sendtoplayer(pplayer, sendbuf);
		return PARSERR_SUPPRESS;
	}

	if (who->seclevel >= pplayer->seclevel) {
		snprintf(sendbuf, sizeof(sendbuf),
		    ">> %s has at least as much authority as you.\r\n",
		    who->name);
		sendtoplayer(pplayer, sendbuf);
		snprintf(sendbuf, sizeof(sendbuf),
		    ">> (%d) %s just tried to change your privileges.\r\n",
		    pplayer->s, pplayer->name);
		sendtoplayer(who, sendbuf);
		return PARSERR_SUPPRESS;
	}

	buf = (char *)skipspace(skipdigits(buf));

	while (*buf) {
		*buf = toupper(*buf);
		switch (*buf) {
		case 'Y':
			if (who->privs & CANYELL) {
				snprintf(sendbuf, sizeof(sendbuf),
				    ">> Player %d is not allowed to '\''yell.\r\n",
				    line);
				sendtoplayer(pplayer, sendbuf);
				who->privs &= ~CANYELL;
			} else {
				snprintf(sendbuf, sizeof(sendbuf),
				    ">> Player %d may now yell.\r\n", line);
				sendtoplayer(pplayer, sendbuf);
				who->privs |= CANYELL;
			};
			break;
		case 'W':
			if (who->privs & CANWHISPER) {
				snprintf(sendbuf, sizeof(sendbuf),
				    ">> Player %d is not allowed to whisper.\r\n",
				    line);
				sendtoplayer(pplayer, sendbuf);
				who->privs &= ~CANWHISPER;
			} else {
				snprintf(sendbuf, sizeof(sendbuf),
				    ">> Player %d may now whisper.\r\n", line);
				sendtoplayer(pplayer, sendbuf);
				who->privs |= CANWHISPER;
			};
			break;
		case 'N':
			if (who->privs & CANNAME) {
				snprintf(sendbuf, sizeof(sendbuf),
				    ">> Player %d is not allowed to set own name.\r\n",
				    line);
				sendtoplayer(pplayer, sendbuf);
				who->privs &= ~CANNAME;
			} else {
				snprintf(sendbuf, sizeof(sendbuf),
				    ">> Player %d may now set own name.\r\n",
				    line);
				sendtoplayer(pplayer, sendbuf);
				who->privs |= CANNAME;
			};
			break;
		case 'Q':
			if (who->privs & CANQUIT) {
				snprintf(sendbuf, sizeof(sendbuf),
				    ">> Player %d is not allowed to quit.\r\n",
				    line);
				sendtoplayer(pplayer, sendbuf);
				who->privs &= ~CANQUIT;
			} else {
				snprintf(sendbuf, sizeof(sendbuf),
				    ">> Player %d may now quit.\r\n", line);
				sendtoplayer(pplayer, sendbuf);
				who->privs |= CANQUIT;
			};
			break;
		case 'T':
			if (who->privs & CANCHANNEL) {
				snprintf(sendbuf, sizeof(sendbuf),
				    ">> Player %d is not allowed to change channels\r\n",
				    line);
				sendtoplayer(pplayer, sendbuf);
				who->privs &= ~CANCHANNEL;
			} else {
				snprintf(sendbuf, sizeof(sendbuf),
				    ">> Player %d may now change channels.\r\n",
				    line);
				sendtoplayer(pplayer, sendbuf);
				who->privs |= CANCHANNEL;
			};
			break;
		case 'C':
			if (who->privs & CANCAPS) {
				snprintf(sendbuf, sizeof(sendbuf),
				    ">> Player %d is not allowed to use capital letters.\r\n",
				    line);
				sendtoplayer(pplayer, sendbuf);
				who->privs &= ~CANCAPS;
			} else {
				snprintf(sendbuf, sizeof(sendbuf),
				    ">> Player %d may now use capital letters.\r\n",
				    line);
				sendtoplayer(pplayer, sendbuf);
				who->privs |= CANCAPS;
			};
			break;
		case 'B':
			if (who->privs & CANBOARD) {
				snprintf(sendbuf, sizeof(sendbuf),
				    ">> Player %d is not allowed to post bulletins.\r\n",
				    line);
				sendtoplayer(pplayer, sendbuf);
				who->privs &= ~CANBOARD;
			} else {
				snprintf(sendbuf, sizeof(sendbuf),
				    ">> Player %d may now post bulletins.\r\n",
				    line);
				sendtoplayer(pplayer, sendbuf);
				who->privs |= CANBOARD;
			};
			break;
		}
		buf++;
	}
	return PARSE_OK;
}

parse_error
yell(struct splayer *pplayer, char *buf)
{
	char *fmtstring;
	static char formatposespace[20] = "(*%d*) %s %s\r\n";
	static char formatposenospace[20] = "(*%d*) %s%s\r\n";
	struct splayer *who, *next;

	if (!(pplayer->privs & CANYELL)) {
		sendtoplayer(pplayer, NO_PERM);
		return PARSERR_SUPPRESS;
	}

	if (PLAYER_HAS(HUSH, pplayer)) {
		snprintf(sendbuf, sizeof(sendbuf), YELL_MSG);
		sendtoplayer(pplayer, sendbuf);
		return PARSERR_SUPPRESS;
	};

	if (pplayer->seclevel >= JOEUSER) {
		buf = (char *)skipspace(buf);
		if (*buf == '`') {
			return stagepose(pplayer, buf + 1, SPEECH_YELL);
		}

		if (*buf == ':' || *buf == ';') {
			buf = (char *)skipspace(buf + 1);

			if (*buf == ',' || *buf == '\'')
				fmtstring = formatposenospace;
			else
				fmtstring = formatposespace;

			snprintf(sendbuf, sizeof(sendbuf), fmtstring,
			    pplayer->s, pplayer->name, buf);
		} else {
			snprintf(sendbuf, sizeof(sendbuf), "(*%d, %s*) %s\r\n",
			    pplayer->s, pplayer->name, buf);
		}
		if ((pplayer->seclevel) > JOEUSER) {
			who = players->next;
			while (who) {
				next = who->next;
				if (!PLAYER_HAS(HUSH, who) &&
				    !isgagged(who, pplayer->s))
					sendtoplayer(who, sendbuf);
				who = next;
			}
		} else /* Joe should think it was sent, but it wasnt */
			sendtoplayer(pplayer, sendbuf);
	};
	return PARSE_OK;
}

parse_error
echo(struct splayer *pplayer)
{
	PLAYER_XOR(ECHO, pplayer);
	if (PLAYER_HAS(ECHO, pplayer)) {
		snprintf(sendbuf, sizeof(sendbuf), ECHO_MSG);
	} else
		snprintf(sendbuf, sizeof(sendbuf), NOECHO_MSG);
	sendtoplayer(pplayer, sendbuf);
	return PARSE_OK;
}

parse_error
hush(struct splayer *pplayer)
{
	/* if unhushed and screaming, can't scream no more. */
	if (PLAYER_HAS(SCREAM, pplayer) && !PLAYER_HAS(HUSH, pplayer)) {
		snprintf(sendbuf, sizeof(sendbuf), NOSCREAM_MSG);
		sendtoplayer(pplayer, sendbuf);
		PLAYER_XOR(SCREAM, pplayer);
	}

	PLAYER_XOR(HUSH, pplayer);
	if (PLAYER_HAS(HUSH, pplayer))
		snprintf(sendbuf, sizeof(sendbuf), HUSH_MSG);
	else
		snprintf(sendbuf, sizeof(sendbuf), UNHUSH_MSG);
	sendtoplayer(pplayer, sendbuf);
	return PARSE_OK;
}

parse_error
scream(struct splayer *pplayer)
{
	if (PLAYER_HAS(HUSH, pplayer)) {
		snprintf(sendbuf, sizeof(sendbuf), YELL_MSG);
		sendtoplayer(pplayer, sendbuf);
	} else {
		PLAYER_XOR(SCREAM, pplayer);
		if (PLAYER_HAS(SCREAM, pplayer))
			snprintf(sendbuf, sizeof(sendbuf), SCREAM_MSG);
		else
			snprintf(sendbuf, sizeof(sendbuf), NOSCREAM_MSG);
		sendtoplayer(pplayer, sendbuf);
	}
	return PARSE_OK;
}

parse_error
promote(struct splayer *pplayer, char *buf)
{
	int linenum;
	struct splayer *who;

	linenum = atoi(buf);
	buf = (char *)skipdigits(buf);

	who = lookup(linenum);
	if (who == (struct splayer *)0) {
		snprintf(sendbuf, sizeof(sendbuf),
		    ">> error:  Player %d does not exist.\r\n", linenum);
		sendtoplayer(pplayer, sendbuf);
	} else {
		if ((who->seclevel + 1) >= pplayer->seclevel) {
			snprintf(sendbuf, sizeof(sendbuf),
			    ">> %s's security level is too high.\r\n",
			    who->name);
			sendtoplayer(pplayer, sendbuf);
		} else {
			who->seclevel++;
			snprintf(sendbuf, sizeof(sendbuf),
			    "%s(%d) promoted %s(%d)", pplayer->name,
			    pplayer->seclevel, who->name, who->seclevel);
			log_msg(sendbuf);
			snprintf(sendbuf, sizeof(sendbuf),
			    ">> %s promoted.\r\n", who->name);
			sendtoplayer(pplayer, sendbuf);
			snprintf(sendbuf, sizeof(sendbuf),
			    ">> You have been promoted.\r\n");
			sendtoplayer(who, sendbuf);
		}
	}
	return PARSE_OK;
}

parse_error
demote(struct splayer *pplayer, char *buf)
{
	int linenum;
	struct splayer *who;

	linenum = atoi(buf);
	buf = (char *)skipdigits(buf);

	who = lookup(linenum);
	if (who == (struct splayer *)0) {
		snprintf(sendbuf, sizeof(sendbuf),
		    ">> error:  Player %d does not exist.\r\n", linenum);
		sendtoplayer(pplayer, sendbuf);
	} else {
		if (who->seclevel < pplayer->seclevel) {
			who->seclevel--;
			snprintf(sendbuf, sizeof(sendbuf), ">> %s demoted.\r\n",
			    who->name);
			sendtoplayer(pplayer, sendbuf);
		} else {
			snprintf(sendbuf, sizeof(sendbuf),
			    ">> You don't have authority over %s!\r\n",
			    who->name);
			sendtoplayer(pplayer, sendbuf);
			snprintf(sendbuf, sizeof(sendbuf),
			    ">> %s just tried to demote you!\r\n",
			    pplayer->name);
			sendtoplayer(who, sendbuf);
		}
	}
	return PARSE_OK;
}

parse_error
kill_player(struct splayer *pplayer, char *buf)
{
	int linenum;
	struct splayer *who;

	linenum = atoi(buf);
	buf = (char *)skipdigits(buf);

	who = lookup(linenum);
	if (who == (struct splayer *)0) {
		snprintf(sendbuf, sizeof(sendbuf),
		    ">> error:  Player %d does not exist.\r\n", linenum);
		sendtoplayer(pplayer, sendbuf);
	} else {
		if (who->seclevel < pplayer->seclevel) {
			snprintf(sendbuf, sizeof(sendbuf), DEAD_MSG);
			sendtoplayer(who, sendbuf);
			snprintf(sendbuf, sizeof(sendbuf),
			    ">> %s(%d) killed.\r\n", who->name, who->s);
			sendtoplayer(pplayer, sendbuf);
			snprintf(sendbuf, sizeof(sendbuf), "%s killed %s.",
			    pplayer->name, who->name);
			log_msg(sendbuf);
			removeplayer(who);
		} else {
			snprintf(sendbuf, sizeof(sendbuf),
			    ">> You don't have authority over %s!\r\n",
			    who->name);
			sendtoplayer(pplayer, sendbuf);
			snprintf(sendbuf, sizeof(sendbuf),
			    ">> %s just tried to kill you!\r\n", pplayer->name);
			sendtoplayer(who, sendbuf);
			snprintf(sendbuf, sizeof(sendbuf),
			    "%s tried to kill %s.", pplayer->name, who->name);
			log_msg(sendbuf);
		}
	}
	return PARSE_OK;
}

parse_error
kill_all_players(struct splayer *pplayer, char *buf)
{
	struct splayer *curr;
	struct splayer *next;

	next = players->next;

	while (next) {
		curr = next;
		next = curr->next;
		if (curr->seclevel < SUPREME)
			removeplayer(curr);
	}
	return PARSE_OK;
}

parse_error
broadcast(struct splayer *pplayer, char *buf)
{
	snprintf(sendbuf, sizeof(sendbuf),
	    ">> Broadcast message from (%d) %s : %s\r\n", pplayer->s,
	    pplayer->name, buf);
	sendall(sendbuf, ALL, 0);
	return PARSE_OK;
}

parse_error
broadcast2(struct splayer *pplayer, char *buf)
{
	int line;
	struct splayer *who;

	if (*buf != ' ')
		line = atoi(buf);
	else
		line = 0;
	if (!line) {
		buf = (char *)skipspace(buf);
		snprintf(sendbuf, sizeof(sendbuf), "%s\r\n", buf);
		sendall(sendbuf, INFORMATIONAL, 0);

		/* 	    snprintf(sendbuf, sizeof(sendbuf), ">> global info
		 * |%s| sent\r\n",buf); */
		/* 	    sendtoplayer(pplayer,sendbuf); */
		/* 	    log_msg(sendbuf); */
	} else {
		who = lookup(line);

		/* 	    snprintf(sendbuf, sizeof(sendbuf), "private info
		 * |%s| targeted %d.\r\n", buf,line); */
		/* 	    sendtoplayer(pplayer,sendbuf); */
		/* 	    log_msg(sendbuf); */

		buf = (char *)skipspace(skipdigits(buf));
		snprintf(sendbuf, sizeof(sendbuf), "%s\r\n", buf);
		if (who) {
			sendtoplayer(who, sendbuf);
			/* 		snprintf(sendbuf, sizeof(sendbuf),">>
			 * sent to %s.\r\n", who->name); */
			/* 		sendtoplayer(pplayer,sendbuf); */
		} else {
			snprintf(sendbuf, sizeof(sendbuf),
			    ">> error:  Player %d does not exist.\r\n", line);
			sendtoplayer(pplayer, sendbuf);
			/*		log_msg(sendbuf); */
		}
	}
	return PARSE_OK;
}

parse_error
playerquit(struct splayer *pplayer, char *buf)
{
	int oldlevel = pplayer->seclevel;

	pplayer->seclevel = changelevel(buf, pplayer);

	if (pplayer->seclevel == -1) {
		if (pplayer->privs & CANQUIT) {
#ifndef NO_LOG_CONNECT
			snprintf(sendbuf, sizeof(sendbuf), "%s was on from %s",
			    pplayer->name, pplayer->host);
			log_msg(sendbuf);
#endif
			snprintf(sendbuf, sizeof(sendbuf), EXIT_MSG);
			sendtoplayer(pplayer, sendbuf);
			PLAYER_SET(LEAVING,
			    pplayer); /* this causes handleinput() to remove */
		}
		pplayer->seclevel = oldlevel;
		snprintf(sendbuf, sizeof(sendbuf), NO_PERM);
		sendtoplayer(pplayer, sendbuf);
	}
	return PARSE_OK;
}

parse_error
playerwrap(struct splayer *pplayer, char *buf)
{
	int cols = 0;
	buf = (char *)skipspace(buf + 1);
	if (*buf)
		cols = atoi(buf);

	if (cols) {
		if (cols < 4) {
			snprintf(sendbuf, sizeof(sendbuf), MINWRAP_MSG, 4);
			sendtoplayer(pplayer, sendbuf);
		} else {
			pplayer->wrap = cols;
			PLAYER_SET(WRAP, pplayer);
		}
	} else
		PLAYER_XOR(WRAP, pplayer);

	if (PLAYER_HAS(WRAP, pplayer)) {
		if (!pplayer->wrap)
			pplayer->wrap = 80; /* default to 80 */
		snprintf(sendbuf, sizeof(sendbuf), WRAP_MSG, pplayer->wrap);
		sendtoplayer(pplayer, sendbuf);
	} else
		sendtoplayer(pplayer, NO_WRAP_MSG);

	return PARSE_OK;
}

parse_error
info_toggle(struct splayer *pplayer)
{
	PLAYER_XOR(INFO, pplayer);
	if (PLAYER_HAS(INFO, pplayer))
		snprintf(sendbuf, sizeof(sendbuf),
		    ">> /i messages enabled.\r\n");
	else
		snprintf(sendbuf, sizeof(sendbuf),
		    ">> /i messages suppressed.\r\n");

	sendtoplayer(pplayer, sendbuf);
	return PARSE_OK;
}

parse_error
level_toggle(struct splayer *pplayer)
{
	PLAYER_XOR(SHOW, pplayer);

	if (!PLAYER_HAS(SHOW, pplayer))
		snprintf(sendbuf, sizeof(sendbuf), NO_LEVEL_MSG);
	else
		snprintf(sendbuf, sizeof(sendbuf), LEVEL_MSG);
	sendtoplayer(pplayer, sendbuf);
	return PARSE_OK;
}

parse_error
secure_channel(struct splayer *pplayer, char *buf)
{
	/* should we implement op secure/unsecure of channel? */
	/* need to implement securing of channel you are subscribed to
	 * i.e. .s mychan
	 */
	if (pplayer->chnl == channels) {
		sendtoplayer(pplayer, NO_SECURE_MSG);
	} else if (pplayer->chnl->persistent) {
		sendtoplayer(pplayer, NO_SECURE_PERSIST);
	} else {
		if (pplayer->chnl->secure) {
			snprintf(sendbuf, sizeof(sendbuf), UNSECURE_MSG,
			    pplayer->s, pplayer->name);
			sendall(sendbuf, pplayer->chnl, 0);
		} else {
			snprintf(sendbuf, sizeof(sendbuf), SECURE_MSG,
			    pplayer->s, pplayer->name);
			sendall(sendbuf, pplayer->chnl, 0);
		}
		pplayer->chnl->secure = !pplayer->chnl->secure;
	}
	return PARSE_OK;
}

parse_error
add_ban(struct splayer *pplayer, char *buf)
{
	buf = (char *)skipspace(buf);
	ban_add(buf);
	snprintf(sendbuf, sizeof(sendbuf), ">> %s added to banlist.\r\n", buf);
	sendall(sendbuf, ALL, pplayer->s);
	return PARSE_OK;
}

parse_error
beeps(struct splayer *pplayer)
{
	PLAYER_XOR(BEEPS, pplayer);
	if (PLAYER_HAS(BEEPS, pplayer))
		snprintf(sendbuf, sizeof(sendbuf), BEEPS_MSG);
	else
		snprintf(sendbuf, sizeof(sendbuf), NOBEEPS_MSG);
	sendtoplayer(pplayer, sendbuf);
	return PARSE_OK;
}

parse_error
hilites(struct splayer *pplayer, char *buf)
{
	if (construct_mask(buf, &pplayer->hilite) < 0) {
		snprintf(sendbuf, sizeof(sendbuf),
		    ">> Error parsing hilite string.\r\n");
		sendtoplayer(pplayer, sendbuf);
	} else {
		char tmpstr[BUFSIZE];
		memset(tmpstr, 0, BUFSIZE);
		snprintf(sendbuf, sizeof(sendbuf), HIGHLIGHT_MSG,
		    mask2string(pplayer->hilite, tmpstr, hi_types, ","));
		sendtoplayer(pplayer, sendbuf);
	};
	return PARSE_OK;
}

parse_error
delete_ban(struct splayer *pplayer, char *buf)
{
	char *s;
	buf = (char *)skipspace(buf);
	s = ban_remove(buf);
	if (s) {
		snprintf(sendbuf, sizeof(sendbuf),
		    ">> %s removed from banlist.\r\n", s);
		sendall(sendbuf, ALL, pplayer->s);
	} else
		sendtoplayer(pplayer, BAN_NOTFOUND);
	return PARSE_OK;
}

parse_error
format_uptime(struct splayer *pplayer)
{
	snprintf(sendbuf, sizeof(sendbuf),
	    ">> Lorien %s has been running for %s.\r\n", VERSION,
	    timelet(lorien_boot_time, 40000));
	sendtoplayer(pplayer, sendbuf);
	snprintf(sendbuf, sizeof(sendbuf),
	    ">> %d of a possible %lu lines are in use.\r\n", numconnected(),
	    MAXCONN - 4);
	sendtoplayer(pplayer, sendbuf);
	return PARSE_OK;
}

parse_error
add_channel(struct splayer *pplayer, char *buf)
{
	new_channelp(buf);
	return PARSE_OK;
}

void
prelogon(struct splayer *pplayer, char *buf)
{
#ifdef ANTISPAM
	if (pplayer->spamming) {
		if (!strcmp(buf, ".")) {
			snprintf(sendbuf, sizeof(sendbuf),
			    "250 2.0.0 ZZZZZZZZ Message logged, you criminal, now Go Away\r\n");
			sendtoplayer(pplayer, sendbuf);
			pplayer->spamming = 0;
		} else
			; /* NULL */
	} else
#endif
	{
		if (!strncmp(buf, ".q", 2) || !strncmp(buf, "/q", 2)) {
			playerquit(pplayer, ++buf);
		} else {
			if (*buf == '.' || *buf == '/')
				buf++;

			switch (*(buf)) {
#ifdef ANTISPAM
			case 'E':
			case 'H':
				snprintf(sendbuf, sizeof(sendbuf),
				    "250 rootaction.net Hello spam [0.0.0.0], logging your every move\r\n");
				sendtoplayer(pplayer, sendbuf);
				return;
				break;
			case 'M':
				snprintf(sendbuf, sizeof(sendbuf),
				    "250 2.1.0 spammer@nowhere... Go Away\r\n");
				sendtoplayer(pplayer, sendbuf);
				return;
				break;
			case 'R':
				snprintf(sendbuf, sizeof(sendbuf),
				    "250 2.1.5 spammer@nowhere... Go Away\r\n");
				sendtoplayer(pplayer, sendbuf);
				return;
				break;
			case 'D':
				snprintf(sendbuf, sizeof(sendbuf),
				    "354 Enter spam, end with %c.%c on a line by itself\r\n",
				    34, 34);
				pplayer->spamming = 1;
				sendtoplayer(pplayer, sendbuf);
				return;
				break;
#endif
			case 'n':
				set_name(pplayer, buf + 1);
				break;
			case '?':
				showhelp(pplayer, &buf[1]);
				break;
			default:;
			};

			if (!(pplayer->privs & CANPLAY)) {
				snprintf(sendbuf, sizeof(sendbuf),
				    "220 You must choose a name, use .n YOURNAME to set your name.\r\n220 Type .? for help.\r\n");
				sendtoplayer(pplayer, sendbuf);
			}
		}
	}
}

struct command commands[] = { CMD_DECLS(CMD_ADDCHANNEL, 0, 2, COSYSOP,
				  add_channel),
	CMD_DECLS(CMD_ADDPLAYER, 0, 2, SUPREME, enablePassword),
	CMD_DECLS(CMD_BANADD, 0, 2, COSYSOP, add_ban),
	CMD_DECLS(CMD_BANDEL, 0, 2, COSYSOP, delete_ban),
	CMD_DECL(CMD_BANLIST, 0, 1, ban_list), CMD_DECL(CMD_BEEPS, 0, 1, beeps),
	CMD_DECLS(CMD_BROADCAST, 0, 2, SYSOP, broadcast),
	CMD_DECLS(CMD_BROADCAST2, 0, 2, SUPREME, broadcast2),
	CMD_DECLS(CMD_DELPLAYER, 0, 2, SUPREME, delete_player),
	CMD_DECLS(CMD_DEMOTE, 0, 2, SYSOP, demote),
#ifdef ONFROM_ANY
#define CAN_SET_DOING BABYCO
#else
#define CAN_SET_DOING SYSOP
#endif
	CMD_DECLS(CMD_DOING, 0, 2, CAN_SET_DOING, change_onfrom),
	CMD_DECL(CMD_ECHO, 0, 1, echo),
	/* BABYCO can see their own gags with finger.
	 * SYSOP and higher can see who others have gagged.
	 */
	CMD_DECL(CMD_FINGER, 0, 2, finger),
	CMD_DECLS(CMD_FORCE, 0, 2, SUPREME, force),
	CMD_DECL(CMD_GAG, 0, 2, gag_Player),
	CMD_DECLS(CMD_GRANT, 0, 2, SUPREME, change_privs),
	CMD_DECL(CMD_HELP, 0, 2, showhelp), CMD_DECL(CMD_HILITE, 0, 2, hilites),
	CMD_DECL(CMD_HUSH, 0, 1, hush),
	/* JOEUSER can't change channel, but they get told that they did.
	 * BABYCO can change own channel.
	 * SYSOP and higher can move players to channels.
	 */
	CMD_DECL(CMD_JOIN, 0, 2, change_channel),
	CMD_DECLS(CMD_KILL, 0, 2, SYSOP, kill_player),
	CMD_DECLS(CMD_KILLALL, 0, 2, SUPREME, kill_all_players),
	CMD_DECL(CMD_MESSAGES, 0, 1, messages),
	CMD_DECLS(CMD_MODPLAYER, 0, 2, SUPREME, modPlayer),
	CMD_DECL(CMD_NAME, 0, 2, set_name),
	CMD_DECLS(CMD_PARSER, 0, 2, SUPREME, install_parser_from_file),
	CMD_DECL(CMD_PASSWORD, 0, 2, changePassword),
	CMD_DECL(CMD_POSE, 0, 2, pose_it),
	CMD_DECL(CMD_POST, 0, 2, bulletin_post),
	CMD_DECLS(CMD_PRINTPOWER, 0, 1, ARCHMAGE, printlevels),
	CMD_DECLS(CMD_PROMOTE, 0, 2, SYSOP, promote),
	CMD_DECLS(CMD_PURGELOG, 0, 1, SUPREME, purgelog),
	CMD_DECLS(CMD_RESTOREPARSER, 0, 1, SYSOP, restore_default_commands),
	/* quit invokes change_level(), which allows modifiation of the
	 * in-core passwords if SUPREME or higher.
	 */
	CMD_DECL(CMD_QUIT, 0, 2, playerquit),
	CMD_DECL(CMD_READ, 0, 2, bulletin_read),
	CMD_DECL(CMD_SCREAM, 0, 1, scream),
	CMD_DECL(CMD_SECURE, 0, 2, secure_channel),
	CMD_DECLS(CMD_SETMAIN, 0, 2, SYSOP, setmain),
	CMD_DECLS(CMD_SETMAX, 0, 2, SUPREME, setmax),
	CMD_DECL(CMD_SHOWINFO, 0, 1, info_toggle),
	CMD_DECLS(CMD_SHOWLEVEL, 0, 1, SYSOP, level_toggle),
	CMD_DECLS(CMD_SHUTDOWN, 0, 1, SUPREME, haven_shutdown),
	/* JOEUSER can't really stage pose, the code pretends to do it for
	 * for them, but will not actually show the results to others.
	 */
	CMD_DECL(CMD_STAGEPOSE, 1, 3, stagepose),
	/* SEE CMD_JOIN for restrictions */
	CMD_DECL(CMD_TUNE, 0, 2, change_channel),
	CMD_DECL(CMD_UPTIME, 0, 1, format_uptime),
	/* JOEUSER can't whisper anyone, even though it appears to
	 * them as if they can.  They can't speak either, see handleinput().
	 */
	CMD_DECL(CMD_WHISPER, 0, 2, whisper), CMD_DECL(CMD_WHO, 0, 2, wholist),
	CMD_DECL(CMD_WHO2, 0, 2, wholist2),
	CMD_DECL(CMD_WRAP, 0, 2, playerwrap),
	/* JOEUSER can't yell, but thinks they can.
	 */
	CMD_DECL(CMD_YELL, 0, 2, yell), { 0, 0, 0, 0, NULL, NULL } };

static int default_parser_entries =
    0; /* the number of entries in default_parse_table */
static struct parse_context *main_parser_context;
static struct parse_context default_parser_context;

parse_error
restore_default_commands(struct splayer *pplayer)
{
	if (pplayer) {
		sendtoplayer(pplayer, ">> default commands restored.\n");
	}
	if (main_parser_context) {
		if (main_parser_context->isdynamic) {
			parser_collapse_dyncontext(main_parser_context);
		}
		/* it is conceivable that there may be multiple static contexts
		 */
		if (default_parser_context.index) {
			main_parser_context = &default_parser_context;
		} else {
			/* handlecommand() will install it
			 * this case is interesting because it currently
			 * isn't possible to install a dynamic command
			 * table without first executing an admin command,
			 * and handlecommand() should have already set up
			 * the default table.
			 *
			 * However, eventually we want to read commands from
			 * lorien.config on startup, so it will eventually
			 * be possible to have done so.
			 */
			main_parser_context = NULL;
		}
	} else {
		/* else:  we didn't have a parser context and could never have
		 * arrived here! */
		return PARSERR_NOTFOUND;
	}
	return PARSE_OK;
}

parse_error
install_parser_from_file(struct splayer *pplayer, char *filename)
{
	char *cp, *tp;
	FILE *parserfile;
	char buf[BUFSIZE];
	struct parse_context *newctxp;
	char key[PARSE_KEY_MAX];
	char command[PARSE_KEY_MAX];
	struct parse_key *keyp;
	struct command *commp;

	if (!filename || !*filename)
		return PARSERR_SUPPRESS;

	newctxp = parser_new_dyncontext(commands);

	if (!newctxp)
		return PARSERR_SUPPRESS;

	/* only open files in the current dir */
	for (cp = filename + strlen(filename); cp != filename; cp--) {
		if (*cp == '/' || *cp == '\\') {
			cp++;
			break;
		}
	}
	cp = skipspace(cp);
	parserfile = fopen(cp, "r");
	snprintf(sendbuf, sizeof(sendbuf), ">> file %s, \r\n", cp);

	if (!parserfile) {
		if (pplayer) {
			snprintf(sendbuf, sizeof(sendbuf),
			    ">> can't open parser file |%s|\r\n", cp);
			sendtoplayer(pplayer, sendbuf);
		}
		parser_collapse_dyncontext(newctxp);
		return PARSERR_SUPPRESS;
	}

	while (fgets(buf, sizeof(buf), parserfile)) {
		/* is this a line to add a command? */
		if ((cp = strchr(buf, '\n')))
			*cp = (char)0;
		cp = skipspace(buf);
		if (strncmp(cp, "command", 7))
			continue; /* no */

		/* yes, we are adding a command, so skip any
		 * separating whitespace
		 */
		cp = skipspace(cp += 7); /* skip "command" */

		/* copy the key that will be used for command text */
		memset(key, 0, sizeof(key));
		for (tp = &key[0];
		    *cp && *cp != ' ' && tp - &key[0] < sizeof(key);
		    *tp++ = *cp++) /* EMPTY */
			;

		/* is there a corresponding command name supplied?
		 * AND was the key short enough?
		 */
		if ((key[PARSE_KEY_MAX - 1] == 0) && *cp &&
		    *(cp = skipspace(cp))) {
			memset(command, 0, sizeof(command));
			for (tp = &command[0]; *cp && *cp != ' ' &&
			    tp - &command[0] < sizeof(command);
			    *tp++ = *cp++) /* EMPTY */
				;
		} else {
			snprintf(sendbuf, sizeof(sendbuf),
			    ">> invalid parser command %s\r\n", buf);
			sendtoplayer(pplayer, sendbuf);
			parser_collapse_dyncontext(newctxp);
			fclose(parserfile);
			return PARSERR_SUPPRESS;
		}

		/* is the command name known? */
		for (commp = newctxp->commands; commp->func; commp++) {
			if (!strncmp(commp->name, command, PARSE_KEY_MAX))
				break; /* found it */
		}
		if (!commp->func) { /* invalid command name */
			snprintf(sendbuf, sizeof(sendbuf),
			    ">> unknown command %s\r\n", command);
			sendtoplayer(pplayer, sendbuf);
			parser_collapse_dyncontext(newctxp);
			fclose(parserfile);
			return PARSERR_SUPPRESS;
		}

		keyp = malloc(sizeof(struct parse_key));
		if (!keyp) {
			snprintf(sendbuf, sizeof(sendbuf),
			    ">> can't alloc new parse_key for %s,%s \r\n", key,
			    command);
			sendtoplayer(pplayer, sendbuf);
			parser_collapse_dyncontext(newctxp);
			fclose(parserfile);
			return PARSERR_SUPPRESS;
		}

		strcpy(keyp->token, key);
		keyp->cmd = commp->cmd;
		if (!parser_add_to_context(newctxp, keyp)) {
			snprintf(sendbuf, sizeof(sendbuf),
			    ">> can't add new context entry for parse_key %s,%s \r\n",
			    key, command);
			sendtoplayer(pplayer, sendbuf);
			free(keyp);
			parser_collapse_dyncontext(newctxp);
			fclose(parserfile);
			return PARSERR_SUPPRESS;
		}
		if (feof(parserfile))
			break;
	}

	fclose(parserfile);

	/* success, we have a context
	 * therefore, collapse the old dynamic table if any
	 */
	if (newctxp->numentries) {
		if (main_parser_context) {
			if (main_parser_context->isdynamic)
				parser_collapse_dyncontext(main_parser_context);
		}
		main_parser_context = newctxp;

		snprintf(sendbuf + strlen(sendbuf),
		    sizeof(sendbuf) - strlen(sendbuf),
		    "installed %lu parse keys.\r\n", newctxp->numentries);
	} else
		snprintf(sendbuf + strlen(sendbuf),
		    sizeof(sendbuf) - strlen(sendbuf),
		    "contains no valid parse keys\n");

	sendtoplayer(pplayer, sendbuf);
	return PARSE_OK;
}

void
handlecommand(struct splayer *pplayer, char *command)
{
	char *buf;
	parse_error rc;

	buf = command;

	/* BUG: this probably doesn't need to be done every time a command is
	 * processed */
	passread();
	/* passopen(); */ /* set up password file. */

	if (!(pplayer->privs & CANPLAY)) {
		prelogon(pplayer, command); /* hasn't set name yet */
	} else {
		if (!default_parser_entries) {
			/* done only once */
			default_parser_entries = parser_count_table_entries(
			    default_parse_table);
			if (!parser_init_context(&default_parser_context,
				default_parse_table, commands, 0)) {
				log_msg("can't allocate parse context\n");
				die(ENOMEM);
			}
			main_parser_context = &default_parser_context;
		}

		/* a hack to permit either . or / to start commands
		 *
		 * this is useful for havens, which traditionally have accepted
		 * either /command or .command
		 *
		 * the alternative is to put entries in the default command
		 * table and/or any local lorien.commands format file to cover
		 * either form.
		 *
		 * if you don't need the hack, and it is interfering, you should
		 * comment out this line, but you'd probably also need to change
		 * prelogon()
		 *
		 */
		if (*buf == '.')
			*buf = '/';

		if ((rc = parser_execute(pplayer, buf, main_parser_context)) !=
		    PARSE_OK) {
			switch (rc) {
			case PARSE_OK: /* should never get here */
			case PARSERR_SUPPRESS:
				break; /* command handler processed any errors
					  already */
			case PARSERR_NOTFOUND:
				sendtoplayer(pplayer, bad_comm_prompt);
				break;
			case PARSERR_AMBIGUOUS:
				sendtoplayer(pplayer, ambiguous_comm_prompt);
				break;
			case PARSERR_NUMARGS:
				sendtoplayer(pplayer, PARSE_ARGS);
				break;
			case PARSERR_NOCLASS:
				sendtoplayer(pplayer, PARSE_CLASS);
				break;
			default:
				sendtoplayer(pplayer, bad_comm_prompt);
			}
		}
	}
}

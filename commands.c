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
#include "board.h"
#include "commands.h"
#include "db.h"
#include "files.h"
#include "help.h"
#include "log.h"
#include "lorien.h"
#include "newplayer.h"
#include "parse.h"
#include "platform.h"
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
	{ "/P", CMD_PASSWORD },
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
	{ "/bbadd", CMD_BOARDADD },
	{ "/bbdel", CMD_BOARDDEL },
	{ "/bblist", CMD_BOARDLIST },
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
		if (findchannel(buf)) {
			snprintf(sendbuf, sendbufsz,
			    ">> channel %s already exists.\r\n", buf);
			sendtoplayer(pplayer, sendbuf);
			return PARSERR_SUPPRESS;
		}
		strncpy(channels->name, buf, MAX_CHAN);
		channels->name[MAX_CHAN - 1] = 0;
		snprintf(sendbuf, sendbufsz, ">> main channel now \"%s\"\r\n",
		    channels->name);
		sendall(sendbuf, ARRIVAL, pplayer);
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
	int rc;

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

	snprintf(sendbuf, sendbufsz, ">> Modifying %s for %s to %s\r\n",
	    element, name, value);
	sendtoplayer(pplayer, sendbuf);

	rc = ldb_player_get(&lorien_db, name, strlen(name), &prefs);

	if (rc == MDB_NOTFOUND) {
		snprintf(sendbuf, sendbufsz,
		    ">> Player %s is not registered.\r\n", name);
		sendtoplayer(pplayer, sendbuf);
		return PARSERR_SUPPRESS;
	} else if (rc != 0) {
		snprintf(sendbuf, sendbufsz,
		    ">> Error %d: Player database unreadable.\r\n", rc);
		sendtoplayer(pplayer, sendbuf);
		return PARSERR_SUPPRESS;
	}

	/* is target player logged on? */
	for (tplayer = players->next; tplayer; tplayer = tplayer->next)
		if (!strcmp(tplayer->name, name) && PLAYER_HAS(VRFY, pplayer))
			break;

	int curlev = tplayer ? tplayer->seclevel : prefs.seclevel;
	if (pplayer->seclevel <= curlev) {
		snprintf(sendbuf, sendbufsz,
		    ">> %s has at least as much authority as you.\r\n", name);
		sendtoplayer(pplayer, sendbuf);
		return PARSERR_SUPPRESS;
	}

	int newlev = -1;

	switch (*element) {
	case 's': /* seclevel */
		newlev = atoi(value);
		if (pplayer->seclevel < newlev) {
			snprintf(sendbuf, sendbufsz,
			    ">> Your level %d < new level %d.\r\n",
			    pplayer->seclevel, newlev);
			sendtoplayer(pplayer, sendbuf);
			return PARSERR_SUPPRESS;
		}
		if ((newlev < 0) || (newlev >= NUMLVL)) {
			snprintf(sendbuf, sendbufsz,
			    ">> %d is not a valid security level..\n", newlev);
			sendtoplayer(pplayer, sendbuf);
			return PARSERR_SUPPRESS;
		}
		prefs.seclevel = newlev;
		break;
	case 'p': /* password */;
		rc = mkpasswd(prefs.password, sizeof(prefs.password), value);
		if (rc != 0) {
			sendtoplayer(pplayer, ">> Can't make password\r\n");
			return PARSERR_SUPPRESS;
		}
	default:
		return PARSERR_AMBIGUOUS;
	}

	rc = ldb_player_put(&lorien_db, &prefs, false);

	if (rc != 0) {
		snprintf(sendbuf, sendbufsz,
		    ">> Error %d: Can't write record for %s\n", rc, name);
		sendtoplayer(pplayer, sendbuf);
		return PARSERR_SUPPRESS;
	}

	/* if logged in, make in-core match */
	if (tplayer) {
		switch (*element) {
		case 'p':
			strlcpy(tplayer->password, prefs.password,
			    sizeof(tplayer->password));
			break;
		case 's':
			tplayer->seclevel = prefs.seclevel;
			break;
		default:
			return PARSERR_AMBIGUOUS;
		}
	}

	snprintf(sendbuf, sendbufsz, ">> %s for %s set to %s\r\n", element,
	    name, value);
	sendtoplayer(pplayer, sendbuf);
	return PARSE_OK;
}

parse_error
messages(struct splayer *pplayer)
{
	PLAYER_XOR(MSG, pplayer);
	if (PLAYER_HAS(MSG, pplayer))
		snprintf(sendbuf, sendbufsz, MESSAGE_MSG);
	else
		snprintf(sendbuf, sendbufsz, NOMESSAGE_MSG);
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
	snprintf(sendbuf, sendbufsz, ">> fd limit now %lu.\r\n", MAXCONN);
	rc = PARSE_OK;

	sendtoplayer(pplayer, sendbuf);
	return rc;
}

/* password code starts here. */
parse_error
enablePassword(struct splayer *pplayer, char *buf)
{
	struct splayer *tplayer;
	struct splayer newplayer = { 0 };
	int rc;

	char *pword = strchr(buf, '=');
	if (pword == (char *)0) {
		snprintf(sendbuf, sendbufsz, IVCMD_SYN);
		sendtoplayer(pplayer, sendbuf);
		return PARSERR_SUPPRESS;
	}
	*pword++ = (char)0;

	/* is target player logged on? */
	for (tplayer = players->next; tplayer; tplayer = tplayer->next)
		if (!strcmp(tplayer->name, buf))
			break;

	if (!tplayer) {
		tplayer = &newplayer;
		/* BUG: we don't know where the new player will log on from
		 * but 0.0.0.0 is impossible. We could change the player login
		 * code to detect this and update the record with the info from
		 * the first host they log in from
		 */
		playerinit(tplayer, time((time_t *)0), "0.0.0.0", "0.0.0.0");
		strlcpy(tplayer->name, buf, sizeof(tplayer->name));
	} else if (PLAYER_HAS(VRFY, tplayer)) {
		sendtoplayer(pplayer, ">> Player is already verified.\r\n");
		return PARSERR_SUPPRESS;
	}

	rc = mkpasswd(tplayer->password, sizeof(tplayer->password), pword);
	if (rc != 0) {
		snprintf(sendbuf, sendbufsz, ">> Cannot hash new password\r\n");
		sendtoplayer(pplayer, sendbuf);
		return PARSERR_SUPPRESS;
	}

	rc = ldb_player_put(&lorien_db, tplayer, true); /* no overwrite */
	if (rc != 0) {
		snprintf(sendbuf, sendbufsz,
		    ">> Error %d: cannot create new player\r\n", rc);
		sendtoplayer(pplayer, sendbuf);
		return PARSERR_SUPPRESS;
	}

	PLAYER_SET(VRFY, tplayer);

	sendtoplayer(pplayer, ">> Password enabled for player.\r\n");

	return PARSE_OK;
}

parse_error
add_board(struct splayer *pplayer, char *buf)
{
	char *n = trimspace(buf, BUFSIZE); /* board name */
	char *d = index(buf, '|');	   /* default description */
	int rc = 0;

	if (d) {
		*d++ = (char)0;
		n = trimspace(n, BUFSIZE - (n - buf));
		d = skipspace(d);
	}

	rc = board_add(n, pplayer->name, d, LDB_BOARD_BULLETIN,
	    time((time_t *)NULL), true);
	if (!rc)
		snprintf(sendbuf, sendbufsz,
		    ">> added board |%s| desc |%s|\r\n", n, d);
	else
		snprintf(sendbuf, sendbufsz, ">> couldn't add board %s\r\n", n);

	sendtoplayer(pplayer, sendbuf);

	return (!rc) ? PARSE_OK : PARSERR_SUPPRESS;
}
parse_error
delete_board(struct splayer *pplayer, char *buf)
{
	char *n = trimspace(buf, BUFSIZE); /* board name */
	int rc = board_remove(n);

	switch (rc) {
	case 0:
		snprintf(sendbuf, sendbufsz, ">> removed board |%s|\r\n", n);
		break;
	case BOARDERR_NOTEMPTY:
		snprintf(sendbuf, sendbufsz, ">> board |%s| is not empty\r\n",
		    n);
		break;
	case BOARDERR_NOTFOUND:
		snprintf(sendbuf, sendbufsz, ">> board |%s| not found\r\n", n);
		break;
	default:
		snprintf(sendbuf, sendbufsz,
		    ">> board |%s| unknown error %d\r\n", n, rc);
	}

	sendtoplayer(pplayer, sendbuf);

	return (rc == 0) ? PARSE_OK : PARSERR_SUPPRESS;
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
		snprintf(sendbuf, sendbufsz, NO_PERM);
		sendtoplayer(pplayer, sendbuf);
		return PARSERR_SUPPRESS;
	}

	return PARSERR_NOTFOUND;
}

/* saves player record (preferences) and optionally updates password */
parse_error
changePlayer(struct splayer *pplayer, char *buf)
{
	char *newpass = strchr(buf, '=');
	parse_error rc = PARSERR_SUPPRESS;
	struct splayer cplayer;

	if (!PLAYER_HAS(VRFY, pplayer)) {
		/* can't save update for an unregistered player */
		snprintf(sendbuf, sendbufsz, bad_comm_prompt);
		goto out;
	}

	if (newpass != NULL)
		*newpass++ = (char)0;

	buf = skipspace(buf);

	rc = ckpasswd(pplayer->password, buf);

	if (rc != 0) {
		rc = PARSERR_SUPPRESS;
		strlcpy(sendbuf,
		    ">> Usage: /Poldpass[=newpass], newpass optional\r\n",
		    sendbufsz);
		goto out;
	}

	memcpy(&cplayer, pplayer, sizeof(cplayer));
	/* If new pass specified, update hash, otherwise just save player */
	if (newpass) {
		rc = mkpasswd(cplayer.password, sizeof(cplayer.password),
		    newpass);
		if (rc != 0) {
			strlcpy(sendbuf, ">> Can't hash new password\r\n",
			    sendbufsz);
			goto out;
		}
	}

	rc = ldb_player_put(&lorien_db, &cplayer, false); /* overwrite */
	if (rc != 0) {
		snprintf(sendbuf, sendbufsz,
		    (newpass) ? ">> Error %d, cannot update player db" :
				">> Error %d, password NOT changed",
		    rc);
		rc = PARSERR_SUPPRESS;
		goto out;
	}

	if (newpass)
		strlcpy(pplayer->password, cplayer.password,
		    sizeof(pplayer->password));

	snprintf(sendbuf, sendbufsz, ">> Player record updated.\r\n");

out:
	sendtoplayer(pplayer, sendbuf);
	return rc;
}

parse_error
delete_player(struct splayer *pplayer, char *buf)
{
	struct splayer *tplayer;
	struct splayer delplayer = { 0 };
	int rc;

	/* is target player logged on? */
	for (tplayer = players->next; tplayer; tplayer = tplayer->next)
		if (!strcmp(tplayer->name, buf) && PLAYER_HAS(VRFY, pplayer))
			break;

	if (!tplayer) {
		tplayer = &delplayer;
		strncpy(tplayer->name, buf, sizeof(tplayer->name) - 1);
		tplayer->name[sizeof(tplayer->name) - 1] = (char)0;
	}

	rc = ldb_player_delete(&lorien_db, tplayer);
	if (rc != 0) {
		snprintf(sendbuf, sendbufsz,
		    ">> Error %d, can't delete player.\r\n", rc);
		rc = PARSERR_SUPPRESS;
		goto out;
	}

	PLAYER_CLR(VRFY, tplayer);
	snprintf(sendbuf, sendbufsz, ">> Player deleted from database.\r\n");
	rc = PARSE_OK;

out:
	sendtoplayer(pplayer, sendbuf);
	return rc;
}

parse_error
set_name(struct splayer *pplayer, char *buf)
{
	char *buf2;
	struct splayer rplayer = { 0 };
	int rc;
	bool newconn = !(pplayer->privs & CANPLAY);

	if (!(pplayer->privs & CANNAME)) {
		sendtoplayer(pplayer, NO_PERM);
		return PARSERR_SUPPRESS;
	}
	buf = (char *)skipspace(buf);
	buf2 = strchr(buf, '=');

	if (buf2 != (char *)0)
		*buf2++ = (char)0;

	rc = ldb_player_get(&lorien_db, buf,
	    strnlen(buf, sizeof(pplayer->name) - 1), &rplayer);

	/* player not in db but password supplied */
	if ((rc != 0) && buf2) {
		snprintf(sendbuf, sendbufsz,
		    ">> Name not found in database.\r\n");
		sendtoplayer(pplayer, sendbuf);
		return PARSERR_SUPPRESS;
	}

	/* player in db but no password supplied */
	if ((rc == 0) && !buf2) {
		snprintf(sendbuf, sendbufsz,
		    ">> That name is reserved.  Please use another.\r\n");
		sendtoplayer(pplayer, sendbuf);
		return PARSERR_SUPPRESS;
	}

	/* player in db and password supplied */
	if ((rc == 0) && buf2) {
		rc = ckpasswd(rplayer.password, buf2);

		if (rc != 0) {
			snprintf(sendbuf, sendbufsz,
			    ">> Invalid password.\r\n");
			sendtoplayer(pplayer, sendbuf);
			return PARSERR_SUPPRESS;
		}

		snprintf(sendbuf, sendbufsz, ">> Last login %s ago from %s\r\n",
		    idlet(rplayer.cameon), rplayer.host);
		sendtoplayer(pplayer, sendbuf);

		/* update last login info in db */
		strlcpy(rplayer.host, pplayer->host, sizeof(rplayer.host));
		rplayer.cameon = time((time_t *)0);

		rc = ldb_player_put(&lorien_db, &rplayer, false);
		if (rc != 0) {
			snprintf(sendbuf, sendbufsz,
			    ">> Error %d. Cannot update login time\r\n", rc);
			sendtoplayer(pplayer, sendbuf);
		}

		strlcpy(pplayer->name, rplayer.name, sizeof(pplayer->name));
		strlcpy(pplayer->password, rplayer.password,
		    sizeof(pplayer->password));
		pplayer->seclevel = rplayer.seclevel;
		pplayer->hilite = rplayer.hilite;
		pplayer->privs =
		    rplayer.privs; /* do we need to persist this? */
		pplayer->wrap = rplayer.wrap;
		pplayer->flags = rplayer.flags;
		pplayer->pagelen = rplayer.pagelen; /* BUG: not implemented */
		pplayer->playerwhen = rplayer.playerwhen;
		pplayer->cameon = rplayer.cameon;
		PLAYER_SET(VRFY, pplayer);
	} else { /* player not in db and no password supplied */
		setname(pplayer, buf);
		PLAYER_CLR(VRFY, pplayer); /* new name is unverified */
	}

	/* set this flag whenever anyone successfully sets their name */
	pplayer->privs |= CANPLAY;

	if (newconn) {
		/* let everyone else know who's here. */
		snprintf(sendbuf, sendbufsz,
		    ">> New arrival on line %d from %s.\r\n",
		    player_getline(pplayer), pplayer->host);
		sendall(sendbuf, ARRIVAL, 0);
		pplayer->chnl = channels;
		channels->count++;
	}

	return PARSE_OK;
}

/* password code ends here. */

void
pose_it(struct splayer *pplayer, char *buf)
{
	int line = player_getline(pplayer);

	if (*buf == ',' || *buf == '\'')
		snprintf(sendbuf, sendbufsz, "(%d) %s%s\r\n", line,
		    pplayer->name, buf);
	else
		snprintf(sendbuf, sendbufsz, "(%d) %s %s\r\n", line,
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
	int sender = player_getline(pplayer);
	struct splayer *who, *next;

	if (isdigit(buf[0])) {
		linenum = atoi(buf);
		buf = (char *)skipdigits(buf);

		who = lookup(linenum);
		if (who == (struct splayer *)0) {
			snprintf(sendbuf, sendbufsz,
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
				snprintf(sendbuf, sendbufsz, fmtstring, sender,
				    who->name, pplayer->name, buf);
				if (pplayer->seclevel > JOEUSER)
					sendall(sendbuf, pplayer->chnl, 0);
				else
					sendtoplayer(pplayer, sendbuf);
			} else {
				buf = (char *)skipspace(buf);
				fmtstring = (mode == SPEECH_YELL) ?
				    fmt_yellstage :
				    fmt_stage;
				snprintf(sendbuf, sendbufsz, fmtstring, sender,
				    pplayer->name, who->name, buf);
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
		snprintf(sendbuf, sendbufsz, bad_comm_prompt);
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
			snprintf(sendbuf, sendbufsz,
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
				snprintf(sendbuf, sendbufsz,
				    ">> %s has at least as much authority as you.\r\n",
				    who->name);
				sendtoplayer(pplayer, sendbuf);
				snprintf(sendbuf, sendbufsz,
				    ">> %s just tried to force you...\r\n",
				    pplayer->name);
				sendtoplayer(who, sendbuf);
			}
		}
	} else {
		snprintf(sendbuf, sendbufsz, bad_comm_prompt);
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
		snprintf(sendbuf, sendbufsz,
		    ">> error:  Player %d does not exist!\r\n", line_number);
	} else if ((line_number >= 1) && (line_number < MAXCONN)) {
		rc = PARSE_OK;
		if (FD_ISSET(line_number, &pplayer->gags)) {
			FD_CLR(line_number, &pplayer->gags);
			snprintf(sendbuf, sendbufsz,
			    ">> Player %d ungagged.\r\n", line_number);
		} else {
			FD_SET(line_number, &pplayer->gags);
			snprintf(sendbuf, sendbufsz, ">> Player %d gagged.\r\n",
			    line_number);
		}
	} else {
		snprintf(sendbuf, sendbufsz, bad_comm_prompt);
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
		snprintf(sendbuf, sendbufsz,
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
		snprintf(sendbuf, sendbufsz,
		    ">> error:  Player %d does not exist!\r\n", line);
		sendtoplayer(pplayer, sendbuf);
		return PARSERR_SUPPRESS;
	}

	snprintf(sendbuf, sendbufsz, ">> Name: %s\r\n", who->name);
	sendtoplayer(pplayer, sendbuf);
	snprintf(sendbuf, sendbufsz, ">> Channel: %s %s\r\n",
	    who->chnl ? (who->chnl->name) : "***none***",
	    who->chnl ? ((who->chnl->secure) ? "(Secured)" : "") : "");
	sendtoplayer(pplayer, sendbuf);
#ifdef ONFROM_ANY
	snprintf(sendbuf, sendbufsz, ">> Doing: %s\r\n", who->onfrom);
	sendtoplayer(pplayer, sendbuf);
#endif

	snprintf(sendbuf, sendbufsz, ">> On From: %s (%s:%d)\r\n", who->host,
	    who->numhost, who->port);
	sendtoplayer(pplayer, sendbuf);

	if ((pplayer->seclevel > BABYCO) || (pplayer == who)) {
		snprintf(sendbuf, sendbufsz, ">> Gags: ");
		for (currfd = 1, gagcount = 0; currfd <= MAXCONN; currfd++) {
			if (FD_ISSET(currfd, &who->gags)) {
				gagcount++;
				snprintf(maskbuf, sizeof(maskbuf), "%d ",
				    currfd);
				strncat(sendbuf, maskbuf,
				    (sendbufsz - 1) - strlen(sendbuf));
				if (gagcount && !(gagcount % 8)) {
					strcat(maskbuf, "\r\n");
					sendtoplayer(pplayer, sendbuf);
					snprintf(sendbuf, sendbufsz,
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

	snprintf(sendbuf, sendbufsz, ">> Toggles enabled: %s\r\n",
	    mask2string32(who->flags, PLAYER_MAX_FLAG_BIT, maskbuf,
		sizeof(maskbuf), player_flags_names, ", "));
	sendtoplayer(pplayer, sendbuf);
	snprintf(sendbuf, sendbufsz, ">> Toggles disabled: %s\r\n",
	    mask2string32(~(who->flags), PLAYER_MAX_FLAG_BIT, maskbuf,
		sizeof(maskbuf), player_flags_names, ", "));
	sendtoplayer(pplayer, sendbuf);
	snprintf(sendbuf, sendbufsz, ">> Hilites: %s\r\n",
	    (who->hilite) ? mask2string(who->hilite, maskbuf, hi_types, ", ") :
			    "None");
	sendtoplayer(pplayer, sendbuf);

	snprintf(sendbuf, sendbufsz, ">> Idle: %s\tWrap width: %d\r\n",
	    idlet(who->idle), who->wrap);
	sendtoplayer(pplayer, sendbuf);

	snprintf(sendbuf, sendbufsz, ">> Privileges granted: %s\r\n",
	    mask2string32(who->privs, CAN_MAX_FLAG_BIT, maskbuf,
		sizeof(maskbuf), player_privs_names, ", "));
	sendtoplayer(pplayer, sendbuf);
	snprintf(sendbuf, sendbufsz, ">> Privileges revoked: %s\r\n",
	    mask2string32(~(who->privs), CAN_MAX_FLAG_BIT, maskbuf,
		sizeof(maskbuf), player_privs_names, ", "));
	sendtoplayer(pplayer, sendbuf);

	snprintf(sendbuf, sendbufsz, ">> End of info\r\n");
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
	int sender = player_getline(pplayer);

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
			snprintf(sendbuf, sendbufsz,
			    ">> error:  Player %d does not exist!\r\n",
			    linenumber);
			sendtoplayer(pplayer, sendbuf);
			return PARSERR_SUPPRESS;
		}

		if (who->seclevel >= pplayer->seclevel) {
			snprintf(sendbuf, sendbufsz,
			    ">> %s has at least as much authority as you!\r\n",
			    who->name);
			sendtoplayer(pplayer, sendbuf);
			snprintf(sendbuf, sendbufsz,
			    ">> %s just tried to change your channel.\r\n",
			    pplayer->name);
			sendtoplayer(who, sendbuf);
			return PARSERR_SUPPRESS;
		}

		strcpy(who->pbuf, "/c");
		strcat(who->pbuf, buf);
		processinput(who);
		snprintf(sendbuf, sendbufsz,
		    ">> (%d) %s placed on channel %s.\r\n", player_getline(who),
		    who->name, who->chnl->name);
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

			snprintf(sendbuf, sendbufsz,
			    ">> %-13s %-10s %-6s %-6s\r\n", "Channel",
			    "# Users", "Secure", "Persists");
			sendtoplayer(pplayer, sendbuf);
			sendtoplayer(pplayer,
			    ">> -------------------------------\r\n");

			while (newc) {
				snprintf(sendbuf, sendbufsz,
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

		snprintf(sendbuf, sendbufsz, ">> (%d) %s has joined.\r\n",
		    sender, pplayer->name);
		sendall(sendbuf, newc, 0);

		pplayer->chnl = newc;

		snprintf(sendbuf, sendbufsz, ">> Channel changed.\r\n");
		sendtoplayer(pplayer, sendbuf);

		snprintf(sendbuf, sendbufsz, ">> (%d) %s has wandered off.\r\n",
		    sender, pplayer->name);
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
	static char formatposespace[] = "%s(%d,p) %s %s%s%s";
	static char formatposenospace[] = "%s(%d,p) %s%s%s%s";
	static char formatwhisper[] = "%s(%d,p %s) %s%s%s";
	static char formatposeecho[] = ">> Pose sent to %s : %s %s\r\n";
	static char formatecho[] = ">> /p sent to %s : %s\r\n";
	char *fmtstring;
	int linenum;
	int sender;
	struct splayer *who = NULL;

	if (!(pplayer->privs & CANWHISPER)) {
		sendtoplayer(pplayer, NO_PERM);
		return PARSERR_SUPPRESS;
	}

	if (isdigit(*buf)) {
		linenum = atoi(buf);
		who = lookup(linenum);
		if (!who) {
			snprintf(sendbuf, sendbufsz,
			    ">> error:  Player %d does not exist.\r\n",
			    linenum);
			sendtoplayer(pplayer, sendbuf);
			return PARSERR_SUPPRESS;
		}
	}
	sender = player_getline(pplayer);

	if (who) {
		pplayer->dotspeeddial = who;
		buf = (char *)skipdigits(buf);
	} else {
		who = pplayer->dotspeeddial;
		if (!who) {
			snprintf(sendbuf, sendbufsz, bad_comm_prompt);
			sendtoplayer(pplayer, sendbuf);
		}
		linenum = player_getline(who);
		if (isspace(*buf))
			buf++;
	}

	buf = (char *)skipspace(buf);
	if (*buf == ':' || *buf == ';') {
		buf++;
		buf = (char *)skipspace(buf);
		if (*buf == ',' || *buf == '\'') {
			buf = (char *)skipspace(buf + 1);
			fmtstring = formatposenospace;
		} else
			fmtstring = formatposespace;
	} else
		fmtstring = formatwhisper;

	snprintf(sendbuf, sendbufsz, fmtstring,
	    (who->hilite) ? expand_hilite(who->hilite, hbuf) : "", sender,
	    pplayer->name, buf, (who->hilite) ? expand_hilite(0L, hbuf2) : "",
	    PLAYER_HAS(BEEPS, who) ? BEEPS_NEWLINE : NOBEEPS_NEWLINE);
	if (fmtstring == formatwhisper)
		snprintf(buf2, sizeof(buf2), formatecho, who->name, buf);
	else
		snprintf(buf2, sizeof(buf2), formatposeecho, who->name,
		    pplayer->name, buf);

	if (pplayer->seclevel > JOEUSER && !isgagged(who, pplayer))
		sendtoplayer(who, sendbuf);

	if (!PLAYER_HAS(ECHO, pplayer)) {
		sendtoplayer(pplayer, ">> /p sent.\r\n");
	} else {
		sendtoplayer(pplayer, buf2);
	}

	return PARSE_OK;
}

parse_error
change_privs(struct splayer *pplayer, char *buf)
{
	int line;
	int sender = player_getline(pplayer);
	struct splayer *who;

	buf = (char *)skipspace(buf);

	if (!isdigit(*buf)) {
		snprintf(sendbuf, sendbufsz,
		    ">> Bad player number in command.\r\n");
		sendtoplayer(pplayer, sendbuf);
		return PARSERR_SUPPRESS;
	}

	line = atoi(buf);

	who = lookup(line);

	if (!who) {
		snprintf(sendbuf, sendbufsz,
		    ">> error:  Player %d does not exist!\r\n", line);
		sendtoplayer(pplayer, sendbuf);
		return PARSERR_SUPPRESS;
	}

	if (who->seclevel >= pplayer->seclevel) {
		snprintf(sendbuf, sendbufsz,
		    ">> %s has at least as much authority as you.\r\n",
		    who->name);
		sendtoplayer(pplayer, sendbuf);
		snprintf(sendbuf, sendbufsz,
		    ">> (%d) %s just tried to change your privileges.\r\n",
		    sender, pplayer->name);
		sendtoplayer(who, sendbuf);
		return PARSERR_SUPPRESS;
	}

	buf = (char *)skipspace(skipdigits(buf));

	while (*buf) {
		*buf = toupper(*buf);
		switch (*buf) {
		case 'Y':
			if (who->privs & CANYELL) {
				snprintf(sendbuf, sendbufsz,
				    ">> Player %d is not allowed to '\''yell.\r\n",
				    line);
				sendtoplayer(pplayer, sendbuf);
				who->privs &= ~CANYELL;
			} else {
				snprintf(sendbuf, sendbufsz,
				    ">> Player %d may now yell.\r\n", line);
				sendtoplayer(pplayer, sendbuf);
				who->privs |= CANYELL;
			};
			break;
		case 'W':
			if (who->privs & CANWHISPER) {
				snprintf(sendbuf, sendbufsz,
				    ">> Player %d is not allowed to whisper.\r\n",
				    line);
				sendtoplayer(pplayer, sendbuf);
				who->privs &= ~CANWHISPER;
			} else {
				snprintf(sendbuf, sendbufsz,
				    ">> Player %d may now whisper.\r\n", line);
				sendtoplayer(pplayer, sendbuf);
				who->privs |= CANWHISPER;
			};
			break;
		case 'N':
			if (who->privs & CANNAME) {
				snprintf(sendbuf, sendbufsz,
				    ">> Player %d is not allowed to set own name.\r\n",
				    line);
				sendtoplayer(pplayer, sendbuf);
				who->privs &= ~CANNAME;
			} else {
				snprintf(sendbuf, sendbufsz,
				    ">> Player %d may now set own name.\r\n",
				    line);
				sendtoplayer(pplayer, sendbuf);
				who->privs |= CANNAME;
			};
			break;
		case 'Q':
			if (who->privs & CANQUIT) {
				snprintf(sendbuf, sendbufsz,
				    ">> Player %d is not allowed to quit.\r\n",
				    line);
				sendtoplayer(pplayer, sendbuf);
				who->privs &= ~CANQUIT;
			} else {
				snprintf(sendbuf, sendbufsz,
				    ">> Player %d may now quit.\r\n", line);
				sendtoplayer(pplayer, sendbuf);
				who->privs |= CANQUIT;
			};
			break;
		case 'T':
			if (who->privs & CANCHANNEL) {
				snprintf(sendbuf, sendbufsz,
				    ">> Player %d is not allowed to change channels\r\n",
				    line);
				sendtoplayer(pplayer, sendbuf);
				who->privs &= ~CANCHANNEL;
			} else {
				snprintf(sendbuf, sendbufsz,
				    ">> Player %d may now change channels.\r\n",
				    line);
				sendtoplayer(pplayer, sendbuf);
				who->privs |= CANCHANNEL;
			};
			break;
		case 'C':
			if (who->privs & CANCAPS) {
				snprintf(sendbuf, sendbufsz,
				    ">> Player %d is not allowed to use capital letters.\r\n",
				    line);
				sendtoplayer(pplayer, sendbuf);
				who->privs &= ~CANCAPS;
			} else {
				snprintf(sendbuf, sendbufsz,
				    ">> Player %d may now use capital letters.\r\n",
				    line);
				sendtoplayer(pplayer, sendbuf);
				who->privs |= CANCAPS;
			};
			break;
		case 'B':
			if (who->privs & CANBOARD) {
				snprintf(sendbuf, sendbufsz,
				    ">> Player %d is not allowed to post bulletins.\r\n",
				    line);
				sendtoplayer(pplayer, sendbuf);
				who->privs &= ~CANBOARD;
			} else {
				snprintf(sendbuf, sendbufsz,
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
	int sender = player_getline(pplayer);

	if (!(pplayer->privs & CANYELL)) {
		sendtoplayer(pplayer, NO_PERM);
		return PARSERR_SUPPRESS;
	}

	if (PLAYER_HAS(HUSH, pplayer)) {
		snprintf(sendbuf, sendbufsz, YELL_MSG);
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

			snprintf(sendbuf, sendbufsz, fmtstring, sender,
			    pplayer->name, buf);
		} else {
			snprintf(sendbuf, sendbufsz, "(*%d, %s*) %s\r\n",
			    sender, pplayer->name, buf);
		}
		if ((pplayer->seclevel) > JOEUSER) {
			who = players->next;
			while (who) {
				next = who->next;
				if (!PLAYER_HAS(HUSH, who) &&
				    !isgagged(who, pplayer))
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
		snprintf(sendbuf, sendbufsz, ECHO_MSG);
	} else
		snprintf(sendbuf, sendbufsz, NOECHO_MSG);
	sendtoplayer(pplayer, sendbuf);
	return PARSE_OK;
}

parse_error
hush(struct splayer *pplayer)
{
	/* if unhushed and screaming, can't scream no more. */
	if (PLAYER_HAS(SCREAM, pplayer) && !PLAYER_HAS(HUSH, pplayer)) {
		snprintf(sendbuf, sendbufsz, NOSCREAM_MSG);
		sendtoplayer(pplayer, sendbuf);
		PLAYER_XOR(SCREAM, pplayer);
	}

	PLAYER_XOR(HUSH, pplayer);
	if (PLAYER_HAS(HUSH, pplayer))
		snprintf(sendbuf, sendbufsz, HUSH_MSG);
	else
		snprintf(sendbuf, sendbufsz, UNHUSH_MSG);
	sendtoplayer(pplayer, sendbuf);
	return PARSE_OK;
}

parse_error
scream(struct splayer *pplayer)
{
	if (PLAYER_HAS(HUSH, pplayer)) {
		snprintf(sendbuf, sendbufsz, YELL_MSG);
		sendtoplayer(pplayer, sendbuf);
	} else {
		PLAYER_XOR(SCREAM, pplayer);
		if (PLAYER_HAS(SCREAM, pplayer))
			snprintf(sendbuf, sendbufsz, SCREAM_MSG);
		else
			snprintf(sendbuf, sendbufsz, NOSCREAM_MSG);
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
		snprintf(sendbuf, sendbufsz,
		    ">> error:  Player %d does not exist.\r\n", linenum);
		sendtoplayer(pplayer, sendbuf);
	} else {
		if ((who->seclevel + 1) >= pplayer->seclevel) {
			snprintf(sendbuf, sendbufsz,
			    ">> %s's security level is too high.\r\n",
			    who->name);
			sendtoplayer(pplayer, sendbuf);
		} else {
			who->seclevel++;
			snprintf(sendbuf, sendbufsz, "%s(%d) promoted %s(%d)",
			    pplayer->name, pplayer->seclevel, who->name,
			    who->seclevel);
			logmsg(sendbuf);
			snprintf(sendbuf, sendbufsz, ">> %s promoted.\r\n",
			    who->name);
			sendtoplayer(pplayer, sendbuf);
			snprintf(sendbuf, sendbufsz,
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
		snprintf(sendbuf, sendbufsz,
		    ">> error:  Player %d does not exist.\r\n", linenum);
		sendtoplayer(pplayer, sendbuf);
	} else {
		if (who->seclevel < pplayer->seclevel) {
			who->seclevel--;
			snprintf(sendbuf, sendbufsz, ">> %s demoted.\r\n",
			    who->name);
			sendtoplayer(pplayer, sendbuf);
		} else {
			snprintf(sendbuf, sendbufsz,
			    ">> You don't have authority over %s!\r\n",
			    who->name);
			sendtoplayer(pplayer, sendbuf);
			snprintf(sendbuf, sendbufsz,
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
		snprintf(sendbuf, sendbufsz,
		    ">> error:  Player %d does not exist.\r\n", linenum);
		sendtoplayer(pplayer, sendbuf);
	} else {
		if (who->seclevel < pplayer->seclevel) {
			snprintf(sendbuf, sendbufsz, DEAD_MSG);
			sendtoplayer(who, sendbuf);
			snprintf(sendbuf, sendbufsz, ">> %s(%d) killed.\r\n",
			    who->name, linenum);
			sendtoplayer(pplayer, sendbuf);
			snprintf(sendbuf, sendbufsz, "%s killed %s.",
			    pplayer->name, who->name);
			logmsg(sendbuf);
			removeplayer(who);
		} else {
			snprintf(sendbuf, sendbufsz,
			    ">> You don't have authority over %s!\r\n",
			    who->name);
			sendtoplayer(pplayer, sendbuf);
			snprintf(sendbuf, sendbufsz,
			    ">> %s just tried to kill you!\r\n", pplayer->name);
			sendtoplayer(who, sendbuf);
			snprintf(sendbuf, sendbufsz, "%s tried to kill %s.",
			    pplayer->name, who->name);
			logmsg(sendbuf);
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
	int sender = player_getline(pplayer);

	snprintf(sendbuf, sendbufsz,
	    ">> Broadcast message from (%d) %s : %s\r\n", sender, pplayer->name,
	    buf);
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
		snprintf(sendbuf, sendbufsz, "%s\r\n", buf);
		sendall(sendbuf, INFORMATIONAL, 0);

		/* 	    snprintf(sendbuf, sendbufsz, ">> global info
		 * |%s| sent\r\n",buf); */
		/* 	    sendtoplayer(pplayer,sendbuf); */
		/* 	    logmsg(sendbuf); */
	} else {
		who = lookup(line);

		/* 	    snprintf(sendbuf, sendbufsz, "private info
		 * |%s| targeted %d.\r\n", buf,line); */
		/* 	    sendtoplayer(pplayer,sendbuf); */
		/* 	    logmsg(sendbuf); */

		buf = (char *)skipspace(skipdigits(buf));
		snprintf(sendbuf, sendbufsz, "%s\r\n", buf);
		if (who) {
			sendtoplayer(who, sendbuf);
			/* 		snprintf(sendbuf, sendbufsz,">>
			 * sent to %s.\r\n", who->name); */
			/* 		sendtoplayer(pplayer,sendbuf); */
		} else {
			snprintf(sendbuf, sendbufsz,
			    ">> error:  Player %d does not exist.\r\n", line);
			sendtoplayer(pplayer, sendbuf);
			/*		logmsg(sendbuf); */
		}
	}
	return PARSE_OK;
}

parse_error
playerquit(struct splayer *pplayer, char *buf)
{
	if (pplayer->privs & CANQUIT) {
		snprintf(sendbuf, sendbufsz, "%s was on from %s", pplayer->name,
		    pplayer->host);
		logmsg(sendbuf);

		snprintf(sendbuf, sendbufsz, EXIT_MSG);
		sendtoplayer(pplayer, sendbuf);
		PLAYER_SET(LEAVING, pplayer); /* handleinput() will remove */
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
			snprintf(sendbuf, sendbufsz, MINWRAP_MSG, 4);
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
		snprintf(sendbuf, sendbufsz, WRAP_MSG, pplayer->wrap);
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
		snprintf(sendbuf, sendbufsz, ">> /i messages enabled.\r\n");
	else
		snprintf(sendbuf, sendbufsz, ">> /i messages suppressed.\r\n");

	sendtoplayer(pplayer, sendbuf);
	return PARSE_OK;
}

parse_error
level_toggle(struct splayer *pplayer)
{
	PLAYER_XOR(SHOW, pplayer);

	if (!PLAYER_HAS(SHOW, pplayer))
		snprintf(sendbuf, sendbufsz, NO_LEVEL_MSG);
	else
		snprintf(sendbuf, sendbufsz, LEVEL_MSG);
	sendtoplayer(pplayer, sendbuf);
	return PARSE_OK;
}

parse_error
secure_channel(struct splayer *pplayer, char *buf)
{
	int sender = player_getline(pplayer);
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
			snprintf(sendbuf, sendbufsz, UNSECURE_MSG, sender,
			    pplayer->name);
			sendall(sendbuf, pplayer->chnl, 0);
		} else {
			snprintf(sendbuf, sendbufsz, SECURE_MSG, sender,
			    pplayer->name);
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
	if (!ban_add(buf, pplayer->name, time((time_t *)NULL), true)) {
		snprintf(sendbuf, sendbufsz,
		    ">> cannot add banlist entry.\r\n");
		sendtoplayer(pplayer, sendbuf);
		return PARSERR_SUPPRESS;
	}
	snprintf(sendbuf, sendbufsz, ">> %s added to banlist.\r\n", buf);
	sendall(sendbuf, ALL, pplayer);
	return PARSE_OK;
}

parse_error
beeps(struct splayer *pplayer)
{
	PLAYER_XOR(BEEPS, pplayer);
	if (PLAYER_HAS(BEEPS, pplayer))
		snprintf(sendbuf, sendbufsz, BEEPS_MSG);
	else
		snprintf(sendbuf, sendbufsz, NOBEEPS_MSG);
	sendtoplayer(pplayer, sendbuf);
	return PARSE_OK;
}

parse_error
hilites(struct splayer *pplayer, char *buf)
{
	if (construct_mask(buf, &pplayer->hilite) < 0) {
		snprintf(sendbuf, sendbufsz,
		    ">> Error parsing hilite string.\r\n");
		sendtoplayer(pplayer, sendbuf);
	} else {
		char tmpstr[BUFSIZE];
		memset(tmpstr, 0, BUFSIZE);
		snprintf(sendbuf, sendbufsz, HIGHLIGHT_MSG,
		    mask2string(pplayer->hilite, tmpstr, hi_types, ","));
		sendtoplayer(pplayer, sendbuf);
	};
	return PARSE_OK;
}

parse_error
delete_ban(struct splayer *pplayer, char *buf)
{
	buf = (char *)skipspace(buf);
	if (ban_remove(buf)) {
		snprintf(sendbuf, sendbufsz, ">> %s removed from banlist.\r\n",
		    buf);
		sendall(sendbuf, ALL, pplayer);
	} else
		sendtoplayer(pplayer, BAN_NOTFOUND);
	return PARSE_OK;
}

parse_error
format_uptime(struct splayer *pplayer)
{
	snprintf(sendbuf, sendbufsz,
	    ">> Lorien %s has been running for %s.\r\n", VERSION,
	    timelet(lorien_boot_time, 40000));
	sendtoplayer(pplayer, sendbuf);
	snprintf(sendbuf, sendbufsz,
	    ">> %d of a possible %lu lines are in use.\r\n", numconnected(),
	    MAXCONN - 4);
	sendtoplayer(pplayer, sendbuf);
	return PARSE_OK;
}

parse_error
add_channel(struct splayer *pplayer, char *buf)
{
	snprintf(sendbuf, sendbufsz, ">> Not implemented.\r\n");
	sendtoplayer(pplayer, sendbuf);
	return PARSE_OK;
}

void
prelogon(struct splayer *pplayer, char *buf)
{
	if (pplayer->spamming) {
		if (!strcmp(buf, ".")) {
			snprintf(sendbuf, sendbufsz,
			    "250 2.0.0 ZZZZZZZZ Message logged, you criminal, now Go Away\r\n");
			sendtoplayer(pplayer, sendbuf);
			pplayer->spamming = 0;
		} else
			; /* NULL */
	} else {
		if (!strncmp(buf, ".q", 2) || !strncmp(buf, "/q", 2)) {
			playerquit(pplayer, ++buf);
		} else {
			if (*buf == '.' || *buf == '/')
				buf++;

			switch (*(buf)) {
			case 'E':
			case 'H':
				snprintf(sendbuf, sendbufsz,
				    "250 rootaction.net Hello spam [0.0.0.0], logging your every move\r\n");
				sendtoplayer(pplayer, sendbuf);
				return;
				break;
			case 'M':
				snprintf(sendbuf, sendbufsz,
				    "250 2.1.0 spammer@nowhere... Go Away\r\n");
				sendtoplayer(pplayer, sendbuf);
				return;
				break;
			case 'R':
				snprintf(sendbuf, sendbufsz,
				    "250 2.1.5 spammer@nowhere... Go Away\r\n");
				sendtoplayer(pplayer, sendbuf);
				return;
				break;
			case 'D':
				snprintf(sendbuf, sendbufsz,
				    "354 Enter spam, end with %c.%c on a line by itself\r\n",
				    34, 34);
				pplayer->spamming = 1;
				sendtoplayer(pplayer, sendbuf);
				return;
				break;
			case 'n':
				set_name(pplayer, buf + 1);
				break;
			case '?':
				showhelp(pplayer, &buf[1]);
				break;
			default:;
			};

			if (!(pplayer->privs & CANPLAY)) {
				snprintf(sendbuf, sendbufsz,
				    "220 You must choose a name, use .n YOURNAME to set your name.\r\n220 Type .? for help.\r\n");
				sendtoplayer(pplayer, sendbuf);
			}
		}
	}
}

// clang-format off
struct command commands[] = {
	CMD_DECLS(CMD_ADDCHANNEL, 0, 2, COSYSOP, add_channel),
	CMD_DECLS(CMD_ADDPLAYER, 0, 2, SUPREME, enablePassword),
	CMD_DECLS(CMD_BANADD, 0, 2, COSYSOP, add_ban),
	CMD_DECLS(CMD_BANDEL, 0, 2, COSYSOP, delete_ban),
	CMD_DECL(CMD_BANLIST, 0, 1, ban_list),
	CMD_DECLS(CMD_BOARDADD, 0, 2, COSYSOP, add_board),
	CMD_DECLS(CMD_BOARDDEL, 0, 2, COSYSOP, delete_board),
	CMD_DECL(CMD_BOARDLIST, 0, 1, board_list),
	CMD_DECL(CMD_BEEPS, 0, 1, beeps),
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
	CMD_DECL(CMD_HELP, 0, 2, showhelp),
	CMD_DECL(CMD_HILITE, 0, 2, hilites),
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
	CMD_DECL(CMD_PASSWORD, 0, 2, changePlayer),
	CMD_DECL(CMD_POSE, 0, 2, pose_it),
	CMD_DECL(CMD_POST, 0, 2, bulletin_post),
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
	CMD_DECL(CMD_WHISPER, 0, 2, whisper),
	CMD_DECL(CMD_WHO, 0, 2, wholist),
	CMD_DECL(CMD_WHO2, 0, 2, wholist2),
	CMD_DECL(CMD_WRAP, 0, 2, playerwrap),
	/* JOEUSER can't yell, but thinks they can.
	 */
	CMD_DECL(CMD_YELL, 0, 2, yell),
	{ 0, 0, 0, 0, NULL, NULL }
};
// clang-format on

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
	snprintf(sendbuf, sendbufsz, ">> file %s, \r\n", cp);

	if (!parserfile) {
		if (pplayer) {
			snprintf(sendbuf, sendbufsz,
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
			snprintf(sendbuf, sendbufsz,
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
			snprintf(sendbuf, sendbufsz,
			    ">> unknown command %s\r\n", command);
			sendtoplayer(pplayer, sendbuf);
			parser_collapse_dyncontext(newctxp);
			fclose(parserfile);
			return PARSERR_SUPPRESS;
		}

		keyp = malloc(sizeof(struct parse_key));
		if (!keyp) {
			snprintf(sendbuf, sendbufsz,
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
			snprintf(sendbuf, sendbufsz,
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

		snprintf(sendbuf + strlen(sendbuf), sendbufsz - strlen(sendbuf),
		    "installed %lu parse keys.\r\n", newctxp->numentries);
	} else
		snprintf(sendbuf + strlen(sendbuf), sendbufsz - strlen(sendbuf),
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

	if (!(pplayer->privs & CANPLAY)) {
		prelogon(pplayer, command); /* hasn't set name yet */
	} else {
		if (!default_parser_entries) {
			/* done only once */
			default_parser_entries = parser_count_table_entries(
			    default_parse_table);
			if (!parser_init_context(&default_parser_context,
				default_parse_table, commands, false)) {
				logmsg("can't allocate parse context\n");
				exit(ENOMEM);
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

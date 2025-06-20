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

/* newplayer.c

Routines for handling players, sending messages to players, new players,
etc.

Chris Eleveld      (The Insane Hermit)
Robert Slaven      (The Celestial Knight/Tigger)
Jillian Alana Bolton  (Creel)
Dave Mott          (Energizer Rabbit)

*/

#include <sys/queue.h>

#include <assert.h>
#include <err.h>
#include <iconv.h>
#include <sysexits.h>

#include "ban.h"
#include "channel.h"
#include "commands.h"
#include "log.h"
#include "lorien.h"
#include "newplayer.h"
#include "parse.h"
#include "platform.h"
#include "servsock_ssl.h"

#define level(p, w) \
	((PLAYER_HAS(SHOW, p) || w->seclevel >= p->seclevel) ? p->seclevel : 1)

SLIST_HEAD(playerlist, splayer) playerhead = SLIST_HEAD_INITIALIZER(playerhead);

int numconnect; /* number of people connected */

char *player_flags_names[16] = { "Showlevel", "Verified", "Whisper Beeps",
	"Connection Messages", "Hushed", "Whisper Echoes", "Leaving", "Wrap",
	".i Messages", "Yell Mode (Screaming)", "Spamming", "ERROR! 11", NULL,
	NULL, NULL, NULL };

char *player_privs_names[16] = {
	"Yell",
	"Whisper",
	"Set own name",
	"Change channel",
	"Quit",
	"Use capital letters",
	"Play",
	"Post Bulletins",
	"ERROR! 8",
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
};

void
player_removegag(struct splayer *pplayer, struct splayer *target)
{
	FD_CLR(player_getline(target), &pplayer->gags);
}

int
numconnected()
{
	int i = 0;
	struct splayer *curr;

	SLIST_FOREACH(curr, &playerhead, entries)
		i++;

	return i;
}

void
playerinit(struct splayer *who, time_t when, char *where, char *numwhere)
{
	strlcpy(who->onfrom, where, sizeof(who->onfrom));
	strlcpy(who->host, where, sizeof(who->host));
	strlcpy(who->numhost, numwhere, sizeof(who->numhost));
	who->flags = PLAYER_DEFAULT;
	who->port = 0;
	who->seclevel = BABYCO;
	who->hilite = 0;
	who->chnl = NULL;
	strncpy(who->name, DEFAULT_NAME, MAX_NAME);
	who->name[MAX_NAME - 1] = (char)0;
	who->playerwhen = when;
	who->idle = when;
	who->cameon = when;
	who->privs = CANDEFAULT;
	who->dotspeeddial = 0;
	who->wrap = 80;
	who->spamming = 0;
	FD_ZERO(&who->gags);
	memset(who->pbuf, 0, sizeof(who->pbuf));
}

int
player_getline(struct splayer *pplayer)
{
	if (!pplayer || !pplayer->h)
		return -1;
	return pplayer->h->sock;
}

int
newplayer(struct servsock_handle *ssh)
{
	struct servsock_handle *h;
	struct splayer *buf, tmpbuf = { 0 };
	time_t tmptime;
	int err = 0, rc;

	h = acceptcon_ssl(ssh, tmpbuf.host, sizeof(tmpbuf.host), tmpbuf.numhost,
	    sizeof(tmpbuf.numhost), &tmpbuf.port);
	if (!h) {
		err = errno;
		logerror("acceptcon_ssl() failed", err);
		errno = err;
		return -1;
	}

	/* create a new player struct(record) */
	buf = (struct splayer *)calloc(1, sizeof(struct splayer));

	if (!buf) {
		snprintf(sendbuf, sendbufsz,
		    ">> Unable to allocate memory for player record.\r\n");
		(void)outtosock_ssl(h, sendbuf);
		(void)closesock_ssl(h);
		*(strchr(sendbuf, '\r')) = (char)0;
		logmsg(sendbuf);

		errno = ENOMEM;
		return -1;
	}

	tmptime = time((time_t *)0);
	playerinit(buf, tmptime, tmpbuf.host, tmpbuf.numhost);
	buf->h = h;

	snprintf(sendbuf, sendbufsz, "Someone came on from %s on line %d",
	    buf->host, buf->h->sock);
	logmsg(sendbuf);

	/* welcome our new arrival */

	numconnect++;
	rc = welcomeplayer(buf);
	if (rc < 0) {
		err = errno;
		logerror("welcomeplayer() failed", err);
		removeplayer(buf);
		errno = err;
		return -1;
	}

	SLIST_INSERT_HEAD(&playerhead, buf, entries);

	errno = 0;
	return player_getline(buf);
}

void
sendall(char *message, struct channel *channel, struct splayer *who)
{
	struct splayer *buf;

	SLIST_FOREACH(buf, &playerhead, entries) {
		if (channel == ALL)
			sendtoplayer(buf, message);
		else if (channel == INFORMATIONAL) {
			if (PLAYER_HAS(INFO, buf))
				sendtoplayer(buf, message);
		} else if (channel == ARRIVAL) {
			if (PLAYER_HAS(MSG, buf))
				sendtoplayer(buf, message);
		} else if (channel == YELL) {
			if (!PLAYER_HAS(HUSH, buf))
				sendtoplayer(buf, message);
		} else if (channel == DEPARTURE) {
			/* Un-gag the leaving player for the player we
			 * are processing. */
			player_removegag(buf, who);
			if (buf->dotspeeddial == who)
				buf->dotspeeddial = NULL;

			if (PLAYER_HAS(MSG, buf))
				sendtoplayer(buf, message);
		} else if (buf->chnl == channel)
			sendtoplayer(buf, message);
	}
}

int
setfds(fd_set *needread, bool removeplayers)
{
	struct splayer *curr;
	int max = 0;

again:
	SLIST_FOREACH(curr, &playerhead, entries) {
		if (removeplayers && PLAYER_HAS(LEAVING, curr)) {
			removeplayer(curr);
			goto again;
		}
		if (curr->h->sock > max)
			max = curr->h->sock;
		FD_SET(curr->h->sock, needread);
	}

	return max;
}

void
processinput(struct splayer *pplayer)
{
	time_t tmptime;
	char *myptr = pplayer->pbuf;
	char *line;
	char buf[OBUFSIZE];

	tmptime = time((time_t *)0);
	pplayer->idle = tmptime;

	while ((line = strtok(myptr, "\r\n"))) {
		myptr = (char *)0;

		if (!(pplayer->privs & CANPLAY)) {
			snprintf(buf, sizeof(buf), "spammer %s: %s",
			    pplayer->host, line);
			handlecommand(pplayer, line);
			if (!(pplayer->privs & CANPLAY))
				logmsg(buf);
			continue;
		}

		handlecommand(pplayer, line);
	}
}

int
defrag(struct splayer *pplayer, int inbytes, char *recvbuf, size_t recvbufsz)
{
	char *iptr;
	char *bufptr;
	char *eol;
	int pbsz = sizeof(pplayer->pbuf);
	int pblen;

	iptr = recvbuf;

	while (inbytes > 0) {
		bufptr = strchr(pplayer->pbuf, (char)0);
		pblen = bufptr - pplayer->pbuf;

		int copied = strlcat(bufptr, iptr, pbsz - pblen);

		inbytes -= copied;
		pblen += copied;
		iptr += copied;
		eol = strrchr(pplayer->pbuf, '\n');
		if (eol || (pblen >= pbsz)) {
			if (eol) {
				*eol++ = (char)0;
				pblen = pblen - (eol - pplayer->pbuf);
			} else
				pblen = 0;

			processinput(pplayer);
			if (PLAYER_HAS(LEAVING, pplayer)) {
				snprintf(sendbuf, sendbufsz,
				    "player %d is leaving",
				    player_getline(pplayer));
				logmsg(sendbuf);
				break;
			}

			if (eol) {
				memmove(pplayer->pbuf, eol, pblen);
				memset(&pplayer->pbuf[pblen], 0, pbsz - pblen);
			} else
				memset(pplayer->pbuf, 0, pbsz);
		}
	}
	return inbytes;
}

void
handleinput(fd_set needread)
{
	struct splayer *pplayer;
	int inbytes;

	SLIST_FOREACH(pplayer, &playerhead, entries) {
		if (!FD_ISSET(pplayer->h->sock, &needread))
			continue;

		if (PLAYER_HAS(LEAVING, pplayer))
			continue;

		do {
			inbytes = recvfromplayer(pplayer);

			if (inbytes == -1) {
				PLAYER_SET(LEAVING, pplayer);
				continue;
			}

			defrag(pplayer, inbytes, recvbuf, recvbufsz);
			if (PLAYER_HAS(LEAVING, pplayer)) {
				PLAYER_SET(LEAVING, pplayer);
				break;
			}
		} while (inbytes > 0);
	}
}

void
removeplayer(struct splayer *player)
{
	int count;

	if (player->chnl) {
		count = channel_deref(player->chnl);

		if ((count <= 0) && !channel_persists(player->chnl))
			channel_del(player->chnl);
	}

	snprintf(sendbuf, sendbufsz, "player %s(%d) from %s left", player->name,
	    player_getline(player), player->host);
	logmsg(sendbuf);

	snprintf(sendbuf, sendbufsz, ">> line %d(%s) just left.\r\n",
	    player_getline(player), player->name);

	if ((player_getline(player) <= (MAXCONN - 3))) {
		PLAYER_SET(LEAVING, player);
		if (player->privs & CANPLAY)
			sendall(sendbuf, DEPARTURE, player);
	}

	closesock_ssl(player->h);

	struct splayer *curr, *prev = NULL;
        bool found = false;
	SLIST_FOREACH(curr, &playerhead, entries) {
		if (curr == player) {
			found = true;
			break;
		}
		prev = curr;
	}

	if (found) {
		if (prev)
			SLIST_REMOVE_AFTER(prev, entries);
		else
			SLIST_REMOVE_HEAD(&playerhead, entries);
	}

	free((struct splayer *)player);
	numconnect--;
}

parse_error
kill_all_players(struct splayer *pplayer, char *buf)
{
	struct splayer *curr;

	SLIST_FOREACH(curr, &playerhead, entries)
		if (curr->seclevel < SUPREME)
			PLAYER_SET(LEAVING, curr);

	return PARSE_OK;
}

char *
idlet(time_t idle)
{
	return timelet(idle, 2);
}

/* Creel 6 Sep 94 */
parse_error
wholist(struct splayer *pplayer, char *instring)
{
	char *target;
	struct splayer *buf;
	int count = 0;
	int match;

	target = skipspace(instring);
	if (!*target)
		target = NULL;

#ifdef ONFROM_ANY
	snprintf(sendbuf, sendbufsz, "%-4s %-27s %-13s %-6s %-25s\r\n", "Line",
	    "Name", "Channel", "Idle", "Doing  ");
#else
	snprintf(sendbuf, sendbufsz, "%-4s %-27s %-13s %-6s %-25s\r\n", "Line",
	    "Name", "Channel", "Idle", "On From");
#endif
	sendtoplayer(pplayer, sendbuf);
	sendtoplayer(pplayer, LINE);

	SLIST_FOREACH(buf, &playerhead, entries) {
		match = 0;
		int line = player_getline(buf);
		if (target) {
			snprintf(sendbuf, sendbufsz, "%d", line);
			if (atoi(target) && !strchr(target, '.')) {
				if (atoi(target) == line)
					match = 1;
			} else if (strstr(buf->name, target) ||
			    strstr(buf->onfrom, target) ||
			    strstr(buf->host, target) ||
			    strstr(sendbuf, target))
				match = 1;
			else if (buf->privs & CANPLAY)
				if (strstr(buf->chnl->name, target))
					match = 1;
		}

		snprintf(sendbuf, sendbufsz,
		    "%c%c%-2d %-27.27s %-13.13s %-6s %-25.25s\r\n",
		    PLAYER_HAS(HUSH, buf) ? 'H' : ' ',
		    (buf->chnl) ? ((buf->chnl->secure) ? 'S' : ' ') : ' ', line,
		    buf->name, buf->chnl ? (buf->chnl->name) : " ",
		    idlet(buf->idle), buf->onfrom);

		if ((!target) || (target && match)) {
			sendtoplayer(pplayer, sendbuf);
			count++;
		};
	}

	sendtoplayer(pplayer, LINE);

	if (count == 1)
		snprintf(sendbuf, sendbufsz, ">> %d record displayed.\r\n",
		    count);
	else
		snprintf(sendbuf, sendbufsz, ">> %d records displayed.\r\n",
		    count);

	sendtoplayer(pplayer, sendbuf);
	return PARSE_OK;
}

parse_error
wholist2(struct splayer *pplayer, char *instring)
{
	struct splayer *buf;
	int count = 0;
	char vrfy[4][4];
	int match;
	char *target;

	target = skipspace(instring);
	if (!*target)
		target = NULL;

	strcpy(vrfy[0], "No");
	strcpy(vrfy[1], "Yes");

	/*
	  Line Name            On For   Vrfy     Host:Port         Lev
	  -------------------------------------------------------------------------------
	  1  ?              4s       No       localhost        :1309     1
	  -------------------------------------------------------------------------------
	*/

	snprintf(sendbuf, sendbufsz, "%-4s %-26s %-8s %-8s %-15s %-8s %-3s\r\n",
	    "Line", "Name", "On For", "Vrfy", "Host", "Port", "Lev");
	sendtoplayer(pplayer, sendbuf);
	sendtoplayer(pplayer, LINE);

	SLIST_FOREACH(buf, &playerhead, entries) {
		int line = player_getline(buf);
		match = 0;
		if (target) {
			snprintf(sendbuf, sendbufsz, "%d", line);
			if (atoi(target) && !strchr(target, '.')) {
				if (atoi(target) == line)
					match = 1;
			} else if (strstr(buf->name, target) ||
			    strstr(sendbuf, target) ||
			    strstr(buf->onfrom, target))
				match = 1;
		}

		snprintf(sendbuf, sendbufsz,
		    "%c%c%-2d %-26.262s %-8s %-8s %-15.15s %-8d ",
		    PLAYER_HAS(HUSH, buf) ? 'H' : ' ',
		    (buf->chnl) ? ((buf->chnl->secure) ? 'S' : ' ') : ' ', line,
		    buf->name, timelet(buf->cameon, 2),
		    vrfy[PLAYER_HAS(VRFY, buf) ? 1 : 0], buf->numhost,
		    buf->port);

		if ((!target) || (target && match)) {
			sendtoplayer(pplayer, sendbuf);

			snprintf(sendbuf, sendbufsz, "%-3d\r\n",
			    (pplayer->seclevel > 0) ? level(buf, pplayer) : 1);
			sendtoplayer(pplayer, sendbuf);

			count++;
		};
	}

	sendtoplayer(pplayer, LINE);

	if (count == 1)
		snprintf(sendbuf, sendbufsz, ">> %d record displayed.\r\n",
		    count);
	else
		snprintf(sendbuf, sendbufsz, ">> %d records displayed.\r\n",
		    count);

	sendtoplayer(pplayer, sendbuf);
	return PARSE_OK;
}

parse_error
wholist3(struct splayer *pplayer)
{
	struct splayer *buf;
	int count = 0;

	sendtoplayer(pplayer, LINE);

	SLIST_FOREACH(buf, &playerhead, entries) {
		int line = player_getline(buf);
		snprintf(sendbuf, sendbufsz, "%2d%c)%-14.14s", line,
		    PLAYER_HAS(HUSH, buf) ? 'H' : ' ', buf->name);
		sendtoplayer(pplayer, sendbuf);
		count++;
		if (!(count % 4))
			sendtoplayer(pplayer, "\r\n");
	}

	if (count % 4)
		sendtoplayer(pplayer, "\r\n");

	sendtoplayer(pplayer, LINE);

	if (count == 1)
		snprintf(sendbuf, sendbufsz, ">> %d record displayed.\r\n",
		    count);
	else
		snprintf(sendbuf, sendbufsz, ">> %d records displayed.\r\n",
		    count);

	sendtoplayer(pplayer, sendbuf);

	return PARSE_OK;
}

// clang-format off
static char badchars[] = {
	0x01, /* SOH */
	0x02, /* STX */
	0x03, /* ETX aka ^D aka EOF */
	0x04, /* EOT */
	0x05, /* ENQ */
	0x06, /* ACK */
	0x07, /* BEL */
	0x08, /* BS */
	/* 0x09, HT */
	/* 0x0a, LF */
	0x0b, /* VT */
	0x0c, /* FF */
	/* 0x0d, CR */
	0x0e, /* SO */
	0x0f, /* SI */
	0x10, /* DLE */
	0x11, /* DC1 aka ^Q */
	0x12, /* DC2 */
	0x13, /* DC3 aka ^S */
	0x14, /* DC4 */
	0x15, /* NAK */
	0x16, /* SYN */
	0x17, /* ETB */
	0x18, /* CAN */
	0x19, /* EM */
	0x1a, /* SUB */
	0x1b, /* ESC */
	0x1c, /* FS */
	0x1d, /* GS */
	0x1e, /* RS */
	0x1f, /* US */
	0x00  /* Terminate the string */
};
// clang-format on

int
cleanupbuf(char *inbuf, size_t inbufsz, bool removecaps)
{
	iconv_t cd;
	size_t rc;
	char buf[2 * inbufsz];
	size_t ileft = strnlen(inbuf, inbufsz - 1);
	size_t oleft = sizeof(buf) - 1;
	char *op = buf;
	char *ip = inbuf;

	memset(buf, 0, sizeof(buf));

	cd = iconv_open("UTF-8", "UTF-8");
	if (cd == (iconv_t)-1)
		err(EX_SOFTWARE, "iconv_open");

	while (ileft && oleft) {
		rc = iconv(cd, &ip, &ileft, &op, &oleft);
		if (rc == (size_t)-1) {
			assert(errno != E2BIG);
			if (errno == EINVAL || errno == EILSEQ) {
				if (oleft) {
					*op = '.';
					op++;
				}
				ileft--;
				ip++;
			}
		}
	}

	/* strip bad characters */
	op = buf;
	while (op)
		if ((op = strpbrk(op, badchars)))
			*op = '.';

	op = buf;
	while (op)
		if ((op = strchr(op, '\r')))
			*op = '\n';

	if (removecaps)
		for (size_t i = 0; i < strnlen(buf, sizeof(buf)); ++i)
			if (isascii(buf[i]))
				buf[i] = tolower(buf[i]);

	rc = strlcpy(inbuf, buf, inbufsz);
	iconv_close(cd);
	return rc;
}

int
recvfromplayer(struct splayer *who)
{
	int numread;
	int outsz;
	bool removecaps = !(who->privs & CANCAPS);

	numread = infromsock_ssl(who->h, recvbuf, recvbufsz);
	if (numread == -1)
		return numread;
	outsz = cleanupbuf(recvbuf, recvbufsz, removecaps);
	if (outsz >= recvbufsz)
		return -1; /* buffer overrun */

	return outsz;
}

int
isgagged(struct splayer *who, struct splayer *sender)
{
	int line = player_getline(sender);
	if ((line < 1) || (line >= MAXCONN))
		return 0;
	else
		return (FD_ISSET(line, &who->gags));
}

char *
wrap(char *s, int w)
{
	static char wrapped[BUFSIZE * 2];
	int i = 0; /* the number of characters copied. */
	int j = 0; /* the index into wrapped buffer */

	memset(wrapped, 0, BUFSIZE * 2);
	while (*s) {
		if (!strncmp(s, "\r\n", 2) || !strncmp(s, "\n\r", 2)) {
			strcat(wrapped, "\r\n");
			s += 2;
			j += 2;
			i = 0;
		} else if (*s == '\n' || *s == '\r') {
			strcat(wrapped, "\r\n");
			s++;
			j += 2;
			i = 0;
		} else {
			wrapped[j] = *s;
			j++;
			i++;
			s++;
			if (!((i + 1) % w)) {
				/* if a newline is at the wrap column, only send
				 * it once */
				if (*s == '\n' || *s == '\r')
					continue;

				strcat(wrapped, "\r\n"); /* indent following
							    lines 3 spaces */
				j += 2;
				i = 0;
			}
		}
	}
	return wrapped;
}

int
sendtoplayer(struct splayer *who, char *message)
{
	int line;
	int end = 1;
	int e;

	if (PLAYER_HAS(LEAVING, who))
		return 0;

	if (message[0] == '(') {
		struct splayer *sender;

		while (!isdigit(message[end]) && message[end])
			end++;
		line = atoi(&message[end]);
		sender = player_lookup(line);
		if (!sender)
			return 0;
		if (isgagged(who, sender))
			return 0; /* do not alert gagged player */
	}

	if (outtosock_ssl(who->h,
		PLAYER_HAS(WRAP, who) ? wrap(message, who->wrap) : message) ==
	    -1) {
		e = errno;
		logerror("outtosock_ssl() failed", e);
		errno = e;
		PLAYER_SET(LEAVING, who);
		return -1;
	}
	return 0;
}

int
welcomeplayer(struct splayer *pplayer)
{
	FILE *fpWELCOME;
	char ibuf[BUFSIZE];
	int e, rc;

	if ((fpWELCOME = fopen(WELCOMEFILE, "r")) == (FILE *)0) {
		e = errno;
		sendtoplayer(pplayer, "Unable to open welcome file.\r\n");
		snprintf(ibuf, sizeof(ibuf), "Unable to open welcome file %s",
		    WELCOMEFILE);
		logerror(ibuf, e);
		errno = 0;
		return 0;
	}

	while (fgets(ibuf, BUFSIZE, fpWELCOME) != NULL) {
		snprintf(sendbuf, sendbufsz, "220 %s\r", ibuf);
		rc = sendtoplayer(pplayer, sendbuf);
		if (rc < 0) {
			e = errno;
			logerror("cannot send welcomefile", e);
			errno = e;
			fclose(fpWELCOME);
			return -1;
		}
	}

	(void)fclose(fpWELCOME);

	snprintf(sendbuf, sendbufsz, "220 This site is running Lorien %s\r\n",
	    VERSION);
	rc = sendtoplayer(pplayer, sendbuf);
	if (rc < 0) {
		e = errno;
		logerror("cant send version", e);
		errno = e;
		return -1;
	}
	errno = 0;
	return 0;
}

struct splayer *
player_lookup(int linenum)
{
	struct splayer *tmp;

	if (linenum == 0)
		return (struct splayer *)0;

	SLIST_FOREACH(tmp, &playerhead, entries) {
		if (tmp->h->sock == linenum)
			return tmp;
	}

	return (struct splayer *)0;
}

struct splayer *
player_find(const char *name)
{
	struct splayer *tmp;

	if (!name)
		return (struct splayer *)0;

	SLIST_FOREACH(tmp, &playerhead, entries) {
		if (!strcmp(name, tmp->name))
			return tmp;
	}

	return (struct splayer *)0;
}

int
setname(struct splayer *pplayer, char *name)
{
	char *buf = name;

	if (pplayer == NULL) {
		return -1;
	} else {
		/* check for returns and newlines */
		if ((buf = (char *)strchr(name, '\r')) != (char *)0) {
			*buf = '\000';
		}
		if ((buf = (char *)strchr(name, '\n')) != (char *)0) {
			*buf = '\000';
		}

		buf = name;
		buf = (char *)skipspace(buf);

		if (*buf != '\000') {
			strlcpy(pplayer->name, buf, sizeof(pplayer->name));
			snprintf(sendbuf, sendbufsz, ">> Name changed.\r\n");
		} else {
			snprintf(sendbuf, sendbufsz, ">> Invalid name\r\n");
		}
		sendtoplayer(pplayer, sendbuf);
	}
	return 0;
}

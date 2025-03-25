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

#include "ban.h"
#include "commands.h"
#include "journal.h"
#include "log.h"
#include "lorien.h"
#include "newplayer.h"
#include "parse.h"
#include "platform.h"
#include "ring.h"
#include "servsock.h"

#define level(p, w) \
	((PLAYER_HAS(SHOW, p) || w->seclevel >= p->seclevel) ? p->seclevel : 1)

struct splayer *players; /* the linked list of players records */
chan *channels;
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

int
numconnected()
{
	int i;
	struct splayer *curr;

	curr = players->next;
	i = 0;
	while (curr) {
		curr = curr->next;
		i++;
	}
	return i;
}

void
initplayerstruct(void)
{
	/* get a place to put our players */

	players = (struct splayer *)malloc(sizeof(struct splayer));

	if (players == (struct splayer *)0) {
		fprintf(stderr,
		    "lorien: Unable to allocate memory for players!\n");
		exit(1);
	}

	if ((channels = (chan *)malloc(sizeof(chan))) == (chan *)0) {
		fprintf(stderr,
		    "lorien: Unable to allocate memory for channels!\n");
		exit(1);
	}

	channels->next = (chan *)0;
	strncpy(channels->name, DEFCHAN, sizeof(channels->name) - 1);
	channels->name[sizeof(channels->name) - 1] = (char)0;
	channels->secure = 0;
	channels->count = 0;
	channels->visited = 0;

	players->next = (struct splayer *)0;
	players->prev = (struct splayer *)0;
}

#ifdef CHECK_CHANNELS
int
checkchannels(curr)
chan *curr;
{
	int rc;

	if (!curr)
		return 1;

	if (curr->visited) {
		log_msg(
		    "checkchannels():  found circular link in channel list.\n");
		return 0;
	}

	curr->visited = 1;
	rc = checkchannels(curr->next);
#ifdef FIX_CHANNEL_LINKS
	if (!rc) {
		curr->next = (chan)*0;
		rc = 1;
	}
#endif
	curr->visited = 0;
	return rc;
}
#endif

chan *
findchannel(char *name)
{
	chan *curr;

#ifdef CHECK_CHANNELS
	if (!checkchannels(channels)) {
		log_msg("findchannel(): channel list incorrect.");
		sleep(2);
		abort(1);
	}
#endif

	curr = channels;

	while (curr) {
		if (!strncmp(curr->name, name, MAX_CHAN))
			return curr;

		curr = curr->next;
	}

	return (chan *)0;
}

chan *
newchannel(char *name)
{
	chan *newchan;

	newchan = (chan *)malloc(sizeof(chan));

	if (!newchan)
		return newchan;

	newchan->secure = 0;
	newchan->visited = 0;
	newchan->persistent = 0;
	strncpy(newchan->name, name, MAX_CHAN);

	newchan->name[MAX_CHAN] = 0;
	newchan->next = (chan *)0;
	newchan->count = 0; /* we wouldn't be building this if someone hadn't
	requested it. */

	if (channels->next)
		newchan->next = channels->next;
	channels->next = newchan;

#ifdef CHECK_CHANNELS
	if (!checkchannels(channels)) {
		log_msg("newchannel():  channel list incorrect.");
		sleep(2);
		abort(1);
	}
#endif

	return newchan;
}

void
remove_channel(chan *channel)
{
	chan *tmp;

#ifdef CHECK_CHANNELS
	if (!checkchannels(channels)) {
		log_msg("remove_channel():  channel list incorrect on entry.");
		sleep(2);
		abort(1);
	}
#endif

	tmp = channels;

	if ((tmp == channel) || tmp->persistent)
		return; /* don't remove the main or other persistent channels */

	while (tmp->next) {
		if (tmp->next == channel) {
			tmp->next = channel->next;
			free(channel);
			break;
		}
		tmp = tmp->next;
	}

#ifdef CHECK_CHANNELS
	if (!checkchannels(channels)) {
		log_msg("remove_channel():  channel list incorrect on exit.");
		sleep(2);
		abort(1);
	}
#endif
}

void
playerinit(struct splayer *who, time_t when, char *where, char *numwhere)
{
	strcpy(who->onfrom, where);
	strcpy(who->host, where);
	strcpy(who->numhost, numwhere);
	who->flags = PLAYER_DEFAULT;
	who->port = players->port;
	who->seclevel = BABYCO;
	who->hilite = 0;
	who->chnl = NULL;
	strncpy(who->name, DEFAULT_NAME, MAX_NAME);
	who->name[MAX_NAME - 1] = (char)0;
	who->playerwhen = when;
	who->idle = when;
	who->cameon = when;
	who->next = (struct splayer *)0;
	who->privs = CANDEFAULT;
	who->dotspeeddial = 0;
	who->wrap = 80;
	who->spamming = 0;
	FD_ZERO(&who->gags);
	memset(who->pbuf, 0, sizeof(who->pbuf));
}

int
newplayer(int s)
{
	int sock;
	struct splayer *buf;
	time_t tmptime;

	sock = acceptcon(s, players->host, sizeof(players->host),
	    players->numhost, sizeof(players->numhost), &players->port);
	if (sock < 0)
		return (-1);

	/* find the tail of the linked list of players */

	buf = players;
	while (buf->next != (struct splayer *)0)
		buf = buf->next;

	/* create a new player struct(record) */
	buf->next = (struct splayer *)malloc(sizeof(struct splayer));

	if (buf->next == (struct splayer *)0) {
		/*
		 * use the dummy record at the beginning of the list to accept
		 * and dump the incoming call because we're out of memory.
		 */

		snprintf(sendbuf, sizeof(sendbuf),
		    ">> Unable to allocate memory for player record.\r\n");
		(void)outtosock(sock, sendbuf);
		(void)close(sock);

		/* not fatal should let others continue to chat */
		return -1;
	}

	buf->next->next = (struct splayer *)0;
	buf->next->prev = buf;

	tmptime = time((time_t *)0);
	buf = buf->next;
	playerinit(buf, tmptime, players->host, players->numhost);
	buf->s = sock;

#ifndef NO_LOG_CONNECT
	snprintf(sendbuf, sizeof(sendbuf),
	    "Someone came on from %s on line %d\n", buf->host, buf->s);

	log_msg(sendbuf);
#endif

	/* welcome our new arrival */

	numconnect++;
	welcomeplayer(buf);

	return buf->s;
}

void
resetgags(int line)
{
	struct splayer *buf;

	buf = players;
	while (buf->next != (struct splayer *)0) {
		buf = buf->next;

		FD_CLR(line, &buf->gags);
	};
}

void
sendall(char *message, chan *channel, int line)
{
	struct splayer *buf, *before;

	buf = players;
	while (buf->next != (struct splayer *)0) {
		before = buf;
		buf = buf->next;

		if (channel == ALL)
			sendtoplayer(buf, message);
		else if (channel == INFORMATIONAL) {
			if (PLAYER_HAS(INFO, buf))
				sendtoplayer(buf, message);
		} else if (channel == ARRIVAL) {
			if (PLAYER_HAS(MSG, buf))
				sendtoplayer(buf, message);
		} else if (channel == DEPARTURE) {
			if (buf->s == line)
				before->next = buf->next;
			else {
				/* Un-gag the leaving player for the player we
				 * are processing. */

				FD_CLR(line, &buf->gags);
				if (buf->dotspeeddial == line)
					buf->dotspeeddial = 0;

				if (PLAYER_HAS(MSG, buf))
					sendtoplayer(buf, message);
			}
		} else if (buf->chnl == channel)
			sendtoplayer(buf, message);
	}
}

int
setfds(fd_set *needread)
{
	struct splayer *pplayer;
	int max = 0;

	pplayer = players;
	while (pplayer->next != (struct splayer *)0) {
		pplayer = pplayer->next;
		if (pplayer->s > max)
			max = pplayer->s;
		FD_SET(pplayer->s, needread);
	}
	return max;
}

void
processinput(struct splayer *pplayer)
{
	time_t tmptime;
	char *myptr = pplayer->pbuf;
	char *line;

	tmptime = time((time_t *)0);
	pplayer->idle = tmptime;

	while ((line = strtok(myptr, "\r\n"))) {
		myptr = (char *)0;

		if (!(pplayer->privs & CANPLAY)) {
			snprintf(sendbuf, sizeof(sendbuf), "spammer %s: %s\n",
			    pplayer->host, line);
			handlecommand(pplayer, line);
			if (!(pplayer->privs & CANPLAY))
				log_msg(sendbuf);
			continue;
		}

		handlecommand(pplayer, line);
	}
	memset(pplayer->pbuf, 0, sizeof(pplayer->pbuf));
}

void
handleinput(fd_set needread)
{
	char *iptr;
	char *bufptr;
	struct splayer *pplayer;
	struct splayer *tplayer;

	tplayer = players->next;

	while (tplayer != (struct splayer *)0) {
		pplayer = tplayer;
		tplayer = pplayer->next;

		if (PLAYER_HAS(LEAVING,
			pplayer)) { /* avoid the need to wait for input */
			removeplayer(pplayer);
			continue;
		}

		if (FD_ISSET(pplayer->s, &needread)) {
			/* check to see if they disconnected */

			if (recvfromplayer(pplayer, recvbuf, BUFSIZE) == -1) {
				removeplayer(pplayer);
				continue;
			}

			recvbuf[BUFSIZE - 1] = 0; /* ensure end of buffer. */

			/* now we have some data. */

			iptr = recvbuf;
			bufptr = strchr(pplayer->pbuf, (char)0);

			while (*iptr) {
				if (strlen(pplayer->pbuf) >= BUFSIZE) {
					sendtoplayer(pplayer,
					    ">> fatal:  %d byte buffer overrun.\r\n");
					recvbuf[0] = 0;

					bufptr = pplayer->pbuf;
					removeplayer(pplayer);
					break;
				}

				if (*iptr == '\r' || *iptr == '\n') {
					while (*iptr == '\r' || *iptr == '\n')
						iptr++;

					if (!PLAYER_HAS(LEAVING, pplayer)) {
						processinput(pplayer);
					}

					/* now we check again after processing
					 * the input. */

					if (PLAYER_HAS(LEAVING, pplayer)) {
						removeplayer(pplayer);
						break;
					}

					bufptr = pplayer->pbuf;
				} else {
					/* It's time to get rid of the caps, if
					 * necessary. */

					if (isascii(*iptr) && isprint(*iptr)) {
						if (!(pplayer->privs & CANCAPS))
							*bufptr++ = tolower(
							    *iptr);
						else
							*bufptr++ = *iptr;
					}
#ifndef NO_UTF_8
					/* without UTF-8 support we used to
					 * strip out non-ascii and
					 * non-printables
					 */
					else {
						*bufptr++ = *iptr;
					}
#endif
					iptr++;
				}
			}
		}
	}
}

void
removeplayer(struct splayer *player)
{
	if (player->chnl) {
		player->chnl->count--;

		if ((player->chnl->count <= 0) && !(player->chnl->persistent))
			remove_channel(player->chnl);
	} else {
		/* NULL */; /* can happen if player has not set name yet and is
			       in limbo */
	}

	snprintf(sendbuf, sizeof(sendbuf), ">> line %d(%s) just left.\r\n",
	    player->s, player->name);

	if ((player->s <= (MAXCONN - 3))) {
		PLAYER_SET(LEAVING, player);
		if (player->privs & CANPLAY)
			sendall(sendbuf, DEPARTURE, player->s);
	}

	shutdown(player->s, 2);

	(void)close(player->s);

	if (player->prev)
		player->prev->next = player->next;

	if (player->next)
		player->next->prev = player->prev;

	free((struct splayer *)player);
	numconnect--;
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

	buf = players->next;

#ifdef ONFROM_ANY
	snprintf(sendbuf, sizeof(sendbuf), "%-4s %-27s %-13s %-6s %-25s\r\n",
	    "Line", "Name", "Channel", "Idle", "Doing  ");
#else
	snprintf(sendbuf, sizeof(sendbuf), "%-4s %-27s %-13s %-6s %-25s\r\n",
	    "Line", "Name", "Channel", "Idle", "On From");
#endif
	sendtoplayer(pplayer, sendbuf);
	sendtoplayer(pplayer, LINE);

	while (buf) {
		match = 0;
		if (target) {
			snprintf(sendbuf, sizeof(sendbuf), "%d", buf->s);
			if (atoi(target) && !strchr(target, '.')) {
				if (atoi(target) == buf->s)
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

		snprintf(sendbuf, sizeof(sendbuf),
		    "%c%c%-2d %-27.27s %-13.13s %-6s %-25.25s\r\n",
		    PLAYER_HAS(HUSH, buf) ? 'H' : ' ',
		    (buf->chnl) ? ((buf->chnl->secure) ? 'S' : ' ') : ' ',
		    buf->s, buf->name, buf->chnl ? (buf->chnl->name) : " ",
		    idlet(buf->idle), buf->onfrom);

		if ((!target) || (target && match)) {
			sendtoplayer(pplayer, sendbuf);
			count++;
		};

		buf = buf->next;
	}

	sendtoplayer(pplayer, LINE);

	if (count == 1)
		snprintf(sendbuf, sizeof(sendbuf),
		    ">> %d record displayed.\r\n", count);
	else
		snprintf(sendbuf, sizeof(sendbuf),
		    ">> %d records displayed.\r\n", count);

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

	buf = players->next;

	/*
	  Line Name            On For   Vrfy     Host:Port         Lev
	  -------------------------------------------------------------------------------
	  1  ?              4s       No       localhost        :1309     1
	  -------------------------------------------------------------------------------
	*/

	snprintf(sendbuf, sizeof(sendbuf),
	    "%-4s %-26s %-8s %-8s %-15s %-8s %-3s\r\n", "Line", "Name",
	    "On For", "Vrfy", "Host", "Port", "Lev");
	sendtoplayer(pplayer, sendbuf);
	sendtoplayer(pplayer, LINE);

	while (buf) {
		match = 0;
		if (target) {
			snprintf(sendbuf, sizeof(sendbuf), "%d", buf->s);
			if (atoi(target) && !strchr(target, '.')) {
				if (atoi(target) == buf->s)
					match = 1;
			} else if (strstr(buf->name, target) ||
			    strstr(sendbuf, target) ||
			    strstr(buf->onfrom, target))
				match = 1;
		}

		snprintf(sendbuf, sizeof(sendbuf),
		    "%c%c%-2d %-26.262s %-8s %-8s %-15.15s %-8d ",
		    PLAYER_HAS(HUSH, buf) ? 'H' : ' ',
		    (buf->chnl) ? ((buf->chnl->secure) ? 'S' : ' ') : ' ',
		    buf->s, buf->name, timelet(buf->cameon, 2),
		    vrfy[PLAYER_HAS(VRFY, buf) ? 1 : 0], buf->numhost,
		    buf->port);

		if ((!target) || (target && match)) {
			sendtoplayer(pplayer, sendbuf);

			snprintf(sendbuf, sizeof(sendbuf), "%-3d\r\n",
			    (pplayer->seclevel > 0) ? level(buf, pplayer) : 1);
			sendtoplayer(pplayer, sendbuf);

			count++;
		};

		buf = buf->next;
	}

	sendtoplayer(pplayer, LINE);

	if (count == 1)
		snprintf(sendbuf, sizeof(sendbuf),
		    ">> %d record displayed.\r\n", count);
	else
		snprintf(sendbuf, sizeof(sendbuf),
		    ">> %d records displayed.\r\n", count);

	sendtoplayer(pplayer, sendbuf);
	return PARSE_OK;
}

parse_error
wholist3(struct splayer *pplayer)
{
	struct splayer *buf;
	int count = 0;

	buf = players->next;

	sendtoplayer(pplayer, LINE);

	while (buf) {
		snprintf(sendbuf, sizeof(sendbuf), "%2d%c)%-14.14s", buf->s,
		    PLAYER_HAS(HUSH, buf) ? 'H' : ' ', buf->name);
		sendtoplayer(pplayer, sendbuf);
		count++;
		if (!(count % 4))
			sendtoplayer(pplayer, "\r\n");

		buf = buf->next;
	}

	if (count % 4)
		sendtoplayer(pplayer, "\r\n");

	sendtoplayer(pplayer, LINE);

	if (count == 1)
		snprintf(sendbuf, sizeof(sendbuf),
		    ">> %d record displayed.\r\n", count);
	else
		snprintf(sendbuf, sizeof(sendbuf),
		    ">> %d records displayed.\r\n", count);

	sendtoplayer(pplayer, sendbuf);

	return PARSE_OK;
}

/* in 8-bit latin1 / ascii, valid printables are 0x20-0x7e and 0xa1-0xff
 * we can't necessarily rely on lib functions because we might be running
 * in a strange locale, but we assume clients use latin1, ascii or utf-8
 * which we limit to the printables that can be represented in only 8 bits
 */
#define isunclean(who, c) \
	(!(((c > 0x1fU) && (c < 0x7fU)) || ((c > 0xa0U) && (c <= 255U))))

void
cleanupbuf(struct splayer *who)
{
	unsigned char tmpbuf[BUFSIZE];
	unsigned char *newc, *old;

	old = (unsigned char *)&recvbuf[0];
	newc = tmpbuf;
	while (*old != '\000') {
		if ((!isunclean(who, *old)) || (*old == '\n') ||
		    (*old == '\r')) {
			*newc = *old;
			newc++;
		}
		old++;
	}
	*newc = '\000';
	strcpy(recvbuf, (const char *)tmpbuf);
}

int
recvfromplayer(struct splayer *who, char *data, int howmuch)
{
	int error;

	error = infromsock(who->s, data, howmuch);
	/* remove nasty chars */
	(void)cleanupbuf(who);

	return error;
}

int
isgagged(struct splayer *who, int line)
{
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
	int gagck = 0;

	if (PLAYER_HAS(LEAVING, who))
		return 0;
	/* If it is a message from a player, then we want to
    listen to the returned value from isgagged, otherwise, go ahead
    and do isgagged because it takes all of 2 nanoseconds, but ignore the
    result. */

	if (message[0] == '(') {
		while (!isdigit(message[end]) && message[end])
			end++;
		line = atoi(&message[end]);
		gagck = 1;
	};

	/* if we are not checking for gags, or we are and the sender is not
	 * gagged.
	 */

	if (!(gagck && isgagged(who, line)))

		if (outtosock(who->s,
			PLAYER_HAS(WRAP, who) ?
			    wrap(message, who->wrap) :
			    message) == -1) { /* no one there */
			PLAYER_SET(LEAVING, who);
			return 0;
		};
	return 1;
}

int
welcomeplayer(struct splayer *pplayer)
{
	FILE *fpWELCOME;
	char ibuf[BUFSIZE];

	if ((fpWELCOME = fopen(WELCOMEFILE, "r")) == (FILE *)0) {
		(void)sendtoplayer(pplayer, "Unable to open welcome file.\r\n");
		fprintf(stderr, "Unable to open welcome file %s.\n",
		    WELCOMEFILE);
		return 0;
	}

	while (fgets(ibuf, BUFSIZE, fpWELCOME) != NULL) {
		snprintf(sendbuf, sizeof(sendbuf), "220 %s\r", ibuf);
		(void)sendtoplayer(pplayer, sendbuf);
	}

	(void)fclose(fpWELCOME);

	snprintf(sendbuf, sizeof(sendbuf),
	    "220 This site is running Lorien %s\r\n", VERSION);
	sendtoplayer(pplayer, sendbuf);
	return 1;
}

struct splayer *
lookup(int linenum)
{
	struct splayer *tmp = players;

	if (linenum == 0)
		return (struct splayer *)0;

	while (tmp->next != (struct splayer *)0) {
		tmp = tmp->next;
		if (tmp->s == linenum)
			return tmp;
	}

	return (struct splayer *)0;
}

int
setname(int linenum, char *name)
{
	char *buf = name;
	struct splayer *tmp;

	/* find out who to set */
	tmp = lookup(linenum);
	if (tmp == (struct splayer *)0) {
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
			(void)strncpy(tmp->name, buf, MAX_NAME - 1);
			tmp->name[MAX_NAME - 1] = 0;
		} else {
			snprintf(sendbuf, sizeof(sendbuf),
			    ">> As if! Pick a real name!\r\n");
			sendtoplayer(tmp, sendbuf);
		}
	}
	return 0;
}

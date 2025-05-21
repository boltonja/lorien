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

/* help.c

   Routines for the Lorien Help system.
   Written by Jillian A. Bolton, Jr.

*/

#include "log.h"
#include "lorien.h"
#include "parse.h"
#include "platform.h"

#ifdef TESTHELP
#define sendtoplayer printhelp
void
printhelp(struct splayer *pplayer, char *buf)
{
	printf("%s", buf);
}
#else
#include "newplayer.h"
#endif

void
strupcase(char *s)
{
	int i;

	for (i = 0; s[i]; i++)
		if (isalpha(s[i]))
			s[i] = toupper(s[i]);
}

#define EXACT 1
#define SUB   2
#define REGEX 4 /* not yet implemented */

/* don't pass strings to strcontains which are any larger than BUFSIZE */

char *
strcontains(char *string, char *target, int mode)
{
	char s[BUFSIZE];
	char t[BUFSIZE];

	strcpy(s, string);
	strcpy(t, target);

	strupcase(s);
	strupcase(t);

	fflush(stdout);

	if (mode == SUB)
		return ((char *)strstr(s, t));
	return (!strcmp(s, t) ? string : (char *)0);
}

parse_error
showhelp(struct splayer *pplayer, char *buf)
{
	FILE *fHelp = fopen(HELPFILE, "r");
	int i, level;
	char *inbuf;
	char ibuf[BUFSIZE];
	char tagbuf[BUFSIZE];
	char outbuf[OBUFSIZE];
	char target[BUFSIZE];
	parse_error rc = PARSERR_SUPPRESS;

	sendtoplayer(pplayer, ">> Lorien Help System v1.1a\r\n");

	if (!fHelp) {
		sendtoplayer(pplayer, ">> Unable to open help file.\r\n");
		return rc;
	}

	while (isspace(*buf))
		buf++;

	if (buf[0] == '/' || buf[0] == '.' || buf[0] == ',')
		buf++;

	if (strlen(buf))
		strcpy(target, buf);
	else
		strcpy(target, "section");

	rewind(fHelp);
	while (fgets(ibuf, BUFSIZE, fHelp) != (char *)0) {
		char *tmp;
		inbuf = ibuf;

		/* remove newline */

		tmp = strstr(inbuf, "\n");
		if (tmp)
			*tmp = (char)0;

		/* check security level */

		if (isdigit(*inbuf)) {
			level = atoi(inbuf);
			if (level > pplayer->seclevel)
				continue;
			while (isdigit(*inbuf))
				inbuf++;
		}

		/* discard comments. */

		if (*inbuf == '#')
			continue;

		switch (*inbuf) {
		case 'M':
			tmp = strstr(inbuf, "|");
			if (tmp) {
				tmp++;
				snprintf(outbuf, sizeof(outbuf), "%s\r\n", tmp);
				sendtoplayer(pplayer, outbuf);
			} else {
				sendtoplayer(pplayer,
				    ">> error, malformed record in help file.\n");
				snprintf(outbuf, sizeof(outbuf),
				    "help:  malformed record: %s", inbuf);
#ifdef TESTHELP
				printf("%s", outbuf);
#else
				logmsg(outbuf);
#endif
			}
			break;
		case 'T':
			inbuf++;
			do {
				i = strcspn(inbuf, ",|");
				if (i) {
					strncpy(tagbuf, inbuf, i);
					tagbuf[i] = (char)0;
					if (strcontains(tagbuf, target,
						EXACT)) {
						tmp = strchr(inbuf, '|');
						if (tmp) {
							tmp++;
							snprintf(outbuf,
							    sizeof(outbuf),
							    "%s\r\n", tmp);
							sendtoplayer(pplayer,
							    outbuf);
						} else {
							sendtoplayer(pplayer,
							    ">> error, malformed record in help file.\n");
							snprintf(outbuf,
							    sizeof(outbuf),
							    "help:  malformed record: %s",
							    inbuf);
#ifdef TESTHELP
							printf("%s", outbuf);
#else
							logmsg(outbuf);
#endif
						}
						break;
					} else {
						inbuf += i + 1;
					}
				}
			} while (i);
			break;
		case 'B':
			tmp = strchr(inbuf, '|');
			if (tmp) {
				tmp++;
				snprintf(outbuf, sizeof(outbuf), "%s\r\n", tmp);
				sendtoplayer(pplayer, outbuf);
			} else {
				sendtoplayer(pplayer,
				    ">> error, malformed record in help file.\n");
				snprintf(outbuf, sizeof(outbuf),
				    "help:  malformed record: %s", inbuf);
#ifdef TESTHELP
				printf("%s", outbuf);
#else
				logmsg(outbuf);
#endif
			}
			break;
		}
	}

	sendtoplayer(pplayer, ">> End of help.\r\n");
	fclose(fHelp);
	return PARSE_OK;
}
#ifdef TESTHELP
int
main()
{
	struct splayer p;
	char buf[BUFSIZE];
	char *tmp;
	while (strcmp("quit\n", buf)) {
		fgets(buf, BUFSIZE - 1, stdin);
		tmp = strstr(buf, "\n");
		if (tmp)
			*tmp = (char)0;
		showhelp(&p, buf);
	}
}
#endif

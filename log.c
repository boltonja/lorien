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

/* log.c

   Routines for writing to the log file.

 Chris Eleveld      (The Insane Hermit)
 Robert Slaven      (The Celestial Knight/Tigger)
 Jillian Alana Bolton  (Creel)
 Daniel Lawrence    (bergon)

*/

#include "lorien.h"
#include "platform.h"

static FILE *fpLOG = (FILE *)0;

/** Get time of day **/
char *
timeset()
{
	long int buf; /* Should be time_t */
	char *sp, *cp;

	/* Get system time */
	time(&buf);

	/* Pass system time to converter */
	sp = ctime(&buf);

	/* Eat newline character */
	for (cp = sp; *cp; cp++)
		if (*cp == '\n') {
			*cp = '\0';
			break;
		}
	return (sp);
}

int
log_msg(char *what)
{
	char logbuf[OBUFSIZE];
	char *buf;

#ifndef LOG
	return 0;
#endif

	strncpy(logbuf, what, sizeof(logbuf));
	logbuf[OBUFSIZE - 1] = (char)0;
	buf = logbuf;

	/* remove unwanted characters \r and \n for now */

	if ((buf = (char *)strchr(what, '\r')) != (char *)0)
		*buf = '\000';
	if ((buf = (char *)strchr(what, '\n')) != (char *)0)
		*buf = '\000';

	/* do we have the file open yet? */

	if (fpLOG == NULL)
		fpLOG = fopen(LOGFILE, "a"); /* Creel */

	/* dump it out saying who we are */

	fprintf(fpLOG, "lorien: %s %s\n", timeset(), logbuf);
	fflush(fpLOG);

	return 0;
}

int
purgelog(struct splayer *pplayer)
{
#ifndef LOG
	return 1;
#endif

	fflush(fpLOG);
	fclose(fpLOG);
	fpLOG = fopen(LOGFILE, "w");
	fprintf(fpLOG, "lorien: %s %s purged the log.\n", timeset(),
	    pplayer->name);
	fflush(fpLOG);

	return 1;
}

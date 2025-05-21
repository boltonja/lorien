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

#include <assert.h>
#include <err.h>
#include <sysexits.h>

#include "log.h"
#include "lorien.h"
#include "platform.h"

char *sendbuf;
char *recvbuf;
static char *logbuf;
const size_t sendbufsz = BUFSIZE;
const size_t recvbufsz = BUFSIZE;
const size_t logbufsz = BUFSIZE;

void
log_alloc_buffers(void)
{
	sendbuf = malloc(sendbufsz);
	if (!sendbuf)
		err(EX_UNAVAILABLE, "cannot allocate sendbuf");
	recvbuf = malloc(recvbufsz);
	if (!recvbuf)
		err(EX_UNAVAILABLE, "cannot allocate recvbuf");
	logbuf = malloc(logbufsz);
	if (!logbuf)
		err(EX_UNAVAILABLE, "cannot allocate logbuf");
}

void
log_error(const char *prefix, int err, const char *file, int lineno)
{
	if (err == 0)
		return;

	(void)snprintf(logbuf, logbufsz, "%s: %s", prefix, strerror(err));
	log_msg(logbuf, file, lineno);
}

void
log_msg(const char *what, const char *file, int line)
{
	char *buf;
	int bufsz = logbufsz;
	time_t tim = time(NULL);

	buf = ctime_r(&tim, logbuf);
	buf = strchr(buf, '\n');
	if (!buf)
		buf = strchr(buf, '\0');
	assert(buf);

	bufsz -= strnlen(logbuf, bufsz);
	snprintf(buf, bufsz, " [%s:%d] %s", file, line, what);

	fprintf(stderr, "%s\n", logbuf);
	fflush(stderr);
}

int
purgelog(const char *who)
{
	fflush(stderr);
	fclose(stderr);
	stderr = freopen(LOGFILE, "w", stderr);
	err_set_file(stderr);

	snprintf(logbuf, logbufsz, "%s purged the log", who);
	logmsg(logbuf);
	fflush(stderr);

	return 1;
}

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

/* chat.c

 Routines for installing the chat server.

 Chris Eleveld      (The Insane Hermit)
 Robert Slaven      (The Celestial Knight/Tigger)
 Jillian Alana Bolton  (Creel)
 Dave Mott          (Energizer Rabbit)

 */

#include <err.h>
#include <sysexits.h>

#include "ban.h"
#include "board.h"
#include "channel.h"
#include "db.h"
#include "files.h"
#include "log.h"
#include "lorien.h"
#include "newplayer.h"
#include "platform.h"
#include "servsock_ssl.h"
#include "utility.h"

int
doit(struct servsock_handle *handle, struct servsock_handle *sslhandle)
{
	fd_set needread; /* for seeing which fds we need to read from */
	int num;	 /* the number of needy fds */
	int max;	 /* The highest fd we are using. */

	strncpy(lorien_db.dbname, "./lorien.db", sizeof(lorien_db.dbname) - 1);
	lorien_db.dbname[sizeof(lorien_db.dbname) - 1] = (char)0;

	int rc = ldb_open(&lorien_db);
	if (rc != 0) {
		// BUG: put the log on stderr so everything goes to the same
		// place
		err(rc, "lmdb can't open %s\r\n", lorien_db.dbname);
		return 1; /* NOTREACHED */
	}

	rc = ban_read_db();
	fprintf(stderr, "read %d bans from database\n", rc);
	rc = board_read_db();
	fprintf(stderr, "read %d boards from database\n", rc);

	channel_init();

#ifdef _MSC_VER
	{
		static WSADATA wsaData;
		int iResult;

		// Initialize Winsock
		iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (iResult != 0) {
			fprintf(stderr, "WSAStartup failed: %d\n", iResult);
			return 1;
		}
	}
#endif

	MAXCONN = gettablesize();

	if (MAXCONN > FD_SETSIZE)
		MAXCONN = FD_SETSIZE;

	while (1) {
		FD_ZERO(&needread);
		if (handle)
			FD_SET(handle->sock, &needread);
		if (sslhandle)
			FD_SET(sslhandle->sock, &needread);
		max = setfds(&needread, true);
		if (handle && (handle->sock > max))
			max = handle->sock;
		if (sslhandle && (sslhandle->sock > max))
			max = sslhandle->sock;

		if ((num = select(max + 1, &needread, (fd_set *)0, (fd_set *)0,
			 (struct timeval *)0)) == -1) {
			if (errno != EINTR) {
				logerror("lorien select failed", errno);
				continue;
			} else
				continue; /* do it again and get it right */
		}

		if (handle && FD_ISSET(handle->sock, &needread))
			if (newplayer(handle) == -1)
				logerror("cannot add player", errno);

		if (sslhandle && FD_ISSET(sslhandle->sock, &needread))
			if (newplayer(sslhandle) == -1)
				logerror("cannot add player", errno);

		handleinput(needread);
	}

	/*NOTREACHED*/
	return 0;
}

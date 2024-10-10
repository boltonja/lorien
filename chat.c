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

/* chat.c

 Routines for installing the chat server.

 Chris Eleveld      (The Insane Hermit)
 Robert Slaven      (The Celestial Knight/Tigger)
 Jillian Alana Bolton  (Creel)
 Dave Mott          (Energizer Rabbit)

 */

#include "files.h"
#include "lorien.h"
#include "newplayer.h"
#include "platform.h"
#include "servsock.h"
#include "utility.h"

char recvbuf[BUFSIZE];
char sendbuf[OBUFSIZE];

int s; /* our initial socket */

int
doit(int port)
{
	fd_set needread; /* for seeing which fds we need to read from */
	int num;	 /* the number of needy fds */
	int max;	 /* The highest fd we are using. */

	initplayerstruct();

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

	if (gethostname(sendbuf, sizeof(sendbuf)) == -1) {
		fprintf(stderr, "lorien: Error getting hostname!\n");
		exit(2);
	}

	fprintf(stderr, "Establishing a socket on %s on port %d...\n", sendbuf,
	    port);

	s = getsock(sendbuf, port);
	fprintf(stderr, "Socket established for Lorien on port %d.\n", port);

	MAXCONN = gettablesize();

	while (1) {
		FD_ZERO(&needread);
		FD_SET(s, &needread);
		max = setfds(&needread);

		if ((num = select(max + 1, &needread, (fd_set *)0, (fd_set *)0,
			 (struct timeval *)0)) == -1) {
			if (errno != EINTR) {
				perror("lorien select failed");
				continue;
			} else
				continue; /* do it again and get it right */
		}

		if (FD_ISSET(s, &needread))
			if (newplayer(s) == -1)
				fprintf(stderr,
				    "lorien: ran out of file descriptors!\n");

		handleinput(needread);
	}

	/*NOTREACHED*/
	return 0;
}

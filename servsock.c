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

#include "lorien.h"
#include "platform.h"
#include "servsock.h"
#include "utility.h"

#ifndef FNDELAY
#define FNDELAY O_NDELAY
#endif

#ifndef INADDR_NONE
#define INADDR_NONE (int)-1
#endif

int
getsock(char *address, int port)
{
	int s;
	struct in_addr tmpaddr;
	struct hostent *tmphost;
	struct sockaddr_in saddr;

	/* get a fresh socket */
	if ((s = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		logerror("lorien: Unable to get a new socket");
		die(errno);
	}

	/* decode address */
	if (isdigit(*address)) {
		if ((tmpaddr.s_addr = inet_addr(address)) ==
		    (u_short)INADDR_NONE) {
			logerror(
			    "lorien: Unable to figure out numeric address");
			die(errno);
		}
	} else {
		alarm(1);
		if ((tmphost = gethostbyname(address)) == (struct hostent *)0) {
			logerror("lorien: unable to get host adress:");
			die(errno);
		}
		alarm(0);
		memcpy((char *)&tmpaddr, tmphost->h_addr,
		    sizeof(struct in_addr));
	}

	/* set up socket address */
	memcpy((char *)&(saddr.sin_addr), (char *)&tmpaddr,
	    sizeof(struct in_addr));
	saddr.sin_addr.s_addr = INADDR_ANY;
	saddr.sin_family = AF_INET;
	saddr.sin_port = (unsigned short)htons((unsigned short)port);

	/* attempt to bind the socket to our address */
	if (bind(s, (struct sockaddr *)&saddr, sizeof(struct sockaddr_in)) ==
	    -1) {
		logerror("lorien: Error attempting to bind");
		die(errno);
	}

#if 0
   pptr = getprotobyname("tcp");
   if (pptr)
      memcpy(&p, pptr, sizeof(p));
   else
      p.p_proto = 6; /* because tcp is 6. */
   endprotoent();
      
   setsockopt(s, p.p_proto, SO_REUSEADDR, &so_true, sizeof(so_true));
   setsockopt(s, p.p_proto, SO_REUSEPORT, &so_true, sizeof(so_true));
#endif

	/* set our backlog of waiting connections to 5(max the system allows) */
	if (listen(s, 5) == -1) {
		logerror("lorien: Error listening");
		die(errno);
	}

	return s;
}

int
acceptcon(int s, char *from, int len, char *from2, int len2, int *port)
{
	int ns;
	socklen_t length = sizeof(struct sockaddr_in);
	char *buf;
	struct hostent *tmphost;
	struct sockaddr_in saddr;

	if ((ns = accept(s, (struct sockaddr *)&saddr, &length)) == -1) {
		logerror("lorien: Error trying to accept connections");
		return -1;
		/*die(errno);*/
	}

	//   fcntl(ns, F_SETFL, FNDELAY);

#ifndef SKIP_HOSTLOOKUP
	tmphost = gethostbyaddr((char *)&(saddr.sin_addr),
	    sizeof(struct in_addr), AF_INET);
#endif
	strncpy(from2, inet_ntoa(saddr.sin_addr), len2);
	from2[len2 - 1] = (char)0;
	if (tmphost == (struct hostent *)0) {
		logerror("lorien: unable to get host adress");
		(void)strncpy(from, from2, len);
	} else {
		buf = tmphost->h_name;
		if (buf != (char *)0) {
			(void)strncpy(from, buf, len);
		} else
			(void)strcpy(from, from2);
	}
	from[len - 1] = (char)0;

	*port = (int)ntohs((unsigned short)saddr.sin_port);

	return ns;
}

int
infromsock(int s, char *buffer, int size)
{
	int numchars;

	if ((numchars = recv(s, buffer, size - 1, 0x0)) <=
	    0) { /* panic for now */
		logerror("lorien: receive failed");
		return -1;
	}

	if (numchars == 0) { /* foreign end closed the connection be graceful */
		return -1;   /* tell the calling proc to deal with it. */
	}
	buffer[numchars] = buffer[size - 1] = '\000';

	return 0;
}

int
outtosock(int s, char *buffer)
{
	int length, numsent;

	length = strlen(buffer);
	if ((numsent = send(s, buffer, length, 0x0)) == -1) {
		logerror("lorien: send");
		fprintf(stderr,
		    "Attempted send to socket %d with message \"%s\"\n", s,
		    buffer);
		return -1;
	} else if (numsent < length) {
		fprintf(stderr, "lorien: send garbled lost characters.\n");
	}

	return 0;
}

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

#include <assert.h>
#include <err.h>
#include <unistd.h>

#include "ban.h"
#include "lorien.h"
#include "newplayer.h"
#include "platform.h"
#include "servsock_ssl.h"
#include "utility.h"

#ifndef FNDELAY
#define FNDELAY O_NDELAY
#endif

#ifndef INADDR_NONE
#define INADDR_NONE (int)-1
#endif

static void
alarmhandler(int s)
{
	return;
}

struct servsock_handle *
getsock_ssl(char *address, int port, bool use_ssl)
{
	struct servsock_handle *ssh;
	struct in_addr tmpaddr;
	struct hostent *tmphost;
	struct sockaddr_in saddr;

	ssh = calloc(1, sizeof(*ssh));
	if (!ssh)
		return NULL;

	/* get a fresh socket */
	ssh->sock = socket(AF_INET, SOCK_STREAM, 0);
	if (ssh->sock == -1) {
		logerror("lorien: Unable to get a new socket");
		exit(errno);
	}

	/* decode address */
	if (isdigit(*address)) {
		if ((tmpaddr.s_addr = inet_addr(address)) ==
		    (u_short)INADDR_NONE) {
			logerror(
			    "lorien: Unable to figure out numeric address");
			exit(errno);
		}
	} else {
		if ((tmphost = gethostbyname(address)) == (struct hostent *)0) {
			logerror("lorien: unable to get host adress:");
			exit(errno);
		}
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
	if (bind(ssh->sock, (struct sockaddr *)&saddr,
		sizeof(struct sockaddr_in)) == -1) {
		logerror("lorien: Error attempting to bind");
		exit(errno);
	}

	struct protoent *pptr = getprotobyname("tcp");
	int p_proto = (pptr) ? pptr->p_proto : 6; /* tcp should be 6... */
	endprotoent();

	int so_true = 1;

	setsockopt(ssh->sock, p_proto, SO_REUSEADDR, &so_true, sizeof(so_true));
#ifdef SO_REUSEPORT
	setsockopt(ssh->sock, p_proto, SO_REUSEPORT, &so_true, sizeof(so_true));
#endif
	if (listen(ssh->sock, 5) == -1) {
		logerror("lorien: Error listening");
		exit(errno);
	}

	if (use_ssl) {
		int rc;
		ssh->use_ssl = use_ssl;
		ssh->ctx = SSL_CTX_new(TLS_server_method());
		if (!ssh->ctx) {
			logerror("lorien: can't create SSL context");
			exit(1);
		}

		rc = SSL_CTX_set_min_proto_version(ssh->ctx, TLS1_2_VERSION);
		assert(rc == 1);

		if (SSL_CTX_use_certificate_file(ssh->ctx, "cert.pem",
			SSL_FILETYPE_PEM) <= 0) {
			logerror("lorien: can't open cert.pem");
			exit(1);
		}

		if (SSL_CTX_use_PrivateKey_file(ssh->ctx, "key.pem",
			SSL_FILETYPE_PEM) <= 0) {
			logerror("lorien: can't open key.pem");
			exit(1);
		}
	}
	return ssh;
}

struct servsock_handle *
acceptcon_ssl(struct servsock_handle *ssh, char *from, int len, char *from2,
    int len2, int *port)
{
	int ns;
	socklen_t length = sizeof(struct sockaddr_in);
	char *buf;
	struct hostent *tmphost = NULL;
	struct sockaddr_in saddr;
	struct servsock_handle *ssc;

	ssc = calloc(1, sizeof(*ssc));
	if (!ssc)
		return NULL;

	ns = accept(ssh->sock, (struct sockaddr *)&saddr, &length);
	if (ns == -1) {
		logerror("lorien: Error trying to accept connections");
		goto free_ssc;
	}

	ssc->sock = ns;
	if (ssh->use_ssl) {
		int rc;
		ssc->use_ssl = true;
		ssc->ssl = SSL_new(ssh->ctx);
		if (!ssc->ssl)
			goto close_sock;

		SSL_set_fd(ssc->ssl, ns);
		rc = SSL_accept(ssc->ssl);
		if (rc <= 0)
			goto close_sock;
	}

	/* If the socket number is less than MAXCONN, it is representable in the
	 * fd_set, and we can pass that to select(2). Otherwise, close the
	 * connection.
	 */
	if (ns >= MAXCONN) {
		snprintf(sendbuf, sendbufsz,
		    ">> All %lu connections are full.\r\n", MAXCONN);
		outtosock_ssl(ssc, sendbuf);
		goto close_ssc;
	}

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
	if (ban_findsite(from2) || ban_findsite(from)) {
		snprintf(sendbuf, sendbufsz,
		    ">> Your site is presently blocked.\r\n");
		outtosock_ssl(ssc, sendbuf);
		goto close_ssc;
	}

	*port = (int)ntohs((unsigned short)saddr.sin_port);

	return ssc;

close_ssc:
	if (ssh->use_ssl) {
		SSL_shutdown(ssc->ssl);
		SSL_free(ssc->ssl);
	}
close_sock:
	close(ns);
free_ssc:
	free(ssc);

	return NULL;
}

int
infromsock_ssl(struct servsock_handle *ssh, char *buffer, int size)
{
	int numchars;

	if (ssh->ssl) {
		int rc;
		struct sigaction oldaction = { 0 }, newaction = { 0 };
		newaction.sa_handler = alarmhandler;
		rc = sigaction(SIGALRM, &newaction, &oldaction);
		if (rc != 0)
			warn("sigaction failed");
		alarm(1);
		numchars = SSL_read(ssh->ssl, buffer, size - 1);
		alarm(0);
		rc = sigaction(SIGALRM, &oldaction, &newaction);
		if (rc != 0)
			warn("sigaction failed");
		if (numchars <= 0) {
			int rc = SSL_get_error(ssh->ssl, numchars);
			if (rc == SSL_ERROR_WANT_READ)
				numchars = 0;
			else
				return -1;
		}
	} else {
		numchars = recv(ssh->sock, buffer, size - 1, MSG_DONTWAIT);
		if ((numchars < 0) && (errno == EAGAIN))
			numchars = 0;
	}

	if (numchars < 0) {
		logerror("lorien: receive failed");
		return -1;
	}

	buffer[numchars] = (char)0;

	return numchars;
}

int
outtosock_ssl(struct servsock_handle *ssh, char *buffer)
{
	int length, numsent;

	length = strlen(buffer);
	if (length == 0)
		return 0;

	if (ssh->use_ssl)
		numsent = SSL_write(ssh->ssl, buffer, length);
	else
		numsent = send(ssh->sock, buffer, length, 0x0);

	if (numsent <= -1) {
		logerror("lorien: send");
		fprintf(stderr,
		    "Attempted send to socket %d with message \"%s\"\n",
		    ssh->sock, buffer);
		return -1;
	} else if (numsent < length) {
		fprintf(stderr, "lorien: send garbled lost characters.\n");
	}

	return 0;
}

int
closesock_ssl(struct servsock_handle *ssh)
{
	if (ssh->use_ssl) {
		SSL_shutdown(ssh->ssl);
		SSL_free(ssh->ssl);
	}
	close(ssh->sock);
	free(ssh);
	return 0;
}

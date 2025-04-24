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

#include <netinet/tcp.h>

#include <assert.h>
#include <err.h>
#include <sysexits.h>
#include <unistd.h>

#include "ban.h"
#include "log.h"
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

static int
decode_ssl_error(struct servsock_handle *ssh, int code)
{
	int e = SSL_get_error(ssh->ssl, code);
	int err = 0;

	switch (e) {
	case SSL_ERROR_NONE:
		break;
	case SSL_ERROR_ZERO_RETURN:
		err = ECONNRESET;
		break;
	case SSL_ERROR_WANT_ACCEPT:
	case SSL_ERROR_WANT_ASYNC:
	case SSL_ERROR_WANT_ASYNC_JOB:
	case SSL_ERROR_WANT_CLIENT_HELLO_CB:
	case SSL_ERROR_WANT_CONNECT:
	case SSL_ERROR_WANT_READ:
	case SSL_ERROR_WANT_WRITE:
	case SSL_ERROR_WANT_X509_LOOKUP:
		err = EAGAIN;
		break;
	case SSL_ERROR_SYSCALL:
	case SSL_ERROR_SSL:
		ssh->no_shutdown = true;
		err = ENOLINK;
		break;
	default:
		err = EPROTO;
	}
	return err;
}

static void
alarmhandler(int s)
{
	return;
}

static void
pipehandler(int s)
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
	if (ssh->sock == -1)
		err(EX_CANTCREAT, "Unable to get a new socket");

	/* decode address */
	if (isdigit(*address)) {
		if ((tmpaddr.s_addr = inet_addr(address)) ==
		    (u_short)INADDR_NONE)
			err(EX_NOHOST, "unable to get inet addr %s", address);
	} else {
		if ((tmphost = gethostbyname(address)) == (struct hostent *)0)
			err(EX_NOHOST, "unable to get host adress");

		memcpy((char *)&tmpaddr, tmphost->h_addr,
		    sizeof(struct in_addr));
	}

#if 0
	/* on BSD (and macOS) this slows new connections down, and it does not
	 * guarantee re-use before the usual 2 minute TCP time out
	 */
	int so_true = 1;
	struct protoent *pptr = getprotobyname("tcp");
	int p_proto = (pptr) ? pptr->p_proto : 6; /* tcp should be 6... */
	endprotoent();

	setsockopt(ssh->sock, p_proto, SO_REUSEADDR, &so_true, sizeof(so_true));
	setsockopt(ssh->sock, p_proto, SO_REUSEPORT, &so_true, sizeof(so_true));
#endif
	
	/* set up socket address */
	memcpy((char *)&(saddr.sin_addr), (char *)&tmpaddr,
	    sizeof(struct in_addr));
	saddr.sin_addr.s_addr = INADDR_ANY;
	saddr.sin_family = AF_INET;
	saddr.sin_port = (unsigned short)htons((unsigned short)port);

	/* attempt to bind the socket to our address */
	if (bind(ssh->sock, (struct sockaddr *)&saddr,
		 sizeof(struct sockaddr_in)) == -1)
		err(EX_CANTCREAT, "Unable to bind port %d", port);

	if (listen(ssh->sock, 5) == -1)
		err(EX_SOFTWARE, "listen() failed");

	if (use_ssl) {
		int rc;
		struct sigaction newaction = { 0 };

		ssh->use_ssl = use_ssl;

		ssh->ctx = SSL_CTX_new(TLS_server_method());
		if (!ssh->ctx)
			err(EX_CANTCREAT, "can't create SSL context");

		rc = SSL_CTX_set_min_proto_version(ssh->ctx, TLS1_2_VERSION);
		if (!rc)
			err(EX_SOFTWARE, "can't set minimum TLS version 1.2");

		rc = SSL_CTX_use_certificate_file(ssh->ctx, "cert.pem",
		    SSL_FILETYPE_PEM);
		if (rc <= 0)
			err(EX_NOINPUT, "can't open cert.pem");

		rc = SSL_CTX_use_PrivateKey_file(ssh->ctx, "key.pem",
		    SSL_FILETYPE_PEM);
		if (rc <= 0)
			err(EX_NOINPUT, "can't open key.pem");

		newaction.sa_handler = pipehandler;
		rc = sigaction(SIGPIPE, &newaction, NULL);
		if (rc != 0)
			logerror("sigaction (SIGPIPE) failed", errno);
	}
	return ssh;
}

struct servsock_handle *
acceptcon_ssl(struct servsock_handle *ssh, char *from, int len, char *from2,
    int len2, int *port)
{
	int ns, e = 0;
	socklen_t length = sizeof(struct sockaddr_in);
	char *buf;
	struct hostent *tmphost = NULL;
	struct sockaddr_in saddr;
	struct servsock_handle *ssc;

	ssc = calloc(1, sizeof(*ssc));
	if (!ssc) {
		e = ENOMEM;
		logerror("calloc failed", e);
		errno = e;
		return NULL;
	}

	ns = accept(ssh->sock, (struct sockaddr *)&saddr, &length);
	if (ns == -1) {
		e = errno;
		logerror("accept() failed", e);
		goto free_ssc;
	}

	int so_true = 1;
	struct protoent *pptr = getprotobyname("tcp");
	int p_proto = (pptr) ? pptr->p_proto : 6; /* tcp should be 6... */

	setsockopt(ns, p_proto, TCP_NODELAY, &so_true, sizeof(so_true));

	ssc->sock = ns;
	if (ssh->use_ssl) {
		int rc;
		ssc->use_ssl = true;
		ssc->ssl = SSL_new(ssh->ctx);

		if (!ssc->ssl) {
			e = ENOMEM;
			logerror("SSL_new() failed", e);
			goto close_sock;
		}

		SSL_set_fd(ssc->ssl, ns);
		rc = SSL_accept(ssc->ssl);
		if (rc <= 0) {
			e = decode_ssl_error(ssc, rc);
			logerror("SSL_accept failed", e);
			goto close_sock;
		}
	}

	/* If the socket number is less than MAXCONN, it is representable in the
	 * fd_set, and we can pass that to select(2). Otherwise, close the
	 * connection.
	 */
	if (ns >= MAXCONN) {
		snprintf(sendbuf, sendbufsz,
		    ">> All %lu connections are full.\r\n", MAXCONN);
		outtosock_ssl(ssc, sendbuf);
		logerror(sendbuf, EMFILE);
		goto close_ssc;
	}

#ifndef SKIP_HOSTLOOKUP
	tmphost = gethostbyaddr((char *)&(saddr.sin_addr),
	    sizeof(struct in_addr), AF_INET);
#endif
	strncpy(from2, inet_ntoa(saddr.sin_addr), len2);
	from2[len2 - 1] = (char)0;
	if (tmphost == (struct hostent *)0) {
		logmsg("unable to get host adress");
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
		int rc;

		snprintf(sendbuf, sendbufsz,
		    ">> Your site is presently blocked.\r\n");
		rc = outtosock_ssl(ssc, sendbuf);
		if (rc == -1) {
			e = errno;
			logerror("can't send ban message", e);
		}
		goto close_ssc;
	}

	*port = (int)ntohs((unsigned short)saddr.sin_port);

	return ssc;

close_ssc:
	if (ssh->use_ssl) {
		if (!ssh->no_shutdown)
			SSL_shutdown(ssc->ssl);
		SSL_free(ssc->ssl);
	}
close_sock:
	close(ns);
free_ssc:
	free(ssc);

	errno = e;
	return NULL;
}

int
infromsock_ssl(struct servsock_handle *ssh, char *buffer, int size)
{
	int numchars, rc;

	if (ssh->ssl) {
		int rc;
		struct sigaction oldaction = { 0 }, newaction = { 0 };

		newaction.sa_handler = alarmhandler;
		rc = sigaction(SIGALRM, &newaction, &oldaction);
		if (rc != 0)
			logerror("sigaction (SIGALRM) failed", errno);

		ERR_clear_error();
		alarm(1);
		numchars = SSL_read(ssh->ssl, buffer, size - 1);
		alarm(0);

		rc = sigaction(SIGALRM, &oldaction, &newaction);
		if (rc != 0)
			logerror("sigaction (SIGALRM) failed", errno);

		if (numchars <= 0) {
			rc = decode_ssl_error(ssh, numchars);
			if (rc == EAGAIN)
				numchars = 0;
			else {
				logerror("SSL error %d on SSL_read", rc);
				errno = rc;
				return -1;
			}
		}
	} else {
		numchars = recv(ssh->sock, buffer, size - 1, MSG_DONTWAIT);
		if (numchars < 0) {
			rc = errno;
			if (rc == EAGAIN)
				numchars = 0;
			else {
				logerror("receive failed", rc);
				errno = rc;
				return -1;
			}
		}
	}

	buffer[numchars] = (char)0;

	errno = 0;
	return numchars;
}

int
outtosock_ssl(struct servsock_handle *ssh, char *buffer)
{
	int length, numsent, e, retries = 10;

	length = strlen(buffer);
	if (length == 0) {
		logmsg("not sending 0 length buffer");
		errno = 0;
		return 0;
	}

	if (ssh->use_ssl) {
	sslretry:
		ERR_clear_error();
		numsent = SSL_write(ssh->ssl, buffer, length);
		if (numsent <= -1) {
			e = decode_ssl_error(ssh, numsent);
			if (e == EAGAIN) {
				if (--retries > 0) {
					goto sslretry;
				}
				logerror("retries exhausted", e);
				errno = 0;
				return 0;
			}
			logerror("SSL_write(3) failed", e);
			errno = e;
			return -1;
		}
	} else {
	sockretry:
		numsent = send(ssh->sock, buffer, length, 0x0);
		if (numsent <= -1) {
			e = errno;
			if (e == EAGAIN) {
				if (--retries > 0)
					goto sockretry;
				logerror("retries exhausted", e);
				errno = 0;
				return 0;
			}
			logerror("send(2) failed", e);
			errno = e;
			return -1;
		}
	}

	if (numsent < length) {
		if (ssh->use_ssl)
			logmsg("SSL_write() garbled lost characters");
		else
			logmsg("send() garbled lost characters");
	}

	errno = 0;
	return 0;
}

int
closesock_ssl(struct servsock_handle *ssh)
{
	if (ssh->use_ssl) {
		if (!ssh->no_shutdown)
			SSL_shutdown(ssh->ssl);
		SSL_free(ssh->ssl);
	}
	close(ssh->sock);
	free(ssh);
	return 0;
}

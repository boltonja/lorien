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

/* lorien.c

 Routines for parsing the command line and getting things rolling.

 Chris Eleveld      (The Insane Hermit)
 Robert Slaven      (The Celestial Knight/Tigger)
 Jillian Alana Bolton  (Creel)
 Dave Mott          (Energizer Rabbit)

*/

#define _LORIEN_C_

#include <err.h>
#include <sysexits.h>
#include <time.h>

#include "chat.h"
#include "log.h"
#include "lorien.h"
#include "platform.h"
#include "security.h"
#include "servsock_ssl.h"

time_t lorien_boot_time = 0;

int handleargs(int argc, char **argv);

static int port = 0;
static int sslport = 0;

static struct servsock_handle *handle;
static struct servsock_handle *sslhandle;
static char *logfile = NULL;

int
main(int argc, char **argv)
{
	lorien_boot_time = time((time_t *)NULL);

	handleargs(argc - 1, argv + 1);

	doit(handle, sslhandle);

	return 0;
}

int
handleargs(int argc, char **argv)
{
	int fdaemon = 0;
	int childid;
	int i;

	for (i = 0; i < argc; i++) {
		if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "-?")) {
			fprintf(stderr, USAGE);
			exit(0);
		}
		if (!strcmp(argv[i], "-d")) {
			fdaemon = 1;
		} else if (!strcmp(argv[i], "-l")) {
			if (++i >= argc) {
				errno = EINVAL;
				err(EX_DATAERR, "missing log file name");
			}
			logfile = argv[i];
		} else if (!strcmp(argv[i], "-s")) {
			if (++i >= argc) {
				errno = EINVAL;
				err(EX_DATAERR, "missing ssl port");
			} else
				sslport = atoi(argv[i]);

			if (!sslport)
				err(EX_DATAERR, "bad ssl port %s", argv[i]);
		} else if ((port = atoi(argv[i]))) {
			port = atoi(argv[i]);
			if (!port)
				err(EX_DATAERR, "bad port %s", argv[i]);
		} else {
			errno = EINVAL;
			err(EX_DATAERR, "Unrecognized command option <%s>",
			    argv[i]);
		}
	}

	if (!sslport && !port)
		err(EX_DATAERR, "you must specify at least one port");

	if (sslport) {
		int rc;

		rc = access("cert.pem", R_OK);
		if (rc != 0)
			err(EX_NOINPUT, "can't access cert.pem");
		rc = access("key.pem", R_OK);
		if (rc != 0)
			err(EX_NOINPUT, "can't access key.pem");
	}

	printf("starting lorien on port %d/+%d.\n", port, sslport);

	if (gethostname(sendbuf, sizeof(sendbuf)) == -1) {
		fprintf(stderr, "lorien: Error getting hostname!\n");
		exit(2);
	}

	if (port) {
		printf("Establishing socket on %s on port %d...\n",
			sendbuf, port);
		handle = getsock_ssl(sendbuf, port, false);
		if (!handle)
			err(EX_OSERR, "can't bind non-ssl port %d", port);
		printf("Socket established on port %d.\n", port);
	}

	if (sslport) {
		printf("Establishing socket on %s on port +%d...\n",
			sendbuf, sslport);
		sslhandle = getsock_ssl(sendbuf, sslport, true);
		if (!sslhandle)
			err(EX_OSERR, "can't bind ssl port %d", sslport);
		printf("Socket established on port +%d.\n", sslport);
	}

	if (logfile) {
		printf("redirecting stderr to %s\n", logfile);
		if (freopen(logfile, "a", stderr) == NULL)
			err(EX_OSERR, "unable to open logfile %s", logfile);
		err_set_file(stderr);
	}

#ifndef _MSC_VER
	if (fdaemon) {
		switch (childid = fork()) {
		case -1:
			fprintf(stdout, "ERROR starting daemon. exitting.\n");
			err(EX_OSERR, "fork");
			break;
		case 0:
			break;
		default:
			printf("Daemon's pid is %d.\n", childid);
			printf("This is the parent process signing off.\n");
			fclose(stdin);
			fclose(stdout);
			close(0);
			close(1);
			exit(0);
		}
	}
#endif
	fclose(stdin);
	fclose(stdout);
	return 0;
}

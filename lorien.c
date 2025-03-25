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

#include "chat.h"
#include "log.h"
#include "lorien.h"
#include "platform.h"
#include "security.h"

time_t lorien_boot_time = 0;

int handleargs(int argc, char **argv);

int port = 2525;

int
main(int argc, char **argv)
{
	printf("MAXCONN = %lu\n", MAXCONN);
	lorien_boot_time = get_timestamp();

	handleargs(argc, argv + 1);

	doit(port);

	return 0;
}

int
handleargs(int argc, char **argv)
{
	int fdaemon = 0;
	int childid;
	char *logfile = (char *)0;

	while (--argc) {
		if (**argv == '-') {
			switch ((*argv)[1]) {
			case 'd':
				fdaemon = 1;
				break;
			case 'l':
				if ((*argv)[2] != '\000') {
					logfile = (*argv) + 2;
				} else {
					if (argc < 1) {
						fprintf(stderr, USAGE);
						fprintf(stderr,
						    "-l requires you to specify a file name.\n");
						exit(1);
					}
					argc--, argv++;
					logfile = *argv;
				}
				break;
			default:
				fprintf(stderr,
				    "lorien: Unrecognized command line option <%s> on command line.\n",
				    *argv);
				exit(3);
			}
		} else if (isdigit(**argv)) {
			port = atoi(*argv);
		} else {
			fprintf(stderr,
			    "lorien: Unrecognized word <%s> on command line .\n",
			    *argv);
			exit(3);
		}

		argv++;
	}

	if (logfile != (char *)0)
		if (freopen(logfile, "w", stderr) == NULL) {
			printf(
			    "lorien: unable to open logfile, %s, on stderr.\n",
			    logfile);
			exit(4);
		}

#ifndef _MSC_VER
	if (fdaemon) {
		switch (childid = fork()) {
		case -1:
			fprintf(stdout, "ERROR starting daemon. exitting.\n");
			perror("lorien: fork");
			exit(2);
			break;
		case 0:
			if (logfile == (char *)0)
				(void)fclose(stderr);
			break;
		default:
			printf("Daemon succesfully Started on port %d.\n",
			    port);
			printf("Daemon's pid is %d.\n", childid);
			printf("This is the parent process signing off.\n");
			fclose(stdin);
			fclose(stdout);
			close(0);
			close(1);
			exit(0);
		}
		/* This line generates a warning if your header files are broken
		 and don't properly cast SIG_IGN. */
		//      signal(SIGPIPE,SIG_IGN);
	}
#endif
	fclose(stdin);
	fclose(stdout);
	return 0;
}

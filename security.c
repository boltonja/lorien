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

/* security.c

  Routines for supporting haven adminstration.

 Chris Eleveld      (The Insane Hermit)
 Robert Slaven      (The Celestial Knight/Tigger)
 Jillian Alana Bolton  (Creel)
 Dave Mott          (Energizer Rabbit)

*/

#include "db.h"
#include "log.h"
#include "lorien.h"
#include "newplayer.h"
#include "parse.h"
#include "platform.h"
/* .shutdown is available to level 4 and higher. */
#include "commands.h"
#include "security.h"

char passwds[NUMLVL + 2][MAX_POWER_PASS] = { "level0", "level1", "level2",
	"level3", "level4", "level5", "BRING_IT_DOWN", "defaultcmds" };

void
init_read_powerfile(void)
{
	FILE *pfile;
	char *tmp;
	char s[BUFSIZE];
	int index = 0;

	pfile = fopen("lorien.power", "r");
	if (pfile) {
		while (!feof(pfile) && index != (NUMLVL + 2)) {
			fgets(s, BUFSIZE - 1, pfile);
			if (!feof(pfile)) {
				if ((tmp = (char *)strstr(s, "\r")))
					*tmp = (char)0;
				if ((tmp = (char *)strstr(s, "\n")))
					*tmp = (char)0;
				strncpy(passwds[index], s, MAX_POWER_PASS);
				index++;
			}
		}
		fclose(pfile);
	}
}

int
printlevels(struct splayer *pplayer)
{
	int i;

	snprintf(sendbuf, sizeof(sendbuf),
	    ">> The passwords are as follows:\r\n");
	sendtoplayer(pplayer, sendbuf);
	for (i = 0; i < NUMLVL; i++) {
		snprintf(sendbuf, sizeof(sendbuf), ">> Level %d: %s\r\n", i,
		    passwds[i]);
		if (i <= pplayer->seclevel)
			sendtoplayer(pplayer, sendbuf);
	}
	snprintf(sendbuf, sizeof(sendbuf), ">> Shutdown: %s\r\n", P_SHUTDOWN);
	sendtoplayer(pplayer, sendbuf);
	snprintf(sendbuf, sizeof(sendbuf),
	    ">> Restore default commands et: %s\r\n", P_COMMANDS);
	sendtoplayer(pplayer, sendbuf);
	return 1;
}

parse_error
haven_shutdown(struct splayer *pplayer)
{
	snprintf(sendbuf, sizeof(sendbuf),
	    "Shutting down to shut down command by %s from %s.\n",
	    pplayer->name, pplayer->host);
	log_msg(sendbuf);
	snprintf(sendbuf, sizeof(sendbuf),
	    ">> Shutting down to shut down command by %s from %s.\r\n",
	    pplayer->name, pplayer->host);
	sendall(sendbuf, ALL, 0);
	ldb_close(&lorien_db);
	exit(0);
	return PARSE_OK; /* not reached */
}

int
changelevel(char *password, struct splayer *pplayer)
{
	char *buf;
	int level;

	/* check for returns and newlines */
	if ((buf = (char *)strchr(password, '\r')) != (char *)0) {
		*buf = '\000';
	}

	if ((buf = (char *)strchr(password, '\n')) != (char *)0) {
		*buf = '\000';
	}

	/* if there is an '=' it is a password change.  old password in password
    new password in buf.  '=' discarded. */

	if ((buf = (char *)strchr(password, '=')) != (char *)0) {
		*buf = '\000';
		buf++;
	}

	if (!strcmp(password, P_FLUSH_ERR)) {
		(void)fflush(stderr);
		level = pplayer->seclevel;
		return level;
	}

	if (!strcmp(password, P_SHUTDOWN)) {
		haven_shutdown(pplayer);
	}

	if (!strcmp(password, P_SHUTDOWN)) {
		restore_default_commands(pplayer);
	}
	/* compare to each successive password if not a command, or if changing.
    if not valid, then set level to -1.  this will kick them off. */

	for (level = JOEUSER; level < NUMLVL; level++)
		if (!strcmp(password, passwds[level]))
			break;

	if (level == NUMLVL)
		level = -1;

	/* if buf is neither null nor empty, then it is a password change.
    player must be at least level 4 for this.  */

	if ((buf != (char *)0) && (*buf != '\000')) {
		if (pplayer->seclevel < SUPREME)
			return -1;

		snprintf(sendbuf, sizeof(sendbuf),
		    "%s changed level %d passwd, %s, to %s.", pplayer->name,
		    level, passwds[level], buf);
		strncpy(passwds[level], buf, MAX_POWER_PASS);
		log_msg(sendbuf);
		snprintf(sendbuf, sizeof(sendbuf),
		    ">> Level %d password changed to %s.\r\n", level,
		    passwds[level]);
		sendtoplayer(pplayer, sendbuf);
		level = pplayer->seclevel;
	} else {
		snprintf(sendbuf, sizeof(sendbuf),
		    ">> Level changed to %d.\r\n", level);
		if (level != -1)
			sendtoplayer(pplayer, sendbuf);
	};

	if (*password != '\000') {
		if (level == -1) {
			snprintf(sendbuf, sizeof(sendbuf),
			    "failed passwd attempt -->%s<-- by %s", password,
			    pplayer->name);
			log_msg(sendbuf);
		}
	}
	return (level);
}

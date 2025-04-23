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

#include "log.h"
#include "platform.h"
#include "utility.h"

char *hi_types[] = { "None", "Bold", "Underline", "Blink", "Reverse", "ERROR!",
	"ERROR!", "ERROR!", "ERROR!" };

char *
skipspace(char *buf)
{
	while (isspace(*buf) && (*buf != (char)0))
		buf++;
	return buf;
}

char *
skipdigits(char *buf)
{
	while (isdigit(*buf) && (*buf != (char)0))
		buf++;
	return buf;
}

char *trimspace(char *buf, size_t sz) {
	char *p = skipspace(buf);
	char *e = &p[strnlen(p, sz) - 1];

	while (isspace(*e) && e >= p)
		*e-- = (char) 0;

	return p;
}

#ifdef NEED_STRERROR
char *
strerror(int num)
{
	static char buf[2048];
	(void)snprintf(buf, sizeof(buf), "Errno: %d\n", num);
	return buf;
}
#endif

#ifdef NEED_STRSTR
char *
strstr(char *s1, char *s2)
{
	char *tmp;

	do {
		tmp = strchr(s1, *s2);
		if (!tmp)
			break;
		if (!strncmp(tmp, s2, strlen(s2)))
			return tmp;
		tmp++;
	} while (!tmp);

	return (char *)0;
}
#endif

void
log_error(char *prefix, char *file, int lineno)
{
	int loc_errno = errno;
	char logbuf[1024];

	if (loc_errno != 0) {
		(void)snprintf(logbuf, sizeof(logbuf), "%s: %s[%s:%d]", prefix,
		    strerror(loc_errno), file, lineno);
		log_msg(logbuf);
	}
}

char *
expand_hilite(int mask, char *buffer)
{
	char *cp;
	int bit;
	char vtesc[] = "[";

	(void)strncpy(buffer, vtesc, sizeof(vtesc) - 1);
	buffer[sizeof(vtesc) - 1] = 0;
	cp = buffer + sizeof(vtesc) - 1;
	bit = 0;
	while (bit < 8) {
		if ((1 << bit) & mask) {
			if (cp != (buffer + 2)) {
				*cp = ';';
				cp++;
			}
			switch (bit) {
			case BOLD:
				*cp = '1';
				break;
			case UNDERLINE:
				*cp = '4';
				break;
			case BLINK:
				*cp = '5';
				break;
			case REVERSE:
				*cp = '7';
				break;
			default: {
				char errbuf[100];
				(void)snprintf(errbuf, sizeof(errbuf),
				    "Unknown bit in hilite mask 0x%x number %d",
				    mask, bit);
				log_msg(errbuf);
			}
				return (char *)0;
			}
			cp++;
		}
		bit++;
	}
	*cp = 'm';
	cp[1] = '\000';
	return buffer;
}

int
construct_mask(char *args, int *mask)
{
	int change = 0x0;

	while (isspace(*args) && *args)
		args++;
	if (!*args) {
		if (*mask)
			*mask = 0;
		else
			*mask = (1 << BOLD);
		return 0;
	}
	while (*args) {
		if (isspace(*args)) {
			args++;
			continue;
		}
		switch (*args) {
		default:
			return -1;
			/*NOTREACHED*/
			break;
		case 'B':
			change |= (1 << BLINK);
			break;
		case 'b':
			change |= (1 << BOLD);
			break;
		case 'r':
			change |= (1 << REVERSE);
			break;
		case 'u':
			change |= (1 << UNDERLINE);
			break;
		case '+':
			*mask |= change;
			change = 0x0;
			break;
		case '-':
			*mask &= ~change;
			change = 0x0;
			break;
		case '=':
			*mask = change;
			change = 0x0;
			break;
		}
		args++;
	}
	return 0;
}

char *
mask2string32(int mask, int validbits, char *buffer, size_t buflen,
    char **strings, char *separator)
{
	int bit;

	bit = 0;
	*buffer = '\000';
	if (validbits > 32)
		validbits = 32;

	if (mask) {
		while (bit <= validbits) {
			if ((1 << bit) & mask) {
				if (*buffer) {
					(void)strlcat(buffer, separator,
						      buflen);
				}
				(void)strlcat(buffer, strings[bit], buflen);
			}
			bit++;
		}
	} else {
		(void)strlcat(buffer, strings[0], buflen);
	}
	return buffer;
}

char *
mask2string(int mask, char *buffer, char **strings, char *separator)
{
	int bit;

	bit = 0;
	*buffer = '\000';
	if (mask)
		while (bit <= 8) {
			if ((1 << bit) & mask) {
				if (*buffer)
					(void)strcat(buffer, separator);
				(void)strcat(buffer, strings[bit]);
			}
			bit++;
		}
	else
		strcat(buffer, strings[0]);
	return buffer;
}

char *
timelet(time_t idle, long precision)
{
	static char buf[81];
	char *cp;
	long timenoc;
	time_t tmptime;
	int wks, days, hrs, mins, secs;

	(void)time(&tmptime);
	timenoc = tmptime - idle;
	secs = timenoc;
	mins = secs / 60;
	hrs = mins / 60;
	days = hrs / 24;
	wks = days / 7;
	secs %= 60;
	mins %= 60;
	hrs %= 24;
	days %= 7;
	/* if we ever need to count months....
    wks %= 4;
    */

	cp = buf;
	*buf = '\000';
	if (precision && wks) {
		(void)snprintf(cp, sizeof(buf), "%dw", wks);
		precision--;
		cp = (char *)strchr(buf, '\000');
	}
	if (precision && days) {
		(void)snprintf(cp, sizeof(buf) - strlen(buf), "%dd", days);
		precision--;
		cp = (char *)strchr(buf, '\000');
	}
	if (precision && hrs) {
		(void)snprintf(cp, sizeof(buf) - strlen(buf), "%dh", hrs);
		precision--;
		cp = (char *)strchr(buf, '\000');
	}
	if (precision && mins) {
		(void)snprintf(cp, sizeof(buf) - strlen(buf), "%dm", mins);
		precision--;
		cp = (char *)strchr(buf, '\000');
	}
	if (precision && secs) {
		(void)snprintf(cp, sizeof(buf) - strlen(buf), "%ds", secs);
		precision--;
		cp = (char *)strchr(buf, '\000');
	}

	return buf;
}


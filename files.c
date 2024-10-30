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

#include "newplayer.h"
#include "platform.h"

/* note that this getdtablesize function only works if
 getrlimit() is defined and can return the maximum
 number of fd's.  Otherwise, we set the hard limit
 at 8192. */

#if defined(NO_GETDTABLSIZE) || defined(_MSC_VER)
int
getdtablesize()
{
	int sz = 0;

#ifdef RLIMIT_NOFILE
	struct rlimit maxdescs;

	getrlimit(RLIMIT_NOFILE, &maxdescs);

	sz = (int)maxdescs.rlim_cur;
#elif defined(_MSC_VER)

	sz = _getmaxstdio();

#else

	FILE *array[8192] = {};
	int i;

	do {
		array[sz] = fopen("/dev/null", "r");
		sz++;
	} while (array[sz - 1]);

	for (i = 0; i < sz; i++)
		fclose(array[i]);

	sz += 3 - 1; /* for stdin, stdout and stderr */
		     /* minus the one we couldn't open */

#endif

	return sz;
}
#endif

int
setdtablesize(int newfdmax)
{
	if (newfdmax > FD_SETSIZE)
		newfdmax = FD_SETSIZE;

#ifdef RLIMIT_NOFILE
	struct rlimit maxdescs;
	getrlimit(RLIMIT_NOFILE, &maxdescs);
	maxdescs.rlim_cur = newfdmax;
	setrlimit(RLIMIT_NOFILE, &maxdescs);
#elif defined(_MSC_VER)
	_setmaxstdio(newfdmax);
#endif

	return getdtablesize();
}

int
gettablesize()
{
	return getdtablesize();
}

int
settablesize(size_t size)
{
	fd_set fds;
	int max;

	max = setfds(&fds);

	if (size < max)
		size = max;

	return setdtablesize(size);
}

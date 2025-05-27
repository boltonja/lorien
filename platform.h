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

#ifndef __PLATFORM_H__
#define __PLATFORM_H__ 1

#if defined(__APPLE__)
#include <arpa/inet.h>
#define htobe64(x) htonll(x)
#define htobe32(x) htonl(x)
#define htobe16(x) htons(x)
#define be64toh(x) ntohll(x)
#define be32toh(x) ntohl(x)
#define be16toh(x) ntohs(x)
#endif

#if defined(__FreeBSD__) || defined(__NetBSD__)
#include <sys/endian.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>

#ifdef __linux__
#include <bsd/bsd.h>
#endif

#include <ctype.h>
#if defined(__linux__) || defined(__illumos__) || defined(__OpenBSD__)
#include <endian.h>
#endif
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef _MSC_VER
#include <sys/resource.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <netinet/in.h>

#include <arpa/inet.h>
#include <err.h>
#include <netdb.h>
#include <sysexits.h>
#include <unistd.h>

#else
#include <io.h>
#include <winsock2.h>
#pragma comment(lib, "Ws2_32.lib")
#define alarm(a) /* */
#endif

#if !defined(__FreeBSD__) && !defined(__HAIKU__)
static inline void
err_set_file(void *fptr)
{
	FILE *buf = fptr;
	int fd = fileno(buf);
	int rc;

	if (fd != 2) {
		close(2);
		rc = dup2(fd, 2);
		if (rc != 2)
			err(EX_OSERR, "dup2() failed");
	}
}
#endif
#endif

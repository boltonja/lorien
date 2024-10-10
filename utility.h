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

#ifndef _UTILITY_H_
#define _UTILITY_H_
#define logerror(s) log_error(s, __FILE__, __LINE__)

#define BOLD	    1
#define UNDERLINE   2
#define BLINK	    3
#define REVERSE	    4

#define HI_BITS	    0x1E

static char *hi_types[] = { "None", "Bold", "Underline", "Blink", "Reverse",
	"ERROR!", "ERROR!", "ERROR!", "ERROR!" };

int construct_mask(char *args, int *mask);
char *expand_hilite(int mask, char *buffer);
time_t get_timestamp(void);
void log_error(char *prefix, char *file, int lineno);
char *mask2string32(int mask, int validbits, char *buffer, size_t buflen,
    char **strings, char *separator);
char *mask2string(int mask, char *buffer, char **strings, char *separator);
char *skipdigits(char *buf);
char *skipspace(char *buf);
char *timelet(long idle, long precision);

#endif

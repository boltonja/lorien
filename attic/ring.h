/*
 * Copyright 2008-2024 Bolton-Dormer Research Partnership
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

/* ring.h - routines to implement a ring buffer.
 * Jillian Alana Bolton
 *
 */

#ifndef _RING_H_
#define _RING_H_

#include <stdio.h>

enum ring_params {
	/*  RING_MAX=20, */ /* max number of items in the ring (now dynamic) */
	RING_START = 0,	    /* for retrieving the first entry in ring */
	RING_END = -1,	    /* when returning the last item in ring */
	RING_BOUNDS =
	    -2, /* when target is greater than min(ringlen, RING_MAX) */
		/*  RING_MAXPAYLOAD=OBUFSIZE (now dynamic) */
};

struct ring_buffer {
	size_t ringidx;		 /* index of the current item */
	size_t ringlen;		 /* how many items currently in the ring */
	size_t ringmax;		 /* how many payload items max */
	size_t ringsiz;		 /* the size of each payload item */
	unsigned char payload[]; /* the first byte after struct */
};

void ring_copy(struct ring_buffer * /* to */, struct ring_buffer * /* from */);

/* returns the size in bytes of the ring and its contents */
size_t ring_compute_size(size_t /* numlinks */, size_t /* linksize */);

/* returns the number of items currently in the ring */
size_t ring_getlen(struct ring_buffer *);

size_t ring_getentrysize(struct ring_buffer *);

struct ring_buffer *ring_new(size_t /* numlinks */, size_t /* linksize */);

void ring_destroy(struct ring_buffer * /* ring */);

void ring_init(struct ring_buffer * /* ring */, size_t /* numlinks */,
    size_t /* linksize */);

void ring_add(struct ring_buffer * /* ring */, void * /* data */,
    size_t /* len */);

int ring_get(struct ring_buffer * /* ring */, void * /* data */,
    size_t /* bufmax */, size_t /* entry */);
#endif

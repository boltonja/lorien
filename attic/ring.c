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

/* ring.c - routines to implement a ring buffer.
 * Jillian Alana Bolton
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lorien.h"

/* todo:  ring_delete() */

size_t
ring_compute_size(size_t numlinks, size_t linksize)
{
	/* bug:  doesn't detect overflow */
	return (sizeof(struct ring_buffer) + (numlinks * linksize));
}

void
ring_copy(struct ring_buffer *to, struct ring_buffer *from)
{
	/* not implemented */
	abort();
}

size_t
ring_getlen(struct ring_buffer *ring)
{
	return ring ? ring->ringlen : 0;
}

size_t
ring_getentrysize(struct ring_buffer *ring)
{
	return ring->ringsiz;
}

struct ring_buffer *
ring_new(size_t numlinks, size_t linksize)
{
	struct ring_buffer *ring;

	ring = malloc(ring_compute_size(numlinks, linksize));

	if (ring)
		ring_init(ring, numlinks, linksize);

	return ring;
}

void
ring_destroy(struct ring_buffer *ring)
{
	if (ring)
		free(ring);
}

void
ring_init(struct ring_buffer *ring, size_t numlinks, size_t linksize)
{
	if (ring) {
		memset(ring, 0, ring_compute_size(numlinks, linksize));
		ring->ringmax = numlinks;
		ring->ringsiz = linksize;
	}
}

void
ring_add(struct ring_buffer *ring, void *data, size_t len)
{
	if (ring) {
		if (ring->ringlen < ring->ringmax)
			ring->ringlen++;

		memcpy(&ring->payload[ring->ringidx * ring->ringsiz], data,
		    (len > ring->ringsiz) ? ring->ringsiz : len);
		ring->ringidx = (ring->ringidx + 1) % ring->ringmax;
	}
}

/* call with desired entry number.
 * returns RING_END or next valid entry number
 */

int
ring_get(struct ring_buffer *ring, void *data, size_t bufmax, size_t entry)
{
	int target;

	if (ring) {
		if (entry >= ring->ringlen)
			return RING_BOUNDS;
		target = (ring->ringidx + ring->ringmax - ring->ringlen +
			     entry) %
		    ring->ringmax;

		memcpy(data, &ring->payload[target * ring->ringsiz],
		    (bufmax > ring->ringsiz) ? ring->ringsiz : bufmax);

		return ((entry + 1) < ring->ringlen) ? entry + 1 : RING_END;
	} else
		return RING_BOUNDS;
}

#ifdef TESTRING
int
main()
{
	struct ring_buffer *r;
	int i, j, rc;
	char buf[OBUFSIZE];
	char buf2[OBUFSIZE];

	r = ring_new(20, OBUFSIZE);

	if (r) {
		for (i = 0; i < 20; i++) {
			snprintf(buf, sizeof(buf), "%d\r\n", i);
			ring_add(r, buf, strlen(buf));
			memset(buf2, 0, sizeof(buf2));
			rc = ring_get(r, buf2, OBUFSIZE, i);
			if (rc > 0) {
				j = atoi(buf2);
				if (j != i)
					printf(
					    "build entry %d should be %d but is %d\n",
					    i, i, j);
			} else if (rc != RING_END)
				printf(
				    "build rc for %d should be RING_END but is %d\n",
				    i, rc);
		}

		for (i = 0; i < (r->ringmax); i++) {
			memset(buf2, 0, sizeof(buf2));
			rc = ring_get(r, buf2, OBUFSIZE, i);
			if ((rc > 0) || rc == RING_END) {
				j = atoi(buf2);
				if (j != i)
					printf(
					    "traverse entry %d should be %d but is %d\n",
					    i, i, j);
				else
					printf("\tOK:  entry %d is %d\n", i, j);
				if (rc != RING_END && (i + 1) == r->ringmax)
					printf(
					    "traverse entry %d should be end but RING_END not issued\n",
					    i);
			} else if ((rc == RING_END) && ((i + 1) == r->ringmax))
				printf("end found correctly\n");
			else
				printf("traverse entry %d gives error %d\n", i,
				    rc);
		}

		for (i = 0; i != RING_END; i = rc) {
			memset(buf2, 0, sizeof(buf2));
			rc = ring_get(r, buf2, OBUFSIZE, i);
			if ((rc > 0) || (rc == RING_END)) {
				j = atoi(buf2);
				if (j != i)
					printf(
					    "traverse 2 entry %d should be %d but is %d\n",
					    i, i, j);
				else
					printf("\tOK:  entry %d is %d\n", i, j);
				if (rc != RING_END && (i + 1) == r->ringmax)
					printf(
					    "traverse entry %d should be end but RING_END not issued\n",
					    i);
			} else if ((rc == RING_END) && ((i + 1) == r->ringmax))
				printf("end found correctly\n");
			else
				printf("traverse 2 entry %d gives error %d\n",
				    i, rc);
		}
		ring_destroy(r);
	} else
		printf("couldn't alloate ring buffer\n");
}
#endif

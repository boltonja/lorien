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

/* newpass.c */
/* new password routines for Lorien --- in-memory package.

   AUTHOR:  Creel (Jillian Alana Bolton)

   This is the supported password package for Lorien.
   It offers management of optional name protection via a dynamically
   allocated database.

   The database is dumped when a password is enabled, changed or deleted.

   The program reads the database file only once, at startup.

   These routines should work on any system whose stream i/o functions
   work properly.  The following stream i/o functions are used:

   fwrite() - fwrite is expected to write.  no error checking is performed.
   fopen()  - fopen is expected to return NULL if an error occurs.
   fread()  - fread is expected to return 0 if an error occurs.
   fclose() - fclose is expected to close.  no error checking is performed.

   The following dynamic memory functions are used:

   malloc()
   free()

   The above functions are expected to behave according to the POSIX standard.

   The following string/memory functions are used:

   memset()
   strcpy()
   strcmp()
   strchr()

   The above functions are expected to behave according the POSIX standard.

   The routines, as you can see, place minimal demands on the system
   libraries.  This should eliminate the infinite loop bug from early versions
   using feof().  It should also eliminate some strange behavior under
   SunOS which occurred with the recent fread() version.


*/

#include "lorien.h"
#include "platform.h"
#include "prefs.h"

struct item {
	struct p_word p;
	struct item *next;
	struct item *prev;
};

FILE *PFILE;

struct item *itemhead = NULL;
struct item *itemtail = NULL;

long
itemdump()
{
	struct item *i = itemhead;

	while (i) {
		printf("name: %s  password: %s\n", i->p.name, i->p.pword);
		i = i->next;
	};

	return 1;
}

long
item_add(struct p_word *p)
{
	struct item *i;

	i = (struct item *)malloc(sizeof(struct item));

	if (!i)
		return 0;

	strcpy(i->p.pword, p->pword);
	strcpy(i->p.name, p->name);

	if (itemhead == NULL) {
		itemhead = itemtail = i;
		i->prev = i->next = NULL;
	} else {
		itemtail->next = i;
		i->prev = itemtail;
		i->next = NULL;
		itemtail = i;
	}

	return 1;
}

struct item *
item_find(struct p_word *p)
{
	struct item *i = itemhead;

	if (!itemhead)
		return NULL;

	do {
		if (!strcmp(i->p.name, p->name))
			return (i);
		i = i->next;
	} while (i);

	return NULL;
}

long
item_delete(struct p_word *p)
{
	struct item *i;
	int ishead;
	int istail;

	i = item_find(p);

	if (!i)
		return 0;

	ishead = (i == itemhead);
	istail = (i == itemtail);

	if (ishead && istail) {
		itemhead = itemtail = NULL;
	} else if (ishead) {
		i->next->prev = NULL;
		itemhead = i->next;
	} else if (istail) {
		i->prev->next = NULL;
		itemtail = i->prev;
	} else {
		i->prev->next = i->next;
		i->next->prev = i->prev;
	}

	free(i);

	return 1;
}

long
item_collapse()
{
	struct item *curr;

	curr = itemhead;

	if (itemhead == NULL || itemtail == NULL)
		return 0;

	itemhead = itemtail = NULL;

	while (curr->next) {
		curr = curr->next;
		free(curr->prev);
	};

	return 1;
}

long
passwrite()
{
	struct item *i = itemhead;

	if (!itemhead)
		return (1);

	//   abort();
	PFILE = fopen(PASSFILE, "wb");

	if (!PFILE)
		return (0);

	do {
		fwrite(&i->p, sizeof(struct p_word), 1, PFILE);
		i = i->next;
	} while (i);

	fflush(PFILE);
	fclose(PFILE);

	return (1);
}

long
passread()
{
	struct p_word p;

	//   abort();
	PFILE = fopen(PASSFILE, "rb");

	if (!PFILE)
		return 0;

	item_collapse();

	while (fread(&p, sizeof(struct p_word), 1, PFILE))
		item_add(&p);

	fclose(PFILE);

	return 1;
}

/* pfree
	Free a record.

     Frees a record whose name is specified, allowing the record to be
     recycled.

     IN:      name -     the name of the record to be freed.
     OUT:     ---
     RETURNS: non-zero if record was found.

     AUTHOR: Jillian Alana Bolton (Creel)
*/

// BUG: bounding these arrays probably provides no additional safety,
//      consider passing or using the lengths (strlcpy, strncpy)
long
pfree(char name[LORIEN_V0173_NAME])
{
	struct p_word p;

	memset(p.name, 0, sizeof(p.name));
	strcpy(p.name, name);

	return (item_delete(&p));
};

/* padd
	Add a record.  No checking is done to see if a record already exists.
	This should be done by the caller via plookup()

	IN:       name -     the name to add.
		  pword -    the associated pword.
	OUT:      ---
	RETURNS:  ---

	AUTHOR:  Jillian Alana Bolton

*/

// BUG: bounding these arrays probably provides no additional safety,
//      consider passing or using the lengths (strlcpy, strncpy)
void
padd(char name[LORIEN_V0173_NAME], char pword[LORIEN_V0173_PASS])
{
	struct p_word p;

	memset(p.pword, 0, sizeof(p.pword));
	memset(p.name, 0, sizeof(p.name));

	strcpy(p.pword, pword);
	strcpy(p.name, name);

	item_add(&p);
};

/* plookup
	Lookup a password.

	IN:       p -      a password structure containing the name
			   of the player to lookup.  (passed by reference)
	OUT:      p -      a password structure containing the name and
			   password of a player.
	 RETURNS: non-zero if found.  zero if not found.

	 AUTHOR: Jillian Alana Bolton (Creel)

*/

int
plookup(struct p_word *p)
{
	struct item *i;

	i = item_find(p);

	if (!i)
		return 0;

	strcpy(p->pword, i->p.pword);
	return (1);
};

/* changepass
	Change the password of a player.

	IN:        name -     the name of the player to change.
		   pword -    the old password.
		   newp -     the new password.
	OUT:       ---
	RETURNS:   -1 if the player was not found.
		    0 if password change was successful.
		    1 if player was found but incorrect password.

       AUTHOR: Jillian Alana Bolton (Creel)

*/

// BUG: use the lengths, i.e. strncpy() or strlcpy()
int
changepass(char name[LORIEN_V0173_NAME], char pword[LORIEN_V0173_PASS],
    char newp[LORIEN_V0173_PASS])
{
	struct p_word p;
	int status;

	strcpy(p.name, name);

	status = plookup(&p);
	if (!status)
		return (-1); /* code for not found */
	if (strcmp(p.pword, pword))
		return (1); /* code for invalid pword */
	pfree(name);
	padd(name, newp);
	passwrite();
	return (0); /* code for ok. */
};

/* enablepass
	 Enables a password.

	 IN:      A buffer containing the name and password.
		  delimited by '='.
	 OUT:     ---
	 RETURNS: non-zero if successful.

	 AUTHOR:  Jillian Alana Bolton

*/

int
enablepass(char *buf)
{
	char *buf2;
	char *buf3;
	struct p_word p;

	buf2 = buf;
	buf3 = strchr(buf2, '=');
	if (buf3 == (char *)0)
		return (0);
	buf3[0] = 0x0;
	buf3++;
	strcpy(p.name, buf2);
	if (plookup(&p))
		return (!changepass(p.name, p.pword, buf3));
	padd(buf2, buf3);
	passwrite();
	return (1);
};

/* newpass
	Changes a password.

	IN:      A buffer containing the name, old pass and new pass
		 delimited by '='.
	OUT:     ---
	RETURNS: non-zero if successful.

	AUTHOR:  Jillian Alana Bolton

*/

int
newpass(char *name, char *buf)
{
	char *buf2;

	if ((buf2 = strchr(buf, '='))) {
		buf2[0] = 0x0;
		buf2++;
		return (!changepass(name, buf, buf2));
	}
	return 0;
};

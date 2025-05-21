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

#include "lorien.h"
#include "parse.h"
#include "platform.h"

extern struct splayer *players;
extern chan *channels;
extern int numconnect;

struct servsock_handle;

chan *findchannel(char *name);
void handleinput(fd_set needread);
char *idlet(time_t idle);
void initplayerstruct(void);
int isgagged(struct splayer *recipient, struct splayer *sender);
struct splayer *lookup(int linenum);
chan *newchannel(char *name);
int newplayer(struct servsock_handle *ssh);
chan *new_channelp(char *channel);
int numconnected();
void playerinit(struct splayer *who, time_t when, char *where, char *numwhere);
void processinput(struct splayer *pplayer);
int recvfromplayer(struct splayer *who);
void remove_channel(chan *channel);
void removeplayer(struct splayer *player);
void sendall(char *message, chan *channel, struct splayer *who);
int sendtoplayer(struct splayer *who, char *message);
int setfds(fd_set *needread);
int setname(struct splayer *pplayer, char *name); // BUG:set_name() vs
						  // setname()?
int welcomeplayer(struct splayer *pplayer);
parse_error wholist(struct splayer *pplayer, char *instring);
parse_error wholist2(struct splayer *pplayer, char *instring);
parse_error wholist3(struct splayer *pplayer);
int player_getline(struct splayer *pplayer);

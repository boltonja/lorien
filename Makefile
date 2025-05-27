#
# Copyright 1990-1996 Chris Eleveld
# Copyright 1992 Robert Slaven
# Copyright 1992-2025 Jillian Alana Bolton
# Copyright 1992-1995 David P. Mott
#
# The BSD 2-Clause License
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     1. Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
#
#     2. Redistributions in binary form must reproduce the above
#     copyright notice, this list of conditions and the following
#     disclaimer in the documentation and/or other materials provided
#     with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
# BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
# OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
# IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
#
# This is the makefile for Lorien
#
# There are some features of the code that can be disabled / enabled
# with defines.  These can be edited in the OPTS line, below.
#
#
# DEFINE	       TO GET THIS
# --------------       ----------------------------------------------------
# DEFAULT_NAME         Picks a default name for new arrivals.
# DEFCHAN              Picks a name for the main channel.  Max 13 chars.
# ONFROM_ANY           If defined, anyone can change host info.
# USE_CONFIG_H         If you can't use DEFAULT_NAME and DEFCHAN in the
#                      makefile because your shell doesn't allow it, define
#                      this and edit config.h to your liking.
# SKIP_HOSTLOOKUP      Skips the host lookups when a player connects.
#
# EDIT THIS LINE FOR OPTIONS.
#
OPTS= -DUSE_CONFIG_H -DONFROM_ANY
#
# Don't mess with these unless you have good reason to.
# Keep reading however because the LIBS and DEFS may need to be changed below.
#

DOC=LICENSE README CHANGELOG aprilfools.commands lorien.commands lorien.help lorien.welcome tfsurvival-3p0.txt sockets.doc

MAK=.clang-format CMakeLists.txt Makefile

HDR= ban.h board.h channel.h chat.h commands.h config.h db.h files.h help.h log.h lorien.h msg.h newplayer.h parse.h platform.h security.h servsock_ssl.h trie.h utility.h

SRC= ban.c board.c channel.c chat.c commands.c db.c files.c help.c dbtool.c  log.c lorien.c msg.c newplayer.c parse.c security.c servsock_ssl.c trie.c utility.c

MAIN= lorien.o

OBJ= ban.o board.o channel.o chat.o commands.o db.o files.o help.o log.o msg.o newplayer.o parse.o security.o servsock_ssl.o trie.o utility.o

# Illumos (e.g., OpenIndiana) needs additionally: -lnsl -lsocket
LIBS?=-lc -L /usr/local/lib -llmdb -lcrypt -lssl -lcrypto -liconv
CFLAGS?=-g -ggdb -fstack-protector-all

CC?=gcc
DEBUG=-g -ggdb
FLAGS?=$(DEBUG) $(CFLAGS) $(OPTS) -fstack-protector-all -Wall -I/usr/local/include
BINARY=lorien
TARGETS=testtrie testhelp testboard testmsg $(BINARY) dbtool

default:
	make $$(uname -s | awk -F- '{print $$1}')

all: $(TARGETS)

FreeBSD:
	make -f Makefile LIBS="-lc -L /usr/local/lib -llmdb -lcrypt -lssl -lcrypto -liconv" all

OpenBSD:
	make -f Makefile LIBS="-L/usr/local/lib -lc -llmdb -lcrypto -lssl -liconv" all

NetBSD:
	export LD_LIBRARY_PATH=/usr/pkg/lib:/lib:/usr/lib:/usr/local/lib
	make CFLAGS="-I/usr/pkg/include" LIBS="-L/usr/pkg/lib -lc -llmdb -lssl -lcrypto" all

Linux:
	make -k LIBS="-lc -L /usr/local/lib -llmdb -lcrypt -lbsd -lssl -lcrypto" all

Haiku:
	make FLAGS="$(DEBUG) $(OPTS) -Wall -I/boot/home/libiconv-1.17/include -I/boot/home/lmdb/libraries/liblmdb" LIBS="-L/boot/system/lib -llmdb -lcrypto -lnetwork -lssl -lbsd /boot/system/lib/libiconv.so.2" all

SunOS:
	make -k CC=gcc CFLAGS="-g -ggdb -O0 -fstack-protector-all -I/usr/local/include" LIBS="-L/usr/local/lib/64 -L/usr/local/lib -lc -llmdb -lnsl -lsocket -lssl -lcrypto" CFLAGS="-D_POSIX_PTHREAD_SEMANTICS" all

Darwin:
	make LIBS="-L/opt/homebrew/lib -lc -llmdb -lssl -lcrypto -liconv" CFLAGS="-I/opt/homebrew/include" all

MINGW64_NT:
	make CFLAGS="-I/mingw64/include" LIBS="-L/mingw64/lib -llmdb -lssl -lcrypto -liconv" all

server: $(BINARY)

clean:
	rm -f $(OBJ) lorien.log maxconn.h core $(TARGETS) $(MAIN)

lint:
	lint $(SRC) $(HDR)

$(BINARY): $(OBJ) $(MAIN)
	$(CC) $(FLAGS) -o $(BINARY) $(OBJ) $(LIBS) $(MAIN)

testhelp: help.c $(OBJ)
	$(CC) -DTESTHELP $(DEBUG) $(FLAGS) -o testhelp help.c $(LIBS)

testtrie: trie.c trie.h $(OBJ)
	$(CC) -DTESTTRIE $(DEBUG) $(FLAGS) -o testtrie trie.c $(LIBS)

testboard: board.c board.h db.o $(OBJ)
	$(CC) -DTESTBOARD $(DEBUG) $(FLAGS) -o testboard board.c db.o log.o $(LIBS)

testmsg: msg.c msg.h board.h db.o board.o trie.o $(OBJ)
	$(CC) -DTESTMSG $(DEBUG) $(FLAGS) -o testmsg msg.c board.o db.o log.o trie.o $(LIBS)

dbtool: db.h lorien.h dbtool.c $(OBJ)
	$(CC) $(DEBUG) $(FLAGS) -o dbtool dbtool.c $(OBJ) $(LIBS)

$(OBJ):$(P) Makefile

.SUFFIXES: .c.o

.c.o: $(HDR)
	$(CC) -c $(DEBUG) $(FLAGS) $*.c

secure:
	chmod 644 $(SRC) $(HDR) $(DOC) $(MAK)
	@chmod 755 $(TARGETS)
	@chmod 644 $(OBJ)

shar:
	shar $(DOC) $(MAK) $(SRC) $(HDR) > $(BINARY).shar

tar:
	tar cf $(BINARY).tar $(DOC) $(MAK) $(SRC) $(HDR)

#
# Copyright 1990-1996 Chris Eleveld
# Copyright 1992 Robert Slaven
# Copyright 1992-2024 Jillian Alana Bolton
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
# CHECK_CHANNELS       If it core dumps in the channel code, define this.
# FIXCHANNELS          If it core dumps in the channel code, define this.
# LOG                  By default, logging is turned on.
# NO_LOG_CONNECT       By default, arrivals and departures are not logged.
# NO_NUMBER_CHANNELS   To disallow numeric channel names.  Default: allowed.
# ONFROM_ANY           If defined, anyone can change host info.
# PASSWORDS_ENABLED    This is enabled by default.  For name protection.
# USE_SHUTDOWN_COMMAND Enables use of .shutdown by level 3+.  Default: on.
# USE_CONFIG_H         If you can't use DEFAULT_NAME and DEFCHAN in the
#                      makefile because your shell doesn't allow it, define
#                      this and edit config.h to your liking. 
# SKIP_HOSTLOOKUP      Skips the host lookups when a player connects.
# ANTISPAM             Adds detection and logging of attempts to relay spam.
# USE_32_BIT_TIME_T    The time_t data type is 32 bits on this system
# USE_64_BIT_TIME_T    The time_t data type is 64 bits on this system
# (note:  you must define one of USE_32_BIT_TIME_T or USE_64_BIT_TIME_T, 
#         and it must be correct for prefdb compatibility!!!)
#
# EDIT THIS LINE FOR OPTIONS. YOU MUST ALWAYS ENABLE -DLORIEN
#
OPTS= -DLORIEN -DPASSWORDS_ENABLED -DLOG -DNO_LOG_CONNECT -DUSE_SHUTDOWN_COMMAND -DUSE_CONFIG_H  -DONFROM_ANY -DANTISPAM 
#
# Don't mess with these unless you have good reason to.
# Keep reading however because the LIBS and DEFS may need to be changed below.
#

DOC=LICENSE README CHANGELOG aprilfools.commands lorien.commands lorien.help lorien.welcome tfsurvival-3p0.txt sockets.doc

SECRETS=lorien.db lorien.pass lorien.power

MAK=.clang-format CMakeLists.txt Makefile

HDR= ban.h chat.h commands.h config.h files.h help.h journal.h log.h lorien.h newpass.h newplayer.h parse.h platform.h prefs.h ring.h security.h servsock.h trie.h utility.h

SRC= ban.c chat.c commands.c files.c help.c journal.c log.c lorien.c newpass.c newplayer.c parse.c prefs.c ring.c security.c servsock.c trie.c utility.c

OBJ= ban.o chat.o commands.o files.o help.o journal.o log.o lorien.o newpass.o newplayer.o parse.o prefs.o ring.o security.o servsock.o trie.o utility.o

# Illumos (e.g., OpenIndiana) needs additionally: -lnsl -lsocket
LIBS?=-lc
DEFS=-DPOSIX -DUSE_32_BIT_TIME_T

CC?=gcc
DEBUG=-g -ggdb
FLAGS=$(DEBUG) $(DEFS) $(OPTS) -fstack-protector-all -Wall
BINARY=lorien
TARGETS=testring testtrie testhelp testiconv convprefs $(BINARY)
TARGETS=testring testtrie testhelp convprefs $(BINARY)
#
#	for parallelisms:
#
P=

default: $(TARGETS)

all: $(TARGETS)

server: $(BINARY)

clean:
	rm -f $(OBJ) lorien.log getmax maxconn.h core $(TARGETS)

lint:
	lint $(SRC) $(HDR) $(LIB)

getmax: 
	$(CC) $(FLAGS) -o getmax getmax.c $(LIBS)
	./getmax | sed 's/.*= \([1-90].*\)\./\#define MAXCONN \1/' > maxconn.h

$(BINARY): $(P) $(OBJ) 
	rm -f getmax maxconn.h
	$(CC) $(FLAGS) -o $(BINARY) $(OBJ) $(LIBS)

testhelp: help.c 
	$(CC) -DTESTHELP $(DEBUG) $(FLAGS) -o testhelp help.c $(LIBS)

testring: ring.c ring.h
	$(CC) -DTESTRING $(DEBUG) $(FLAGS) -o testring ring.c $(LIBS)

testtrie: trie.c trie.h
	$(CC) -DTESTTRIE $(DEBUG) $(FLAGS) -o testtrie trie.c $(LIBS)

testiconv: testiconv.c rfc2066.h
	$(CC) -DTESTICONV $(DEBUG) $(FLAGS) -o testiconv testiconv.c $(LIBS) $(ICONVLIB)

convprefs: prefs.c prefs.h trie.o trie.h
	$(CC) -DCONVERT $(DEBUG) $(FLAGS) trie.o -o convprefs prefs.c $(LIBS) 

$(OBJ):$(P) Makefile

.SUFFIXES: .c.o

.c.o: $(HDR)
	$(CC) -c $(DEBUG) $(FLAGS) $*.c

secure:
	touch $(SECRETS)
	chmod 600 $(SECRETS)
	chmod 644 $(SRC) $(HDR) $(DOC) $(MAK)
	@chmod 755 $(TARGETS)
	@chmod 644 $(OBJ) 

shar:
	shar $(DOC) $(MAK) $(SRC) $(HDR) > $(BINARY).shar

tar:
	tar cf $(BINARY).tar $(DOC) $(MAK) $(SRC) $(HDR)

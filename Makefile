# dmklib
# Makefile
# $Id: Makefile,v 1.5 2002/10/19 23:49:31 eric Exp $
# Copyright 2002 Eric Smith <eric@brouhaha.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.  Note that permission is
# not granted to redistribute this program under the terms of any
# other version of the General Public License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111  USA

# -----------------------------------------------------------------------------
# options
# -----------------------------------------------------------------------------

CFLAGS = -g -Wall $(DEFINES)
LDFLAGS = -g


# -----------------------------------------------------------------------------
# You shouldn't have to change anything below this point, but if you do please
# let me know why so I can improve this Makefile.
# -----------------------------------------------------------------------------

PACKAGE = dmklib
VERSION = 0.3
DISTNAME = $(PACKAGE)-$(VERSION)

DATE := $(shell date +%Y.%m.%d)
SNAPNAME = $(PACKAGE)-$(DATE)

TARGETS = libdmk.o rfloppy dmkformat dmk2raw dumpids

HEADERS = libdmk.h dmk.h

SOURCES = libdmk.c rfloppy.c dmkformat.c dmk2raw.c dumpids.c

DEFINES =

OTHERSRC = Makefile
MISC = COPYING README

OBJECTS = $(SOURCES:.c=.o)
DEPENDS = $(SOURCES:.c=.d)

DISTFILES = $(MISC) $(OTHERSRC) $(HEADERS) $(SOURCES)


all: $(TARGETS) $(MISC_TARGETS)


# -----------------------------------------------------------------------------
# Artificial targets.
# -----------------------------------------------------------------------------

dist: $(DISTNAME).tar.gz

snap: $(SNAPNAME).tar.gz

%.tar.gz: $(DISTFILES)
	-rm -rf $@ $*
	mkdir $*
	for f in $(DISTFILES); do ln $$f $*/$$f; done
	tar --gzip -chf $@ $*
	-rm -rf $*


clean:
	rm -f $(TARGETS) $(MISC_TARGETS) $(OBJECTS) $(DEPENDS)


# -----------------------------------------------------------------------------
# Real targets.
# -----------------------------------------------------------------------------

rfloppy: rfloppy.o libdmk.o

dmkformat: dmkformat.o libdmk.o

dmk2raw: dmk2raw.o libdmk.o

dumpids: dumpids.o


# -----------------------------------------------------------------------------
# Automatically generate dependencies.
# -----------------------------------------------------------------------------

%.d: %.c
	$(CC) -M -MG $(CFLAGS) $< | sed -e 's@ /[^ ]*@@g' -e 's@^\(.*\)\.o:@\1.d \1.o:@' > $@

include $(DEPENDS)



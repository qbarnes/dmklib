# Not a very good Makefile.

CFLAGS=-g -Wall
LDFLAGS=-g # -ldsk

all: rfloppy dmkformat dmk2raw libdmk.o
all2: dumpids r6085 rcoco

rfloppy: rfloppy.o libdmk.o

dmkformat: dmkformat.o libdmk.o

dmk2raw: dmk2raw.o libdmk.o

libdmk.o: libdmk.c libdmk.h dmk.h

r6085: r6085.o

rcoco: rcoco.o

dumpids: dumpids.o

foo: foo.o

foo2: foo2.o


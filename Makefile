CFLAGS=-g -Wall
LDFLAGS=-g # -ldsk

all: rfloppy r6085 rcoco dumpids

rfloppy: rfloppy.o

r6085: r6085.o

rcoco: rcoco.o

dumpids: dumpids.o

foo: foo.o

foo2: foo2.o


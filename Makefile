CFLAGS=-g -Wall
LDFLAGS=-g # -ldsk

all: r6085 dumpids

r6085: r6085.o

dumpids: dumpids.o

foo: foo.o

foo2: foo2.o


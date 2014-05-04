CC=gcc
CFLAGS=-Wall -g -DRAM_DICT -DBOOTSTRAP -DPC_SIM -DINLINE=""
LDFLAGS= -g -lm

SRCS=	uforth.c  uforth-ext.c  utils.c
OBJS= $(SRCS:.c=.o)
uforth-pc: $(OBJS) uforth-pc.o ext.f
	gcc $(CFLAGS) -o uforth-pc $(OBJS) uforth-pc.o

ext.f: uforth-ext.h
	awk -f make_ext_words.awk uforth-ext.h > ext.f

depend:
	makedepend -- $(CFLAGS) -- $(SRCS)

clean:
	-rm -f *.o *.exe *~ *.stackdump *.aft-TOC uforth-pc

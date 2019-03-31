#distribution version of makefile for *.113  iomega backup programs  
#in NH 5/13/17 had to add -lrt to most of executables so clock_gettime() could be found

CC?=gcc
CFLAGS = -g -Wall
CFLAGSEXTRA = -ggdb -ansi -pedantic -Werror -W -Wmissing-prototypes \
			  -Wstrict-prototypes -Wconversion -Wshadow -Wpointer-arith \
			  -Wcast-qual -Wcast-align -Wwrite-strings -Wnested-externs \
			  -fshort-enums -fno-common -Dinline= -g -ggdb

.SUFFIXES:
.SUFFIXES: .c .o

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<


prefix = /usr/local
exec_prefix = $(prefix)

PROGRAMS := vtbl64

all: $(PROGRAMS)

PROG = vtbl64
$(PROG): main.o qic122.o qic113.o bitsbytes.o
	$(CC) -g -o $@ $^

$main.o: main.c qic.h
	$(CC) $(CFLAGS) $(CFLAGSEXTRA) -o $@ -c $<

qic122.o: qic122.c qic.h
	$(CC) $(CFLAGS) $(CFLAGSEXTRA) -o $@ -c $<

qic113.o: qic113.c qic.h
	$(CC) $(CFLAGS) $(CFLAGSEXTRA) -o $@ -c $<

bitsbytes.o: bitsbytes.c qic.h
	$(CC) $(CFLAGS) $(CFLAGSEXTRA) -o $@ -c $<

.PHONY: clean

clean:
	$(RM) -f *.o $(PROGRAMS)

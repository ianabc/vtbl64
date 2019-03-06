#distribution version of makefile for *.113  iomega backup programs  
#in NH 5/13/17 had to add -lrt to most of executables so clock_gettime() could be found

CC = gcc
CFLAGS = -g -Wall -Dunix
CFLAGSEXTRA = -ggdb -ansi -pedantic
LDFLAGS = -lrt -g

.SUFFIXES:
.SUFFIXES: .c .o

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<


prefix = /usr/local
exec_prefix = $(prefix)

PROGRAMS := rd113 iocfg slib vtbl vtbl64

all: $(PROGRAMS)

PROG=rd113
$(PROG): $(PROG).o
	gcc -g -o $@ -g -lrt $^

PROG = iocfg
$(PROG): $(PROG).o
	gcc -g -o $@ -g -lrt $^

PROG = slib
$(PROG): $(PROG).o
	gcc -g -o $@ -g -lrt $^

PROG = vtbl
$(PROG): $(PROG).o qicdcomp.o
	gcc  -g -o $@ $^

PROG = vtbl64
$(PROG): $(PROG).o
	gcc  -g -o $@ $^

$(PROG).o: $(PROG).c $(PROG).h
	$(CC) $(CFLAGS) $(CFLAGSEXTRA) -o $@ -c $<

.PHONY: clean

clean:
	$(RM) -f *.o $(PROGRAMS)

#distribution version of makefile for *.113  iomega backup programs  
#in NH 5/13/17 had to add -lrt to most of executables so clock_gettime() could be found

CC = gcc
CFLAGS = -g -Wall
LDFLAGS = -lrt -g

.SUFFIXES:
.SUFFIXES: .c .o

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<


prefix = /usr/local
exec_prefix = $(prefix)

PROGRAMS := rd113 iocfg slib

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

.PHONY: clean

clean:
	$(RM) *.o -f $(obj) rd113 iocfg slib

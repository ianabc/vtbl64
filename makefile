#distribution version of makefile for *.113  iomega backup programs  
#in NH 5/13/17 had to add -lrt to most of executables so clock_gettime() could be found

all: /usr/local/bin/rd113  /usr/local/bin/iocfg  /usr/local/bin/slib

clean:
	rm *.o
	rm /usr/local/bin/rd113
	rm /usr/local/bin/iocfg
	rm /usr/local/bin/slib


/usr/local/bin/rd113: rd113.o
	gcc -o $@ -g -lrt rd113.o

rd113.o: rd113.c 
	gcc -c -g  rd113.c

/usr/local/bin/iocfg: iocfg.o
	gcc -o $@ -g -lrt iocfg.o

iocfg.o: iocfg.c 
	gcc -c -g  iocfg.c

/usr/local/bin/slib: slib.o
	gcc -o $@ -g  slib.o

slib.o: slib.c 
	gcc -c -g  slib.c


# MSDOS makefile for rd113 and related iocfg and slib with Open Watcom
#copied from iomega/1Srest.wat 1/23/17 and modified for correct source file dependancy

#for Watcom  large memory model MSDOS console application 
#    [I think -bt=dos forces define of MSDOS so -dMSDOS redundant]
CC = wcc
CFLAGS = -bc -bt=dos -ml -d2 -dMSDOS
LINKER = wcl
LFLAGS = -"DEBUG ALL" -lr        #dos real mode link  with full debug info

all: rd113.exe iocfg.exe  slib.exe

clean:
	del rd113.exe
	del iocfg.exe
	del slib.exe
	del rd113.obj
	del iocfg.obj
	del slib.obj

rd113.exe: rd113.obj
	$(LINKER) $(LFLAGS) rd113.obj

iocfg.exe: iocfg.obj
	$(LINKER) $(LFLAGS) iocfg.obj

slib.exe: slib.obj
	$(LINKER) $(LFLAGS) slib.obj

rd113.obj: rd113.c
	$(CC) $(CFLAGS) rd113.c

iocfg.obj: iocfg.c
	$(CC) $(CFLAGS) iocfg.c

slib.obj: slib.c
	$(CC) $(CFLAGS) slib.c

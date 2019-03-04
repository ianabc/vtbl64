/* vtbl.c  - decompression of Iomege *.113 backup files

   See www.willsworks.net/file-format/iomega-1-step-backup  for full project description
    
    Copyright (C) 2017  William T. Kranz

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
    
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

  
  
   I feel really stupic.  Aaron emailed last week to say he had worked out the data compression
   for Iomega 113 files.  Turns out its qic as in MSbackup.  See qic122b.pdf
   and HTML/wwhome/msbackup#??.htm   and DUMP/msbackup/qicdcomp.c
   I should have put this together from the VTBL tag which occures in all my files at offset 0x15c00
   ie the 3rd segment,  There is space for more than one, but my sample files only have the one.

   So here I skip ahead to the 1st VTBL record, read, and display it.
   next I will add stepping through the segments, and ultimately try to call the decompression routine
   one segment at a time.
   
   2/28/18 was my first cut at this.  I got it to open file and read VTBL header at 0xE800 
   Test for compressed, and terminate if not compressed.
   
   This first trial just called the qicdcomp.o routine to decomp_seg() for first data segment.
   This seems to work at this simplistic level, decompresses segment
   
   3/1/18 extend a little, add a couple command line arguments so can enable verbose mode
   or skip decompression and just list file segments.
   working on updating headers if write a dcomp.out, stilll needs a little work.
   Note I added a pad_seg2(), had something similar in qicdcomp.c so there was a name conflict.
   If move some of that code here to remove external refs will figure out a way to combine,
   maybe only write data if its not NULL?  Or adjust old logic to use new routine.
   
   I'm close, but something a little off. With current version I decompressed ImageEC.113 to dcomp.out 
   then ran rd113 on original uncompressed and my decompressed output.
   Both give same 3 drive directory listing with 3 drives, and a total of 13 dir entries.
   but with dcomp.out it sees PF_END flag and then tries to get another header which fails.
   Enough for now.
   
   The other curious thing is my decompressed files are noticably smaller the original uncompressed
   version.  Does original have garbage at end or I am lossing something....
   
   3/2/18 add some debug stuff after disp_vtbl() to check time stamps
   
   3/4/18 screwed with date_time issues in ttime.c for more than a day to work it out even given qic113g.pdf
   see qictime() and disp_datetime() which were added today from that work.

   3/5/18 added struct fhead113 in qicdcomp.h and -t flag to invoke header tests
   10pm late in day think I see significant logic error, when calc bytes written I am currently doing
   coff=lseek(fo,off,SEEKSET)  this looks wrong  should be coff = lseek(fo,0l,SEEK_END) to find currrent eof
   [ok its not so bad only used this for VERBOSE output record, but would have fucked that up if used -v option
   think all my bytes written maybe bougs due to incorrect positioning above.
   I backed up to vtbl2.c which has this bug.  Attempt to correct below and a totwr output tally
   this is debug stuff, but worth doing right
   
   WARNING need to add the RAW copy logic in main loop below eventually
   but for now need to work out were catalog is which is what I'm working on now.
   
   3/7/18 see ng-dcomp-cat.c which was a trail segment I added to main() where I tried to append
   what I thought might be catalog after detecting end of main compressed data chain.
   Now looks like this is wrong.  Change so it watches for source sgement # to advance to fhead1->blkcnt
   which I now believe to be the start of the catalog region at a segment boundary.
   add logic to pad output file to next segment boundry before continue decompression.  Save the new output file 
   segn value to put in the new output files fhead1.blkcnt
   
   This seems like a good time to start using struct fhead113  instead of older BYTE * for storage of two file headers.
  
   shit this doesn't seem to be it either.  Give up for now.
   
   3/8/18 back up yesterdays new trial to vtbl3.c
   wrote sdcomp.c as test routine.  I never get to last block in compressed file. With current logic that terminates 
   on shead.cum_sz == 0, this is typically shead of last in file, ie fhead->kcnt  but contains data which is catalog
   
   changed logic in main decompression loop so pads for catalog before decomp_seg()
   Think this is close, but needs debugging
   
   3/9/18 add calculation and update of vtbl.dirSZ, vtbl.dataSz, and vtbl.end
   oops rookie error I was setting # of segments in in output file headers to ns which is # segments read
   new to use output block count
   
   2/16/19 bumped version from 0.95 to 0.96 as change to new qicdcomp.h
   which includes 64 bit logic for define of DWORD
   also added a few corrects, esp in display of time, for 64 bit compatibility
   2/19/19 backing this up to vtbl1.c  It appears to have a bug in
   the decompress algorithm when I loose 50 to 100 bytes at the end of each
   decompressed segment.  Identified this in the work on Lane's large files.
   It apparently doesn't so up in my small sample files as neither the data nor 
   catalog span more than one segment?
   
   ok I've looked at code in msbackup/qicdcomp.c
   I don't see a glaring error.  But it includes msqic.h, not qicdcomp.h
   It has a couple globals:
   I currently access
     int fi,fo;       - input and output file handles
     FOFFSET comp_wr; - bytes written to output file fo
   I used to ignore but think may want to check:
     BYTE dcomp_verb;     - set to 1 for verbose mode in qicdcomp
     int hptr;            - number of bytes in temp input buffer (triggers flush_buf())
     FOFFSET comp_rd;     - number of bytes read from input
       
   adding some bit defines for state in main, and change it to a WORD so
   it can accomedate more flags.
   
   2/20/19 try a while loop for decomp_seg() in main
	  while(cum_data < shead.cum_sz) // try while loop 2/20/19
   this did not work, reads too far.  Need to understand the shead.cum_sz
   the decomp_seg() termination condition may really be different for these?
       
   premanently add test below above  after decom_seg()   
      if(shead.seg_sz != comp_rd)  print a warning, but these are always equal!

   2/22/19 I wrote a new version of bdiff.c and used it to compare the 
   output vtbl produces to some of may sample uncompressed files, ie ImageB.113
   The portion of each decompressed data produced matches the uncompressed file,
   but on order of 1/4 of data is never produced by current vtbl.
   After decomp_seg() the current shead.cum_sz + 0x15c00 (START_DATA) should be 
   the current file position.  In my tests its less.  Add a test to validate
   final file position in main based on chead.cum_sz (ignoring hi order long)
   If doesn't match currently only display as a warning, and then pad 
   output with zero bytes to make up difference so final lengths will be
   the same...
   
   2/25/19 tweaked some of display and added option -vv the other day
   expanded -t so it now takes an option #.  Current if # > 0 it is the 
   number of times to call dcomp_seg() for a single segment.  This is just
   a trial for now.
   
   oh dead I had a test for bytes read != shead.seg_sz but it was being
   ignored.  Somehow adding -t1 above triggers, review
   looks like it occurs on the 2nd pass with -t1
   change logic to handle, but seg_sz seems to match what I read in 1st pass!
   
   2/26/19 see notes in decomp_prob.txt  I think I am on the right track
   with this try surpressing pad() if test > 0
   and add an option :# to set skip value.  It looks like I may need to
   skip some control bytes before start 2nd decompress.
   
   ok backed this version up to vtbl2.c
   It appears to decompress correctly with my test option -t1:2
   which calls decomp_seg() twice, skipping 2 bytes after first call
   which turn out to be DWORD size of the next compressed data chunk.
   This seg_head.seg_sz is size of 1st segment.  Many of mine seem to have
   2 chunks, but need to watch this.  Should probably have a while loop
   on seg_head.cum_sz rather than assume anything. Yup a couple gave 3 chuncks and a number only 1
   Now modify the code below removing tests saved in vtbl2.c and making it always read on WORD length
   after each chunk to see if another follows, WORD at end of list always 0
   
   Had to add initialization of tot_wr when sn=3 for decomp of images which are not 1st in series.
   
*/


#include <stdio.h>
#include <fcntl.h>
#include <string.h>             // define memset
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>           /* for open() defines */
#include <stdlib.h>             // next for exit under linux

#if  defined(MSDOS) || defined(_WIN32)
#ifdef MSDOS
#define OS_STR "MSDOS"
#else
#define OS_STR "WIN32"
#endif
#include <memory.h>
#include <io.h>                 // for lseek
#else
#ifdef unix
#define OS_STR "LINUX"
#include <unistd.h>             // for read/write etc.
//#  define exit     _exit        // ralf likes this..
#define strnicmp strncasecmp
#define stricmp  strcasecmp
#ifndef O_BINARY
#define O_BINARY 0              // no difference between text and binary mode for LINUX
#endif
#else
#error Unknown build environment.
#endif
#endif

#include "qicdcomp.h"           // this replaces my original msqic.h

// #define VERSION "0.95"   current 3/9/18 version, appears to work with msbackup/qicdcomp.c
// #define VERSION "0.96"   // bump current version as add 64 bit compatible logic
// #define VERSION "0.97"   // bump version as add some debug tests and catalog only decomp
#define VERSION "0.98"          // implement multiple calls to decomp_seg for reach segment

#define BLK_SZ 1024
BYTE buf[BLK_SZ];               // used by most read routines. Assumes
                   // all blocks smaller than this, checks if correct


#define strnicmp strncasecmp
#define stricmp  strcasecmp

/*                   
3/4/18 developed form short time spec in qic113g.pdf
This seems to work with my *.113 sample file data 
should work with linux and open Watcom which have structure definitions for time_t and struct tm
call qictime() to get a struct tm pointer to a static buffer.  If need to retain copy to a new buffer before
calling qictime() again.  Can then call mktime() for a new time_t value which is OS specific
and ctime(time_t) to display the time value.

2/16/19 update for 64 bit compile.  Change arg for disp_datetime()
from unsigned long to DWORD to make it consistent, and simplify a little.
removeing qictime() and mktime() calls.
*/

struct tm *qictime(unsigned long *t)
{
    static struct tm tbuf;
    unsigned long secs = *t & 0x1ffffff;        // low order 25 bits are basically seconds elapsed
    tbuf.tm_year = (*t >> 25) + 70;     // note qic113 suggests should be + 1970 but the is wrong for these
    tbuf.tm_sec = secs % 60;
    secs /= 60;
    tbuf.tm_min = secs % 60;
    secs /= 60;
    tbuf.tm_hour = secs % 24;
    secs /= 24;
    tbuf.tm_mday = secs % 31 + 1;       // was doing +1, questionable may introduce an off by 1 (3/4/18)
    // could it be another error in spec, or difference from?
    secs /= 31;
    tbuf.tm_mon = secs % 11;
    return (&tbuf);
}


// from disp.c
#define FHDR_SZ 0x90            // apparent size of file header at offset 0


struct fhead113 *get_fheader(int fp)
{
    struct fhead113 *hdr = NULL;
    time_t *t;
    int sz, rd;
    sz = sizeof(struct fhead113);
    if ((hdr = (struct fhead113 *) malloc(sz)) == NULL ||
        (rd = read(fp, hdr, FHDR_SZ)) != sz) {
        if (hdr == NULL)
            printf("\nfailed to allocate space for header");
        else
            printf("\nonly read 0x%x bytes of 0x%x byte header", rd, sz);
    } else {
        if (hdr->sig != 0xAA55AA55) {
            printf("\ninvalid header signature 0x%lx != 0xAA55AA55");
            hdr = NULL;
        }
    }
    return (hdr);
}

// parse job and disk # from data string
int get_fhead_sdata(struct fhead113 *hdr, WORD * job, WORD * disk)
{
    int i, ret = 0;
    char *ch = hdr->desc;
    *job = 0;
    *disk = 0;
    if (sscanf(ch, "%d", &i) == 1) {
        ret++;
        *job = i;
    }
    for (i = 0; i < 6 && *ch != ','; i++, ch++);
    if (*ch == ',' && sscanf(ch + 7, "%d", &i) == 1) {
        ret++;
        *disk = i;
    }
    return (ret);
}

void do_dump(unsigned char *uptr, int lines)
{
    int i, j;
    printf("\n    hex dump of next 32 bytes follows:");
    for (i = 0; i < lines; i++) {
        printf("\n    ");
        for (j = 0; j < 16; j++)
            printf("%02x ", *uptr++);
    }
}



// from msqic.c

// see pp 204 in runtime C lib ref for my C/C++ ref (used only in testing)
void disp_dosdt(unsigned long date)
{
    int mon, day, yr, hour, min, sec;
    sec = 2 * (date & 0x1f);
    date = date >> 5;
    min = (date & 0x3f);
    date = date >> 6;
    hour = date & 0x1f;
    date = date >> 5;
    day = date & 0x1f;
    date = date >> 5;
    mon = date & 0xf;
    date = date >> 4;
    yr = date + 1980;           // high order 7 bits
    printf("%02d/%02d/%04d  %02d:%02d:%02d", mon + 1, day, yr, hour, min,
           sec);
}



#ifdef OLDSDATE                 // this was my old code savee for ref, I called with disp_dateime(long dt - VTBL_DATE_FUDGE)
                 // but it wasn't very close.  FUDGE defined in qichdcomp.h  replaced by below which actually works
#define BASEYR 1970             // for Unix or 1904 for Mac  just to be different!
unsigned char mondays[] =
    { 31, 28, 31, 30, 31, 30, 31, 31, 31, 30, 31, 31 };


/* may be an off by one day in below, but seems to work
   should be able to do more directly!
*/
void disp_datetime(unsigned long date)
{
    unsigned short yr = BASEYR, mon = 0, day, hour, min, sec;
    char lpyr;
    sec = date % 60;
    date /= 60;
    min = date % 60;
    date /= 60;
    hour = date % 24;
    date /= 24;
    do {
        if ((yr % 100) == 0 || (yr % 4) != 0) {
            day = 365;          // not a leap year
            lpyr = 0;
        } else {
            day = 366;          // is a leap year
            lpyr = 1;
        }
        if (date > day) {
            yr++;
            date -= day;
        }
    } while (date > day);
    day = date;

    mondays[1] += lpyr;
    while (mon < 12) {
        if (mondays[mon] >= day)
            break;
        else
            day -= mondays[mon++];
    }
    mondays[1] -= lpyr;
    printf("%02d/%02d/%04d  %02d:%02d:%02d", mon + 1, day, yr, hour, min,
           sec);
}
#else
void disp_datetime(DWORD date)
{
    /* 2/16/19 below appears to work for 32 bit compiles, and was working 
     * with 64 bit but it shouldn't! the date param above was an unsigned long
     * and its called with a DWORD.  Maybe compiler did the correct cast.
     * but I want this to be right, and see no need for qictime() or mktime(t) calls
     struct tm * t = qictime(&date);
     time_t  tv = mktime(t);
     */
    time_t tv = date;
    printf("%s", ctime(&tv));
}

#endif


char *flagbits[] = {
    "Vendor specific volume",
    "Volume spans multiple cartidges",
    "File sets written without verification",
    "Reserved (should not occur)",
    "Compressed data segment spaning",
    "File Set Directory follow data section"
};

char *OStype[] = {
    "Extended",
    "DOS Basic",
    "Unix",
    "OS/2",
    "Novell Netware",
    "Windows NT",
    "DOS Extended",
    "Windows 95"
};


void disp_vtbl(struct qic_vtbl *vtbl)
{
    int i, rd;

    printf("\nLabel: %.44s  \nVTBL volume contains %lu logical segments",
           vtbl->desc, vtbl->nseg);
// oh dear added 10/4, thought would be easy, but need to fudge?  why?
// 63072000 would be two years in seconds + 691200 is 8 days???
// above is value of VTBL_DATE_FUDGE also used in msqicrcv.c:create_vtbl()
    printf("\ncreated: ");
    disp_datetime(vtbl->date);
//         printf("\ncreated: "); disp_dosdt(vtbl->date);  way off yr=2519

    printf("\nflag 0x%x:", vtbl->flag);
    for (i = 0, rd = 1; i < 5; i++) {
        if (rd & vtbl->flag) {
            printf("\n%s", flagbits[i]);
            if (rd == 2)        // its multi-volume, display seq
                printf("   sequence #: %d: ", vtbl->seq);
        }
        rd = rd << 1;
    }
    if ((vtbl->flag & 1) == 0)  // generic, not vendor specific
    {
        // fields after flag not valid if vendor specific

        // ignore quad word, assume vtbl->dataSz[1] == 0
        printf("\nversion: %0x:%0x", vtbl->rev_major, vtbl->rev_minor);
        printf("\ndir size 0x%lx data size 0x%lx",
               vtbl->dirSz, vtbl->dataSz[0]);
        printf("\nQFA physical start block 0x%lx end block 0x%lx",
               vtbl->start, vtbl->end);
        printf("\ncompression byte 0x%x", vtbl->comp);
        if (vtbl->comp & 0x80)
            printf("\nCompression used, type 0x%x", vtbl->comp & 0x3f);
        if (vtbl->OStype < 8)
            printf("\nOS type: d => %s", OStype[vtbl->OStype]);
    }
}


// if data != NULL write data header block of sz, always pad remainder of segment with 0s
// if data == NULL just pad with sz zeros instead of rest of segment
int pad_seg2(int fp, BYTE * data, int sz)
{
    int suc = 0, wr = BLK_SZ;
    if (data != NULL) {
        if (write(fp, data, sz) != sz)
            suc++;
        sz = SEG_SZ - sz;       // remaining bytes in segment
    }
    memset(buf, 0, BLK_SZ);
    while (suc == 0 && sz > 0) {
        if (sz < BLK_SZ)
            wr = sz;
        if (write(fp, buf, wr) != wr)
            suc++;
        sz -= wr;
    }
    return (suc);
}

void fhead_tst(struct fhead113 *fh)
{
    int i;
    if (fh->unkwn[0] != 0xff || fh->unkwn[1] != 2 || fh->unkwn[2] != 0) {
        printf("\nnon std unkwn[] data = {");
        for (i = 0; i < 3; i++)
            printf("0x%x ", fh->unkwn[i]);
        putchar('}');
    }
    printf("\nblkcnt 0x%lx unkwn2[] = {", fh->blkcnt);
    for (i = 0; i < 4; i++)
        printf("0x%x ", fh->unkwn2[i]);
    putchar('}');

    printf("\n4 time stamps in header:\n");
    disp_datetime(fh->t1);
    disp_datetime(fh->t2);
    disp_datetime(fh->t3);
    disp_datetime(fh->t4);

    if (fh->unkwn4 != 1)
        printf("\nnon std unknw4 = %x", fh->unkwn4);
//     do_dump((unsigned char *)fh->desc,7);   used this to establish I needed to PACK structure

}

int append_desc(struct fhead113 *fh, char *str) // just for giggles add a string to desc
{
    int suc = 1, cnt = 0, l = strlen(str);
    char *ch = fh->desc + 42;   // end of buffer with space for terminating NUL 
    while (*ch == ' ' && cnt < l && cnt < 42) {
        ch--;
        cnt++;
    }
    if (cnt == l)               // string fits
    {
        strcpy(ch, str);
        suc = 0;
    }
    return (suc);
}

// this is a test/debug routine to pad the dcomp output file
#define PBUF 0x200
int pad(int fo, long pad, BYTE val)
{
    int b, suc = 0, wr;
    BYTE buf[PBUF];
    memset(buf, val, PBUF);
    while (pad > 0 && suc == 0) {
        if (pad > PBUF)
            b = PBUF;
        else
            b = pad;
        if (write(fo, buf, b) != b)
            suc++;
        else
            pad -= b;
    }
    return (suc);
}


extern int fin, fout;           // currently in msbackup/qicdcomp.c
extern FOFFSET comp_wr;         // for this version assum _4GB is defined and comp_wr is an unsigned long
extern FOFFSET comp_rd;
extern int hptr;
extern BYTE dcomp_verb;

// these are defines used locally in main() BYTE state
#define VERBOSE 1               // display data verbosely
#define C_DISP  2               // do not decompress a compressed file, just display segments
#define TEST    4
#define CAT_ONLY 8              // only decompress catalog  added 2/19/19

/* note all segments at 0x7400 bounteries, ie SEG_SZ from qicdcomp.h
 * 1st file header  at        0   has header with descriptive string included
 * 2nd file header  at   0x7400   this has similar but less info, ie descriptive string is all spaces
 * 3rd segment      at   0xE800   contains one VTBL record
 * 4th segment      at  0x15C00   start of date (compresses or uncompressed depending on VTBL)
 * 
*/



int main(int argc, char *argv[])
{
    int i, j, fo, fp, rd, suc = 0;
    BYTE incat = 0, pass;       // use pass to count decomp_seg() calls
    WORD state = 0, lseg_sz, seg_sz;    // previous and current seg_sz
    struct fhead113 *fhead1, *fhead2;
    struct qic_vtbl vtbl;
//    struct fhead113 *fh;  think this is no longer used
    struct cseg_head shead;
    unsigned long dt, tot_wr = 0, cum_data = 0, cum_dir = 0, lcum_sz = 0;
// note cum_data and cum_dir are only raw copy or decompressed data written
// tot_wr includes my padding with zeros as current decomp falls behind
    long off, p, cats = 0, coff, loff, *lptr, nblk, sn = 3;
    for (i = 2; i < argc; i++) {
        if (strnicmp(argv[i], "-v", 2) == 0) {
            state |= VERBOSE;
            if (strnicmp(argv[i], "-vv", 3) == 0)
                dcomp_verb++;   // produces a LOT if decomp info

        } else if (strnicmp(argv[i], "-c", 2) == 0) {
            state |= CAT_ONLY;
        } else if (strnicmp(argv[i], "-d", 2) == 0)
            state |= C_DISP;
        else if (strnicmp(argv[i], "-t", 2) == 0) {
            state |= TEST;      // see vtbl2.c for older options, no longer using TEST state
        }
    }

    printf("\nvtbl version %s compiled for %s\n", VERSION, OS_STR);
    if (state & CAT_ONLY && state & C_DISP) {
        printf("\nincompatible options -c can not be used with -d\n");
        exit(1);
    }

    if (argc < 2) {
        printf
            ("\nusage: vtbl <iomega.113 archive file> [-c] [-d] [-v[v]]");
        printf
            ("\n       -c only decompress catalog data and write to new file 'cat.out'");
        printf
            ("\n       -d surpress file decompression, just display compressed file segments");
        printf("\n       -t do header test/display and exit");
        printf
            ("\n       -v enables verbose mode for display, vv makes dcomp verbose");
        printf
            ("\n   by default write decompressed data to new file 'dcomp.out'\n");
        suc++;
    } else if ((fp = open(argv[1], O_BINARY | O_RDWR, S_IREAD)) < 0) {
        printf("error opening %s", argv[1]);
        suc++;
    } else if ((fhead1 = get_fheader(fp)) == NULL) {
        printf("\nfailed to get 1st file header");
        suc++;
    } else if (lseek(fp, SEG_SZ, SEEK_SET) != SEG_SZ
               || (fhead2 = get_fheader(fp)) == NULL) {
        printf("\nfailed to get 2nd file header");
        suc++;
    } else if ((off = lseek(fp, 2 * SEG_SZ, SEEK_SET)) != 2 * SEG_SZ
               || read(fp, &vtbl, sizeof(vtbl)) != sizeof(vtbl)) {
        printf("\nfailed to get VTBL");
        suc++;
    } else if (strnicmp(vtbl.tag, "VTBL", 4) != 0) {
        printf("\nmissing 'VTBL' tag, invalid record at offset 0x%lx ",
               off);
        suc++;
    } else {
        disp_vtbl(&vtbl);
        if (state & TEST) {
            printf("\n1st header at offset 0");
            fhead_tst(fhead1);
            printf("\n2nd header at offset 0x7400");
            fhead_tst(fhead2);
        } else                  // sumarrize header info
        {
//           a couple more validity checks
            printf("\n\nfhead1 nblk = %lx", fhead1->blkcnt);
            printf("\nfhead2 nblk = %lx", fhead2->blkcnt);
            printf("  hdr time:  ");
            disp_datetime(fhead1->t1);
            putchar('\n');
        }
    }

    if (suc == 0) {
        if (vtbl.comp == 0)
            printf("\nfile is not compressed");
        else                    // its compressed, step through compressed segments
        {
            off += 0x7400;      // advance to start of compressed data
            if (!(state & C_DISP) || state & CAT_ONLY)
                // doing decompression open and position output file at data start
            {
                if (state & CAT_ONLY) {
                    if ((fo =
                         open("cat.out",
                              O_BINARY | O_RDWR | O_CREAT | O_TRUNC,
                              S_IREAD | S_IWRITE)) < 0) {
                        printf("\nfailed to create output file: cat.out");
                        suc++;
                    }
                } else {
                    if ((fo =
                         open("dcomp.out",
                              O_BINARY | O_RDWR | O_CREAT | O_TRUNC,
                              S_IREAD | S_IWRITE)) < 0) {
                        printf
                            ("\nfailed to create output file: dcomp.out");
                        suc++;
                    } else if ((loff = lseek(fo, off, SEEK_SET)) != off) {
                        printf
                            ("\nfailed to advance output file to start of data");
                        suc++;
                    }
                }
                if (suc == 0) {
                    fin = fp;   // external file pointers in qicdcomp.c
                    fout = fo;
                }
            }
            while (suc == 0 && sn < fhead1->blkcnt) {
                if (lseek(fp, off, SEEK_SET) != off) {
                    printf("\nfailed to seek to 0x%lx", off);
                    suc++;
                } else if ((i = read(fp, &shead, sizeof(struct cseg_head)))
                           != sizeof(struct cseg_head)) {
                    printf("\nfailed to read cseg_head");
                    suc++;
                } else if (shead.cum_sz_hi != 0)        // temp 64 bit exclusion
                {
                    printf
                        ("\nabort: 64 bit file offsets not currently supported");
                    exit(1);
                } else {
                    if (sn > 3 && shead.cum_sz_hi == 0
                        && shead.cum_sz == 0) {
                        printf
                            ("\nthis was the last compressed data segment in file, start of compressed catalog");
                        tot_wr = 0;     // reset cumulative counter for dir
                        if (state & CAT_ONLY && incat == 0) {
                            printf
                                ("\nstarting catalog only decompression");
                        }
                        if (incat == 0)
                            incat = 1;  // signal decompress remaining data for catalog

                        // start of catalog adjust, need to pad to a new segment in output file
                        if (!(state & C_DISP) && !(state & CAT_ONLY)
                            && suc == 0)
                            // doing full decompression and no errors so far, pad fo to next seg boundry
                        {
                            coff = lseek(fo, 0L, SEEK_END);
                            if (coff < 0) {
                                printf
                                    ("\abort, error finding current output file position");
                                suc++;
                            } else {
                                i = coff % SEG_SZ;      // how far into current segment are we
                                if (i != 0) {
                                    i = SEG_SZ - i;     // # of bytes to append to get to next segment
                                    if (pad_seg2(fo, NULL, i)) {
                                        printf
                                            ("\nfailed to pad file to segment boundry before catalog");
                                        suc++;
                                    } else
                                        printf
                                            ("\npadded current segment with 0x%x NUL bytes to adv for catalog",
                                             i);
                                    coff += i;
                                }
                            }
                            if (suc == 0) {
                                cats = coff / SEG_SZ;
                                if (coff % SEG_SZ)
                                    printf
                                        ("\nwarning adjusted offset not at a SEG_SZ boundry!");
                                printf
                                    ("\ndecompressed catalog starts at 0x%lx  segment 0x%lx",
                                     coff, cats);
                            }
                        }
                        // end catalog adjust
                    }


                    if ((!(state & C_DISP) && !(state & CAT_ONLY))
                        || (state & CAT_ONLY && incat))
                        // doing full decompression or Catalog only and in catalog region
                    {
                        if (sn == 3)
                            tot_wr = shead.cum_sz;      // initialize to start value for this file
                        else if (shead.cum_sz != tot_wr) {
                            printf
                                ("\nfatal error: cum_sz 0x%lx != tot_wr 0x%lx",
                                 shead.cum_sz, tot_wr);
                            exit(1);    //2/26/19 changed from suc++ to an exit()
                        }

                        seg_sz = shead.seg_sz;  // initialize to shead value for 1st pass
                        lseg_sz = 0;
                        pass = 0;
                        // warning 2/26/19 I still don't pass seg_sz to decomp_seg() maybe should
                        // below currently only implemented for 32 bit file offsets
                        // make repeated calls to decomp_seg() to get all data from each segment

                        do {    // multiple calls to decomp_seg() to get all data, often 2 but open ended
                            // 3/8/18 add RAW logic from qicdcomp.c:do_decompress()
                            if (seg_sz & RAW_SEG)       // don't decompress this one!
                            {
                                if (state & VERBOSE)
                                    printf("\n: Raw data copy for this segment");       // never seen in my sample files
                                // do a raw copy of the segment
                                if (copy_region((long) (seg_sz & ~RAW_SEG))
                                    != 0) {
                                    printf(" - copy failed, abort");
                                    suc++;
                                } else {
                                    tot_wr += seg_sz & ~RAW_SEG;
                                    lseg_sz += seg_sz & ~RAW_SEG;
                                    if (incat)
                                        cum_dir += seg_sz & ~RAW_SEG;   // raw data appended
                                    else
                                        cum_data += seg_sz & ~RAW_SEG;  // raw data appended
                                }

                            } else {
                                decomp_seg();   // no error checking....
                                tot_wr += comp_wr;
                                lseg_sz += comp_rd;
                                if (incat)
                                    cum_dir += comp_wr; // dir bytes written
                                else
                                    cum_data += comp_wr;        // data bytes written

                                // test below always seems to work,  but seg_sz often much less than SEG_SZ
                                // the new 2/26/19 multi pass logic should resolve this...
                                if (seg_sz != comp_rd) {
                                    printf
                                        ("fatal decompression error, bytes read 0x%x != seg_sz 0x%x",
                                         comp_rd, shead.seg_sz);
                                    //suc++;
                                } else {        // get next seg_sz for current segment
                                    if ((rd =
                                         read(fp, &seg_sz,
                                              sizeof(WORD))) ==
                                        sizeof(WORD)) {
                                        if (state & VERBOSE && seg_sz > 0)
                                            printf
                                                ("\npass %d segment decomp region length 0x%x",
                                                 pass + 1, seg_sz);
                                    } else {
                                        printf
                                            ("\nfatal error reading next decomp region length");
                                        suc++;
                                    }
                                }
                            }
                            pass++;
                        } while (suc == 0 && !(seg_sz & RAW_SEG)
                                 && seg_sz > 0);

                        // end of decompress loop
                        if ((state & VERBOSE)
                            && (coff = lseek(fo, 0l, SEEK_END)) > loff) {
                            printf
                                ("\n%4ld: uncompressed data started at 0x%lx  wrote 0x%lx bytes",
                                 sn, loff, coff - loff);
                            loff = coff;
                        }
                    } else      // just display headers without file decompression
                    {
                        if (state & VERBOSE)    // show bytes written and compress ratio
                        {
                            // use dt as a temp unsigned long variable
                            if (sn > 3) // display stats for prev seg
                            {
                                if (shead.cum_sz < lcum_sz)     // wrapped to hi order DWORD
                                {
                                    dt = (0xffffffff - lcum_sz) + 1;
                                    dt += shead.cum_sz;
                                } else
                                    dt = shead.cum_sz - lcum_sz;
                                printf("\n      bytes written 0x%lx compression ratio %1.2f", dt, (float) dt / (float) SEG_SZ); // for file as a whole
                                // using SEG_SZ as divisor as there is some waste at end of each segment
                                // and the 1st three segments are wasted, which this ignores
                                // but its a good metric for the algorithm
                            }
                            // save current for next calc
                            lcum_sz = shead.cum_sz;
                            lseg_sz = shead.seg_sz;
                        }

                        printf
                            ("\n%4ld: seg_head cumulative bytes high 0x%lx  low 0x%lx  bytes in this segment 0x%x",
                             sn, shead.cum_sz_hi, shead.cum_sz,
                             shead.seg_sz);
                        coff =
                            off + sizeof(struct cseg_head) + shead.seg_sz;
                        pass = 1;
                        do {
                            if (lseek(fp, coff, SEEK_SET) != coff
                                || read(fp, &seg_sz, 2) != 2) {
                                printf
                                    ("\nfailed to read length for next pass");
                                suc++;
                            } else if (seg_sz > 0)
                                printf
                                    ("\n      pass %d  data len 0x%x at offset 0x%lx",
                                     ++pass, seg_sz, coff);
                            coff += 2 + seg_sz;
                        } while (suc == 0 && seg_sz > 0);
                    }
                }
                if (suc == 0) {
                    off += SEG_SZ;
                    sn++;
                }
            }

            if (state & VERBOSE && !(state & C_DISP))
                printf
                    ("\neof output file at 0x%lx  wrote 0x%lx dir bytes 0x%lx data bytes",
                     coff, cum_dir, cum_data);

            if (suc == 0 && vtbl.comp != 0 && !(state & C_DISP)
                && !(state & CAT_ONLY))
                // wrote a new dcomp.out, adjust file headers
            {
                i = cum_dir / SEG_SZ;   // number of full segments used by catalog
                if (cum_dir % SEG_SZ)
                    i++;        // total segments in use by catalog (normally 1)
                printf("\nupdating decompressed output file headers");
                // adjust number of segments in file headers
                // number of segments in use in file
                fhead1->blkcnt = cats + i;      // what should be done with fhead2->blkcnt?
                // should do more here, for now just turn off compression
                // should also adjust datasize, and probably dir size
                vtbl.comp = 0;
                // ignore dataSz quad word, force vtbl.dataSz[1] = 0
                vtbl.dirSz = cum_dir;
                vtbl.dataSz[0] = cum_data;
                vtbl.dataSz[1] = 0;     // assume hi order DWORD is 0
                vtbl.end = cats + i - 1;        // last segment in use in file
                append_desc(fhead1, " Decomp"); // just for giggles append a string to desc


                if ((off = lseek(fo, 0L, SEEK_SET)) != 0
                    || pad_seg2(fo, (BYTE *) fhead1, FHDR_SZ)) {
                    printf("\nfailed to write 1st header");
                    suc++;
                } else if (pad_seg2(fo, (BYTE *) fhead2, FHDR_SZ)) {
                    printf("\nfailed to write 2nd header");
                    suc++;
                } else
                    if (pad_seg2(fo, (BYTE *) & vtbl, (int) sizeof(vtbl)))
                {
                    printf("\nfailed to write VTBL");
                    suc++;
                } else
                    printf(" - success");


            }

        }
    }

    putchar('\n');
    return (suc);
}

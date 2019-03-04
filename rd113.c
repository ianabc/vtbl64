/* rd113.c  first attempt at reading *.113 files
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

  
   see 113.sum for history of archives and insights.

1/7/17 first attempt based on only uncompressed file I have ImageB.113
I have two compressed files where data starts at 0x15C08 which I am speculating
indicates a compressed file.  In the uncompressed file the data starts at 0x15C00
with a none zero value.

The headers for ImageB.113 and ImageBC.133 (the compressed version of same file)
are very similar.  Don't understand differences yet.

It focuses on parsing disk image directory header data which I think is what
is at the start of each file following signature {CC 33 CC 33}

ok just getting back into this.
DUMP/dosdisk.c with time stamp 5/20/02 includes my dosboot.h
DUMP/dos_dir.c with time stamp 12/30/09 does also and is actually of a
               similar ventage, but patch in 09 to handle a 2009 date rollover issue
               
see csource/include for dosboot.h and fat32.h
   dosboot.h does a nested include of fat32.h
   
1/12/17 debugging see 113.sum  seems to be a logic error in parse_ucomp()
with ImageC.113 and entry 37: for maxfilms.txt
See starting line 265, this all looks right, maybe the xlen logic wrong for this one?
Its set in get_dheader()  ok fixed problem...
backup up repaired version to rd113b.c

Now leave logic, but have get_dheader create a uni_str for path
and add to struct dhead

1/13/17 in the middle of the night I wondered if it might use attribute bits
to flag disk change.  I was surprised not to find all bits listed easily
on internet. The purpose of the key 4 was mentioned multiple places
but not the bit value!  Nor the subdir bit.  Oddly enough what I wanted
is in my wwhome/fat32info.htm
//    Flags used for Attributes member of DIR_ENTRY
//
//       bit 0 - readonly
//           1 - hidden
//           2 - system
//           3 - volume (Netware = execute-only)
//           4 - directory
//           5 - archive
//           6 - UNUSED
//           7 - UNUSED BY DOS (NetWare = shareable, Novell DOS = pending delete)

#define  ATTRIBUTE_NORMAL     0x00  // no attributes set
#define  ATTRIBUTE_READONLY   0x01  // read-only
#define  ATTRIBUTE_HIDDEN     0x02  // hidden
#define  ATTRIBUTE_SYSTEM     0x04  // system
#define  ATTRIBUTE_VOLUME     0x08  // volume label
#define  ATTRIBUTE_DIRECTORY  0x10  // sub-directory
#define  ATTRIBUTE_ARCHIVE    0x20  // file is new or changed (for backup purposes)
so there are a couple bits to spare. However see lfn discussion, which
has bearng on iocfg.c although none on rd113.c as the names are already expanded.

1/18/17 got a glimpse of how compressed data organized, see struct comp_head
and parse_comp()  needs more work, what I thought was clen
doesn't seem to be, nor is 1st try at nblk working out...
1/19/17 back up current parse_comp() logic to parse_comp1.c
then change to what I think better reflects what it does and might
work as a decompressor if I can work the compression logic out.

ah ha, find there is a catalog and as in *.1_step the file header
points to it.  In this case the long at offset 0xa in header is
block # where catalog starts.  add struct cat_head and parse_cat()
routine.  The cat_head is quite similar to dir_head, but has no signature bytes,
nor the path strings.

1/20/17 correct error in do_tests(), the final display of fixed data was wrong.
unknw1[] correct, unknw2[] and unknw3[] redisplayed data from unknw1[]
back up above to rd113c.c
add tests for catalog entries, via do_tests1() and do_tests2() by making
somd subroutines to do the work on unknwn[] arrarys.

1/21/17 added conditional define FTSTAMP in get_fheader()
to try display of longs as timestamps with ctime().  Its close, but not
right, see comments in routine.

1/22/17 may have finally found how drive letter/path is done.  See
plen = unknw1[10] and pflg = unknw1[14] additions in parse_ucomp() a little 
complex but doable.  Yes, that seems to do it.
Now try adding -x and -p logic as in 1Srest.c  got that working in a long day.

1/23/17 compiled (possibly for first time) with watcomw  It gives a lot of warning messages
specifically about arguments to ctime().  Try to clean this up a little but declaring
pointer I use a time_t *t  rather than long *
also added ret value = len from get_cheader()

1/25/17 spent yesterday writting some docs in HTML/ghome/
today I create a two media disk backup by tweeking the size available on zip disk
see Image14a.113 and Image14b.113  Its different.  As 1st step try -f
option which I do not include in useage for force acceptack of data file
start adding a little more to try to parse these but need to backup this version soon.
add parsing of job and diskn parameters to in main from data returned by get_fheader()

ok got a working work around with -f option for parse_ucomp() by adding a
soff parameter to that routine and adding a find_sig() routine so
if forcing, search for next FILE_SIG and assume its start of listing.
find_sig() a little rough, but works in this case.

backup current 'working code' to rd113d.c
Will try one last mod. remove xlen code in get_dheader() and remove
xlen in struct dir_head. Also cleanup read of
path so it uses plen from unknwn1[10] and pflg from unknw1[14]
When displaying dhead in VVERB mode add plen and pflg above
   remove in unknw1[] display in VVERB mode
2/15/17 oh dear, did 1st ver 4.4 backup.  It by default (even though only drives D: and C:)
includes a huge # of Windows Registry files.  More than half the backup I made.
Hence catalog is huge and not in next to last block.  Add code to search backwards to start
of catalog.

1/11/18 add conditional USE64BITS which must be defined for 64 bit systems where
  int is 4 bytes and long is 8 bytes
  
2/15/19 add return type int for functions create_path() and test_results()
  which previously had none, and add they type int for param in get_unistr()
  to get rid of some warnings.
  replace USE64BITS define above with preprocessor  __x86_64__ so
  auto detects
  couple minor changes with casts to long for DWORD values.
  change 3rd param of do_extract() to a long value rather than long *flen
  to simplify cast.
  Turns out most time values need mods for 64 bits where time_t is 8 bytes long.
  In 32 bit code its only 4 bytes and can case a DWORD value to a time_t, not in 64 bit.
  think I got them all, buts needs validation
  
  2/16/19 add a conditional DEBUG option to add debugging options
  add reading struct vtbl in main() via new routine get_VTBL()
  and CALL TO disp_vtbl() from vtbl.c if a vtbl is read.

  I was in the middle of mods, what I may need is a raw parse, may 
  have done that before, see new notes in ./Lane/bill_lane.txt
  
  2/17/19 correct fnd_cat() to use DWORD compare, was using longs for 
  key dwords at start of catalog
  fix time display in parse_cat(), old assumed sizeof(time_t) = 4
  
  It appears Lane's data files and ver are create by ver 71 as mine were
  add some debug stuff in parse_cat() esp toff 
  also at toff test in get_unistr() and both a warning and a pad adjustment
  in get_cheader if 'OIMG' not found in unkwn3 region
  
  2/20/18 added an option in vtbl to write just catalog to cat.out 
  it has same problems, but a little easier to debug as a much smaller file.
  add some special code to call do_debug() before check file headers and vtbl
  if DEBUG defined and option -dc0 is given to view these new files.
  Needs to be called early as these files have no file headers nor vtbl region.
  
  2/27/19  I am starting to modify source code again.  I backed up current
  source to rd113-92.c and the executable to /usr/local/bin/rd113-92
  
  add -d option to replace vtbl program. call new routine do_decomp()
  which contains logic from main() in vtbl.c
  call is surpressed if input files vtbl indicates it is not a compressed file.
  bump VERSION to 0.94
  
  oh dear, when I started this my mode parameter all fit in a char, ie 8 bits
  I explanded to a WORD around  but was no consistent about this.
  Until today only a char param was passed to 
      parse_ucomp(),  parse_cat(), and parse_cat()
  an int was passed to get_fheader()  and a WORD to recent do_debug()
  Changed all occurrances to WORD today, although it most cases I was
  only pulling low order bit for VERBOSE, but good to be consistent.
  and I needed to expand as move decompression in from vtbl.c
  add new DECOMP define for mode if decompressing, and
  add C_DISP and CAT_ONLY that were used in VTBL to control display option
  and restrict decompression to the catalog region
  use -d option flag for struct seg_head display only which matches vtbl useage
  add -u option flag to turn on decompression or -uc for catalog only
      not yet implemented, but direction I am planning on.
      
  See the backup rd113-92.c for the prior logic where I read 1st 8 bytes
  at offset 0x15c00 to attempt to determine type of file.
  I have now included vtbl logic, and check it instead.
     removed calls to parse_cat() parse_comp() or parse_ucomp() for now.
     not sure about above, a parse_comp() call maybe better than my vtbl logic
     but would simplify.
     
  2/28/19 add logic based on hdr1.nblk - vtbl.dirSz to locate start catalog
    this allows it to correctly find start of catalog in volumes with multiple dir segments
    
  3/1/19 see decomp-prob.txt which has expanded beyond just decomp issues.
  I hope I solved the decomp issue.
  Now I am working on parse_ucomp() which is called via -dd<offset> option
  It looks like Lane's data includes a variable length data section following the file
  data, but is the same up to that point.  Add conditional LANE in parse_ucomp()
  to search for next DIR_SIG after reading or skipping file data.
  Hopefully this will work with my existing files which don't have the additional data
  and Lane's.
  It introduced a problem with ImageB.113 as find_sig() called at or near eof and it
  did not terminate correctly.  I have been diddling find_sig() to resolve this.
  Its a moderately messy routine! but seems ok now.
  
  Not surprisingly some of Lanes paths, esp new end of Image1 and in Image2
  are much longer than anything I tested.  Error cheding still sloppy on buffer reads
  in get_dheader() but allocate space to read path into rather than trying to fit in
  global buffer and then allocate.  This should solve problem with it failing to detect
  FILE_SIG.  Surprising I didn't get an access violation as think was overwritting memory.
*/



#include <stdio.h>
#include <time.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>             // for malloc() and exit() to surpress warning about implicit declaration


#if  defined(MSDOS) || defined(_WIN32)
#ifdef MSDOS
#define OS_STR "MSDOS"
#else
#define OS_STR "WIN32"
#endif
/* search routines from msqic compiled for large file access
*/
#define IS_UNIX 0
#include <io.h>                 // for lseek
#include <sys\types.h>
#include <sys\stat.h>           // this include must follow types.h for MSC
#include <sys\utime.h>
#include <direct.h>             // for mkdir()
#else
// its a unix like environment
#define IS_UNIX 1
#define OS_STR "LINUX"
#define O_BINARY 0              // required for DOS, this makes that bit Linux compatible
#include <unistd.h>             // for lseek, read, write
#include <sys/stat.h>
#include <sys/types.h>
#include <utime.h>

#define strnicmp strncasecmp
#define stricmp  strcasecmp

// linux subsitute for DOS function time()
long time(long *t)
{
    struct timespec ts;
    long ret = 0;
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0)
        ret = ts.tv_sec;
    if (t != NULL)
        *t = ret;
    return (ret);
}
#endif


#include "qicdcomp.h"           // My DWORD,WORD,BYTE defines and struct vtbl

#define VERSION "0.94"          // current program version, preliminary - does not handle compression

#define HDR_SZ 0x90             // apparent size of file header at offset 0

#define BLK_SZ 0x7400           // size of blocks, esp compressed data blocks where it matters

#define MAX_DRV 6               // allocated size of drives[] array

#define VTBL_SIG "VTBLK" treat this as a string, its a total of 8 bytes padded with zeros
// treating other signatures as unsigned longs reverses byte order....
#define FHEAD_SIG  0xAA55AA55

#define DIR_SIG   0x33CC33CC

#define FILE_SIG  0x66996699

#define START_DATA 0x15C00      // default offset to start of data in uncompressed file

// bitmaps for pflg used in parse_ucomp(), note several bits unaccounted for!
#define PF_CONT  0              // contine processing
#define PF_NEW   1              // a new entry, typically a drive or subdir
#define PF_LAST  8              // last entry at this level
#define PF_END  0x20            // looks like indicator for end of data
#define PF_DRV  0x40            // drive spec

// bitmaps for my mode define in main(), also passed to some of subroutines
#define VERBOSE 1               // verbose display
#define VVERB   2               // even more verbose display
#define DH_SAVE 4               // allocate and save dhead records in parse_ucomp() or chead in parse_cat()
#define XTRACT  8               // xtract data
#define CATALOG 16              // display catalog
#define FORCE   32              // attempt to allow it to continue even if doesn't look valid
#define DECOMP  64              // decompress input file (could be full or CAT_ONLY)
#define C_DISP   128            // just display struct seg_head data, don't decompress
#define CAT_ONLY 256            // restrict decompression or display to catalog region
// change mode to a short and added some debug options in hig bits
#define F_CAT  0x101            // force catalog at offset

struct uni_str {
    WORD len;
    char ustr[];
};


struct dir_head {
    DWORD dsig;
    BYTE unknw1[69];
    struct uni_str *nm1;        // 1st variable length unicode string
    BYTE unknw2[21];            // 2nd variable length unicode string
    struct uni_str *nm2;
    BYTE unknw3[28];            // the fixed string "OIMG" occurs starting at unknw3[12]
    struct uni_str *nm3;        // 3rd variable length unicode string
    struct uni_str *path;       // maybe Null if unknw1[10] == 4
};

struct cat_head {
    BYTE unknw1[69];
    struct uni_str *nm1;
    BYTE unknw2[21];
    struct uni_str *nm2;
    BYTE unknw3[28];            // the fixed string "OIMG" occurs starting at unknw3[12]
    struct uni_str *nm3;
};

struct dir_list {
    int cnt;
    long off;                   // offset to dhead location in file
    struct dir_head dhead;      // struct to store data read in get_dheader()
    struct dir_list *next;      // pointer to next in list.
};

struct cat_list {
    int cnt;
    long off;                   // offset to chead location in file
    struct cat_head chead;      // struct to store data read in get_cheader()
    struct cat_list *next;      // pointer to next in list.
};

// add below to handle call from disp_vtbl()  done wrong in vtbl???
void disp_datetime(DWORD dt)
{
    time_t t = dt;
    printf("%s", ctime(&t));
}

/* following routine disp_vtbl() and its support strings copied directly
 * from vtbl.c as it turns out all files (compressed or not have one in 3rd segment
*/

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

int do_decomp(int fp, WORD mode, char *hdr1, char *hdr2,
              struct qic_vtbl *vtbl)
{
    int i, j, fo;
}

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


#define BUF_SZ 0x400

BYTE buf[BUF_SZ];               // global working buffer  used in several routines

/* search for sig from current file position to loff
*/
long find_sig(int fp, long loff, unsigned long sig)
{
    int i, len, rd, ret = 0;
    long coff;                  // current offset
    loff -= 3;                  // last read address allowable to contain a long before loff above
    if ((coff = lseek(fp, 0L, SEEK_CUR)) < 0)
        return (coff);
    else if (coff >= loff) {
        printf("\nalready at end of data!");
        return (-1L);
    }
    coff -= 3;
    do {
        len = loff - coff;
        if (len <= 0)
            break;
        if (len > BUF_SZ)
            len = BUF_SZ;
        if (lseek(fp, -3L, SEEK_CUR) != coff ||
            (rd = read(fp, buf, (int) len)) <= 4)
            ret = -1;
        else {
            len = rd;
            for (i = 0; i <= len - 4 && ret == 0; i++) {
                if (sig == *((DWORD *) (buf + i))) {
                    coff += i;
                    ret = 1;    // got a hit
                }
            }
            if (ret == 0)
                coff += len - 3;
        }
    } while (ret == 0);
    if (ret == 1)
        return (coff);
    else
        return (-1L);
}



// add this 2/16/19
struct qic_vtbl *get_vtbl(int fp)
{
    int suc = 0;
    long off;
    struct qic_vtbl *pvtbl;
    if ((pvtbl = malloc(sizeof(struct qic_vtbl))) == NULL)
        printf("\nfailed to allocate space for VTBL header");
    else if ((off = lseek(fp, 2 * SEG_SZ, SEEK_SET)) != 2 * SEG_SZ ||
             read(fp, (BYTE *) pvtbl,
                  sizeof(struct qic_vtbl)) != sizeof(struct qic_vtbl)) {
        printf("\nfailed to read VTBL header");
        suc++;
    } else if (strnicmp(pvtbl->tag, "VTBL", 4) != 0) {
        printf("\nmissing 'VTBL' tag, invalid record at offset 0x%lx ",
               off);
        suc++;
    }

    if (pvtbl != NULL && suc != 0) {
        free(pvtbl);
        pvtbl = NULL;
    }

    return (pvtbl);
}

/* to date only things I have identified at
 * long signature at offset 0
 * long number of 0x7400 data blocks ng at offset 0xA
*/
BYTE *get_fheader(int fp, WORD mode)
{
    BYTE *hdr = NULL;
    DWORD *sig;
    time_t tv;
    int rd = 0;
    if ((hdr = (unsigned char *) malloc(HDR_SZ)) == NULL ||
        read(fp, hdr, HDR_SZ) != HDR_SZ) {
        if (hdr == NULL)
            printf("\nfailed to allocate space for header");
        else
            printf("\nonly read 0x%x bytes of 0x%x byte header", rd,
                   HDR_SZ);
    } else {
        sig = (DWORD *) hdr;
        if (*sig != 0xAA55AA55) {
            printf("\ninvalid header signature 0x%lx != 0xAA55AA55");
            hdr = NULL;
        }
#ifdef FTSTAMP
/* define FTSTAMP to try this display
 * its close to being a timestamp value, but 
 * values 1,2, and 4 are the same while 3 is on second later
 * probably not what it represets as well as fact the file made in 2017 
 * display as 2020
*/
        else {
            if (mode & VERBOSE) {
                tv = *((DWORD *) (hdr + 14));
                printf("\nfile header timestamp1: %s", ctime(&tv));
            }
            if (mode & VVERB) {
                tv = *((DWORD *) (hdr + 18));
                if (!(mode & VERBOSE))
                    putchar('\n');
                printf("file header timestamp2: %s", ctime(&tv));
                tv = *((DWORD *) (hdr + 0x4a));
                printf("file header timestamp3: %s", ctime(&tv));
                tv = *((DWORD *) (hdr + 0x8a));
                printf("file header timestamp4: %s", ctime(&tv));
            }
        }
#endif
    }
    return (hdr);
}

// parse job and disk # from data string
int get_fhead_sdata(BYTE * hdr, WORD * job, WORD * disk)
{
    int i, ret = 0;
    char *ch = (char *) (hdr + 40);     // change from 41 to 40, still not very flexible
    *job = 0;
    *disk = 0;
    if (sscanf(ch, "%d", &i) == 1) {
        ret++;
        *job = i;
    }
    for (i = 0; i < 6 && *ch != ','; i++, ch++);
    if (*ch == ',' && sscanf(ch + 6, "%d", &i) == 1) {
        ret++;
        *disk = i;
    }
    return (ret);
}


/* just learning a little about compressed blocks which seem to be BLK_SZ bytes long
 * and block cnt is related to (long *)(hdr+0xa)
*/
int parse_comp(int fp, WORD mode, char *hdr)
{
    long cnt = 1, off, len;
    unsigned long *l_lo, *l_hi, *l_dat;
    int ret = 0;
    unsigned char *ibuf, *pch;
    if ((ibuf = (unsigned char *) malloc(BLK_SZ)) == NULL) {
        printf("\nfailed to allocate input buffer");
        ret = -3;
    }
    l_lo = (unsigned long *) ibuf;
    l_hi = l_lo + 1;            // point to 1st 3 longs in ibuf
    l_dat = l_lo + 2;
    off = START_DATA;
    if (lseek(fp, off, SEEK_SET) != off) {
        printf("\nerror seeking to start offset %0xlx", off);
        ret = -1;
    }

    while (ret == 0) {
        if (read(fp, ibuf, BLK_SZ) != BLK_SZ) {
            printf("\nblock read error");
            ret = -2;
        } else if (*l_dat == 0 || (cnt > 1 && *l_lo == 0)) {
            printf("\n looks like EOF for compressed data at 0x%08lx",
                   off);
            break;
        } else {
            printf("\n%2d: off 0x%08lx uncompressed off 0x%08lx",
                   cnt++, off, *l_lo);
            off += BLK_SZ;
        }
        len = BLK_SZ;
        pch = ibuf + (len - 1); // point to last char in block
        while (len > 0 && *pch == 0) {
            len--;
            pch--;
        }
        printf(" data length 0x%lx", len - 8);  // there are 2 longs at start of record
        // I expect values <= 0x73f7 as I expect a least one NUL terminator in data stream
    }
    if (ret < 0)
        return (ret);
    else
        return (cnt);
}

struct uni_str *get_unistr(int fp)
{
    WORD rd, w;
    long toff;                  // for debugging
    struct uni_str *us = NULL;
    toff = lseek(fp, 0L, SEEK_CUR);     // get current position for debugging
    if ((rd = read(fp, &w, 2)) == 2 &&
        (us = (struct uni_str *) malloc((int) w + 2)) != NULL) {
        us->len = w;            // string length
        if ((rd = read(fp, (char *) us + 2, w)) != w) {
            free(us);
            us = NULL;
        }
    }
    return (us);
}

struct uni_str *dup_uni_str(struct uni_str *us)
{
    struct uni_str *us2;
    if ((us2 = (struct uni_str *) malloc((int) us->len + 2)) != NULL)
        memcpy(us2, us, (int) us->len + 2);
    return (us2);
}

void disp_uni_str(struct uni_str *ustr)
{
    int i = 0;
    char *ch = (char *) ustr + 2;
    while (i < ustr->len)       // print every other character
    {
        putchar(*ch);
        ch += 2;
        i += 2;
    }
}

void disp_uni_path(struct uni_str *path)
{
    int i = path->len;
    char *ch = path->ustr;
    while (i > 0)               // display uni code xstring
    {
        if (*ch == 0)
            putchar('/');
        else
            putchar(*ch);
        ch += 2;
        i -= 2;
    }
}

void free_dhead(struct dir_head *dh)
{
    if (dh->nm1 != NULL)
        free(dh->nm1);
    if (dh->nm2 != NULL)
        free(dh->nm2);
    if (dh->nm3 != NULL)
        free(dh->nm3);
    if (dh->path != NULL)
        free(dh->path);
}

void free_chead(struct cat_head *ch)
{
    if (ch->nm1 != NULL)
        free(ch->nm1);
    if (ch->nm2 != NULL)
        free(ch->nm2);
    if (ch->nm3 != NULL)
        free(ch->nm3);
}


/* seemed straight forward at first, but was confused by 3 different strings.
 * hoping one is the short DOS name and one is long filename.  But 3rd seems to be path.
 * why is it file name is initial samples?  sigh
 * 1/8/17 find what was unknw4[] is a screwy variable length thing, so put it all in buf[] for now
 * and just save total # bytes in xlen
 * 1/12/17 think this is a little off still for ImageC.113 entry 37 maxfilms.txt
 * ah ha, it seems this is my 1st sub directory with two nodes in path,
 * ie DOS/text and I was terminating when read a word == 0. Should terminate
 * when see start of FILE_SIG, ie word == 0x6699
 * 1/12/17 on success now create uni_str dh->path  bit also leave data in buf[]
 * as it does no harm and maybe useful for debugging
 * 1/26/17 remove xlen and change path to uni_str * path in struct def.
 * xlen was path->len which == plen == unknw1[10]
*/
int get_dheader(int fp, struct dir_head *dh)
{
    // was unsigned char *plen = till 3/1/19
    WORD *plen = (WORD *) & (dh->unknw1[10]);   // lenth of drv string and path string
    int i, rd, ret = 0, xlen = 0;       // xlen used to be in struct dir_head, now local
    long tsig = 1;
    dh->nm1 = dh->nm2 = dh->nm3 = dh->path = NULL;

    if ((rd = read(fp, (BYTE *) dh, 73)) != 73 || (tsig = *((DWORD *) dh)) != DIR_SIG) {        // read 1st fixed region and validate signature DWORD
        if (rd == 73 && tsig == 0)
            return (1);         // valid EOF
        else {
            printf("\ninvalid header");
            ret = -1;
        }
    }
    // allocate space a read variable lenth unicode strings and fixed regions into structure
    else if ((dh->nm1 = get_unistr(fp)) == NULL)
        ret = -2;
    else if ((rd = read(fp, (BYTE *) & dh->unknw2, 21)) != 21)
        ret = -1;
    else if ((dh->nm2 = get_unistr(fp)) == NULL)
        ret = -2;
    else if ((rd = read(fp, (BYTE *) & dh->unknw3, 28)) != 28)
        ret = -1;
    else if ((dh->nm3 = get_unistr(fp)) == NULL)
        ret = -2;
    else if ((rd = read(fp, buf, 2)) != 2)      // should be word 0xA that follows 3rd uni_str
        ret = -1;
    else
        xlen = rd;              // 1st 2 bytes in buffer normally 0A 00
    // if there is no path FILE_SIG follows immediately

    if (strnicmp((char *) &dh->unknw3[12], "OIMG", 4) != 0)
        printf("\nwarning 'OIMG' not found to dheader");
    // new logic based on *plen still fills buf with data read for debugging
    if (*plen > 4)              // there is path data in addition to drive spec
        // until 3/1/19 just read into global buf[], Lane's paths too long, allocate space before read
    {
        i = *plen - 4;
        if ((dh->path = (struct uni_str *) malloc(i + 2)) != NULL) {
            dh->path->len = i;
            if ((rd = read(fp, dh->path->ustr, i)) != i)
                ret = -1;       // read error
        } else
            ret = -2;           // allocation error      
    }
    if (ret == 0)               // read trailing bytes {0,0}, FILE_SIG, and bytes {7,0} which always follow
    {
        if ((i = read(fp, buf + xlen, 8)) != 8)
            ret = -1;
        else if (*((DWORD *) (buf + xlen + 2)) != 0x66996699) {
            printf
                ("\nfailed to find FILE signature 0x66996699 at end of header");
            ret = -3;
        } else
            xlen += 8;          // read last part of structure data

    }
    /* add funky variable length uni_str type name that is not preceeded by a length word
       // instead it is terminated by a 0x6699 word, start of FILE_SIG
       // this makes it a little trickier as has to read 1st part of sig to detect end strings
       while(ret == 0 && (rd = read(fp,buf+dh->xlen,2)) == 2) 
       {
       dh->xlen+=2;
       if(*((WORD*)(buf+dh->xlen-2)) == 0x6699)
       break; // end of data
       }
       if(rd != 2)
       ret = -1; // read error

       else if((rd = read(fp,buf+dh->xlen,4)) == 4)  // read last FILE_SIG bytes
       {
       dh->xlen+=4;  // this includes the 0x7 word that follows FILE_SIG
       // now create dh->path with data just read, its an extended series of strings
     */

    if (ret < 0)
        free_dhead(dh);         // release any allocated uni_str data
    if (ret == -1)
        printf("\nread error in get_dheader()");
    else if (ret == -2)
        printf
            ("\n read or allocation error in get_unistr() for get_dheader()");
    return (ret);
}


/* this dynamically allocates 3 variable length strings as it reads the 
 * header data into struct cat_head *ch.  These are released by free_chead()
*/
int get_cheader(int fp, struct cat_head *ch)
{
    int i, len = 0, pad = 0, rd, ret = 0;
    char *cptr = &ch->unknw3[12], *targ = "OIMG", tbuf[14];     // 2/17/19 for new test
    ch->nm1 = ch->nm2 = ch->nm3 = NULL;
    long coff;                  // for DEBUG msg

    if ((rd = read(fp, (BYTE *) ch, 69)) != 69) {
        // should watch for valid termination condition...
        printf("\nread error");
        ret = -1;
    } else if ((ch->nm1 = get_unistr(fp)) == NULL)
        ret = -2;
    else if (ch->nm1->len == 0)
        ret = 1;                // looks like end of listing with empty string
    else if ((rd = read(fp, (BYTE *) & ch->unknw2, 21)) != 21)
        ret = -1;
    else if ((ch->nm2 = get_unistr(fp)) == NULL) {
        free(ch->nm1);
        ch->nm1 = NULL;
        ret = -2;
    } else if ((rd = read(fp, (BYTE *) & ch->unknw3, 28)) != 28)
        ret = -1;
#ifdef DEBUG
    else
        while (pad < 12 && strnicmp(cptr, targ, 4) != 0) {
            pad++;
            cptr++;
        }
    if (pad > 0) {
        coff = lseek(fp, 0L, SEEK_SET) - 16;

        if (pad >= 12) {
            printf
                ("\nabort in get_cheader() could not find string 'OIMG' @0x%lx\n",
                 coff);
            ret = -3;
        } else {
            if ((rd = read(fp, tbuf, pad)) != pad) {
                printf("fatal read error padding unknwn3 @ 0x%lx",
                       coff - 12);
                ret = -3;
            } else
                printf
                    ("Warning had to skip %d bytes to find 'OIMG' in unknw3[] @0xlx\n",
                     pad, coff - 12);
        }
    }
#endif

    if (ret == 0 && (ch->nm3 = get_unistr(fp)) == NULL) {
        free(ch->nm1);
        free(ch->nm2);
        ch->nm1 = ch->nm2 = NULL;
        ret = -2;
    }
    if (ret == 0) {
        // length is fixed len + pad + 3 unknow region lengths + 3 shorts in uni_str.len + 3 uni_str.len
        ret = 124 + pad + ch->nm1->len + ch->nm2->len + ch->nm3->len;
    }

    return (ret);
}


/* correct path so names are suitable for OS and relative to current dir,
 * ie remove drive spec from names converting "C:" to "C_" and
 * changing '\' to '/' if UNIX is defined
 * also copied from 1Srest.c
*/
int validate_dir(char *p)
{                               // ultimately check for existance and create if required
    struct stat sb;
    int suc = 0;
    if (stat(p, &sb) == 0)      // something with that name exists
    {
        if (!(sb.st_mode & S_IFDIR))
            suc = 1;            // but its not a directory
    } else                      // must try to create this sub directory
#if IS_UNIX
    if (mkdir(p, 0777) != 0)
        suc = 2;
#else                           // fuck msc doesn't support _mkdir() although Lib Ref says it does, watcom doesn't support mkdir()
    if (mkdir(p) != 0)
        suc = 2;                // there is no permissions argument in DOS
#endif

    return (suc);
}

#if IS_UNIX
#define PTERM '/'
#else
#define PTERM '\\'
#endif
int create_path(char *p)
{
    int i, len, ret = 0;
    char *ch = p;
    len = strlen(p);
    for (i = 0; i < len && ret == 0; i++, ch++)
        if (*ch == PTERM)       // for each node in path string
        {
            *ch = 0;
            ret = validate_dir(p);
            *ch = PTERM;        // restore path separator
        }
    if (ret == 0)
        ret = validate_dir(p);  // get last node which has no PTERM
    return (ret);
}

// shift it up two chars so can make it a local file by inserting "./"
// at start of file, replace ':' with '_' and '\\' with '/'
// copied from 1Srest.c
void adjust_path(char *ch)
{
    while (*ch != 0) {
#if IS_UNIX
        if (*ch == '\\')
            *ch = '/';
#endif
        if (*ch == ':')
            *ch = '_';
        ch++;
    }
}


// convert unicode paths to relative ascii path in buf[]
int mk_path_str(struct uni_str *drv, struct uni_str *path)
{
    WORD w;
    int i;
    unsigned char *ch = buf;    // global buffer
    w = drv->len;
    for (i = 0; i < w; i += 2)
        *ch++ = *(drv->ustr + i);
    *ch = 0;
    if (path != NULL) {
        *ch++ = '\\';           // use DOS termination
        w = path->len;
        for (i = 0; i < w; i += 2) {
            if (*(path->ustr + i) == 0 && i < w + 2)    // there is another string following
                *ch++ = '\\';
            else
                *ch++ = *(path->ustr + i);
        }
        *ch = 0;
    }
    return (strlen(buf));
}

// only called if file is to be extracted, local path is in global buf[]
// must create any required directories, append file name, and the write data
// 2/15/19 chanr len parm from pointer to value to simplify 64 bit logic
int do_extract(int fp, struct uni_str *nm, long len, time_t * t,
               BYTE atrib)
{
    int fo, rd, ret = 0;
    char *ch;
    long wr = 0;
    adjust_path(buf);           // make it local and if required with unix terminators
    // first try full path, may just have added a node
    // if that fails try to create full path, node by node
    if (validate_dir(buf) != 0 && create_path(buf) != 0) {
        printf("\nfailed to create path %s", buf);
        ret = 1;
    } else                      // append file name, open it, and copy file
    {
        ch = buf + strlen(buf); // end of path
        *ch++ = PTERM;
        for (rd = 0; rd < nm->len; rd += 2)
            *ch++ = *(nm->ustr + rd);
        *ch = 0;
        if ((fo =
             open(buf, O_BINARY | O_RDWR | O_CREAT | O_TRUNC,
                  S_IREAD | S_IWRITE)) < 0) {
            printf("\nfailed to open %s for output", buf);
            ret = 2;
        }
        while (ret == 0 && wr < len)    // copy file data
        {
            if (len - wr > BUF_SZ)
                rd = BUF_SZ;
            else
                rd = len - wr;
            if (read(fp, buf, rd) != rd || write(fo, buf, rd) != rd) {
                ret = 3;
                printf("\nfile io error");
            } else
                wr += rd;
        }
        // ultimately set timestamp and attrib
        if (fo > 0)
            close(fo);
    }
    return (ret);
}

int parse_ucomp(int fp, WORD mode, long soff, struct dir_list **ptrl,
                char *xpath)
{
    int cnt = 0, i, j, nx = 0, rd, ret = -1;
    char adv, *ch, verb = mode & VERBOSE;
    struct uni_str *drv[MAX_DRV];
    struct dir_head dhead;      // used to store data from get_dheader()
    long off, coff;
    DWORD *flen = (DWORD *) & dhead.unknw1[17]; // this apppears to be file length
    DWORD *tptr;
    time_t tv;
    // as of 1/22/17 these two appear to control drive # parsing
    unsigned char ndrv = 0, cdrv = 0, lrflg = 0, *plen =
        &dhead.unknw1[10], *pflg = &dhead.unknw1[14];

    if (lseek(fp, soff, SEEK_SET) != soff) {
        printf("\nfailed to seek to 0x%x for start of data", soff);
        return (ret);
    }

    printf("\nBackup File listing:");
    while ((off = lseek(fp, 0l, SEEK_CUR)) > 0
           && (ret = get_dheader(fp, &dhead)) == 0) {
        printf
            ("\n%3d: Directory entry at offset 0x%lx  length 0x%lx  attrib 0x%x",
             ++cnt, off, (long) *flen, dhead.unknw1[41]);
        if (mode & VVERB) {
            printf
                ("\nunknw1[0] 0x%x unknw1[2-3] 0x%04x unknw1[4] 0x%x unknw1[55-56] 0x%04x",
                 dhead.unknw1[0], *((unsigned short *) &dhead.unknw1[2]),
                 dhead.unknw1[4], *((unsigned short *) &dhead.unknw1[55])
                );

            printf("\nunknw1[63-64] 0x%04x  unknw1[67-68] 0x%04x",
                   *((unsigned short *) &dhead.unknw1[63]),
                   *((unsigned short *) &dhead.unknw1[67]));
        }
        tptr = (DWORD *) & dhead.unknw1[45];
        tv = *tptr;             // the 1st of 3 timestamps
//      printf("\ntimestamps");
        if (verb) {
            printf("\n     path len 0x%x path flag  0x%x",
                   dhead.unknw1[10], dhead.unknw1[14]);
            printf("\n  1: ctime() of value 0x%lx: %s", (long) tv,
                   ctime(&tv));
        } else
            printf("\n     ctime() of value 0x%lx: %s", (long) tv,
                   ctime(&tv));

        if (verb) {
            for (i = 2; i < 4; i++) {
                tptr += 2;      // every 8 bytes  
                tv = *tptr;
                printf("%3d: ctime() of value 0x%lx: %s", i, (long) tv,
                       ctime(&tv));
            }
            tv = *((DWORD *) & dhead.unknw2[13]);       // 4th timestamp after 1st uni_str
            printf("  4: ctime() of value 0x%lx: %s", (long) tv,
                   ctime(&tv));
        }

        printf("     ");
        disp_uni_str(dhead.nm1);
        if (verb)               // so far the next 2 are identical!
        {
            printf("\n     ");
            disp_uni_str(dhead.nm2);
            printf("\n     ");
            disp_uni_str(dhead.nm3);
        }

        if (mode & VVERB) {
            printf("\nunknw2[12-13] 0x%04x  unknw2[19-20] 0x%04x",
                   *((unsigned short *) &dhead.unknw2[12]),
                   *((unsigned short *) &dhead.unknw2[19]));
            printf("\nunknw3[16] 0x%x  unknw3[20-21] 0x%04x",
                   dhead.unknw3[16],
                   *((unsigned short *) &dhead.unknw3[20]));
        }



        if (dhead.path != NULL) {
            printf("\n     ");
            disp_uni_path(dhead.path);
        }

        // we are in extration mode and not defining drives actually write file data out
        adv = 1;                // default seek ahead past file data
        if (*flen == 0)
            adv = 0;            // do not advance when *flen == 0  no need
        else if (mode & XTRACT) // flen is 0 when its a drive or dir spec
        {
            mk_path_str(drv[cdrv], dhead.path); // make dos style asci path in global buf[]
            if (xpath != NULL)
                j = strlen(xpath);
            else
                j = 0;
            // backup is not consistent about case of drive, check listing to be sure
            if (xpath == NULL
                || (j > 0 && (i = strncmp(xpath, buf, j)) == 0)) {
                tv = *((DWORD *) & dhead.unknw1[45]);   // use 1st timestamp
                if (do_extract
                    (fp, dhead.nm1, (long) *flen, &tv,
                     dhead.unknw1[41]) == 0) {
                    printf("\nfile extracted successfully");
                    nx++;
                    adv = 0;    // don't seek ahead was done in above
                } else {
                    printf("\nfatal error in do_extract(), abort");
                    ret = -3;
                    break;
                }
            }
        }
        //if flen > 0 seek past file  always read trailer and release data allocated in dhead
        // adv is set if do_extract() called which advances over file data
        if (adv && lseek(fp, *flen, SEEK_CUR) < 0) {
            printf("\nerror skipping over file data");
            ret = -3;
            break;
        }

        // attempt to add drive logic here, seems to work!
        if (*pflg & PF_DRV) {
            if (*pflg & PF_NEW)
                drv[ndrv++] = dup_uni_str(dhead.nm1);
            if (*pflg & PF_LAST) {
                printf
                    ("\n\nlast drive spec for a total of %d drives, starting on drive ",
                     ndrv);
                if (ndrv > 0)
                    disp_uni_str(drv[cdrv]);
                else
                    putchar('?');
                putchar('\n');
            }
        } else if (*plen == 4)  // we are in root and not adding drives
        {
            if (lrflg & PF_LAST)
                if (cdrv < ndrv - 1) {
                    cdrv++;
                    printf("\nincrement drive to ");
                    disp_uni_str(drv[cdrv]);
                } else
                    printf
                        ("\ndrive logic error lrflg == PF_LAST, but on last drive");
            lrflg = *pflg;      // set last root level flag
        }

        if (*pflg & PF_END)     // this was a debug test, but seems to be EOD marker
            printf("\nsaw PF_END flag in this record");


        if ((rd = read(fp, buf, 18)) != 18 ||
            *((DWORD *) & buf[0]) != FILE_SIG) {
            off = lseek(fp, 0l, SEEK_CUR);
            printf
                ("\nfailed to read valid trailer, bytes read below end at offset 0x%lx:\n",
                 off);
            for (i = 0; i < rd; i++) {
                j = buf[i];
                printf("0x%02x ", j);
            }
            putchar('\n');
            ret = -4;
            break;
        }
#ifdef LANE                     // this may be generic, but not sure, Lane's files differ from mine, seem to require this
        coff = lseek(fp, 0l, SEEK_CUR);
        if ((off = find_sig(fp, coff + 0x300, DIR_SIG)) > 0 && off > coff)      // the 0x300 is arbitrary for now
        {
            if (verb)
                printf
                    ("\nfound DIR_SIG 0x%lx bytes after expected!, skip ahead at offset 0x%lx",
                     off - coff, coff);
            coff = off;         // advance to new location
        }
        if (lseek(fp, coff, SEEK_SET) != coff)  // always reset fp as find_sig() call changes
        {
            printf
                ("\nerror adjusting file position to next DIR_SIG in parse_ucomp()");
            ret = -5;
            break;
        }
#endif

        if (mode & DH_SAVE) {
            if ((*ptrl =
                 (struct dir_list *) malloc(sizeof(struct dir_list))) ==
                NULL) {
                printf("\nmemory allocation error saving record");
                ret = -2;
                break;
            } else {
                (*ptrl)->cnt = cnt;
                (*ptrl)->off = off;
                memcpy(&((*ptrl)->dhead), &dhead, sizeof(struct dir_head));
                (*ptrl)->next = NULL;
                ptrl = &((*ptrl)->next);
            }
        } else
            free_dhead(&dhead); // release previous uni_str data
        putchar('\n');
    }
    if (nx > 0)
        printf("\n%d files extracted", nx);
    if (ret == 1)
        printf
            ("\nappears to be EOF for uncompressed archive data at 0x%lx\n",
             off);
    if (ret >= 0)
        return (cnt);           // valid entries
    else {
        if (ret <= -1 && ret >= -3)
            printf(" following offset 0x%lx", off);     // error in get_dheader() parse
        return (ret);
    }
}



long fnd_cat(int fp, long off)
{
    long ret = -1;
    unsigned char sbuf[16];
    while (off > START_DATA && ret < 0) {
        if (lseek(fp, off, SEEK_SET) != off) {
            printf
                ("\nfailed to seek to 0x%lx for while searching for start of catalog",
                 off);
            break;
        } else if (read(fp, sbuf, 16) != 16) {
            printf
                ("\nread error at 0x%lx while searching for start of catalog",
                 off);
            break;
        } else if (*((DWORD *) sbuf) == 0xA80086 &&
                   *((DWORD *) (sbuf + 4)) == 0 &&
                   *((DWORD *) (sbuf + 8)) == 0x40000)
            ret = off;
        else
            off -= BLK_SZ;

    }
    return (ret);
}

/* parse_cat() 
 * originally worked out parse_ucomp() which parses data in the data region of
 * an uncompressed file at START_DATA.  Then discovered most have a separate catlog
 * region which this parses, very similar to above, but a few fields not there
 * Note in my short sample file, the catalog fits in the last block.
 * In a large file this might not be true, a while loop seeking to start of catalog
 * data maybe required for generic version that works with large files.
 * Not clear what conditions to use,  Looks like Drive spec starts
 * with byte sequence {86 00 A8 00  00 00 00 00}
 * 
 * 2/15/17 try adding this search for ver44a.113
 * 2/17/19 added DEBUG conditional for testing from do_debug.  About to got
 * to a new version the uses information in VTBL to directly jump to start 
 * of catalog without need of fnd_cat().  But try to find bug I uncovered
 * in Lane where where at cnt=146 the display gets messed up.  This was
 * largest backup I'd tried.
*/
int parse_cat(int fp, WORD mode, struct cat_list **ptrl, long off)
{
    int cnt = 0, i, ret = -1;
    char *ch, verb = mode & VERBOSE;
    struct cat_head chead;      // used to store data from get_dheader()
    DWORD *flen = (DWORD *) & chead.unknw1[17]; // this apppears to be file length
    time_t tv;
    long toff;                  // 2/17/19 add to test offsets

#ifndef DEBUG                   // 2/17/19 below is old logic, don't do if debug defined
    if ((off = fnd_cat(fp, off)) < 0L) {
        printf("\nfailed to find start of catalog");
        return (ret);
    }
#endif
    if (lseek(fp, off, SEEK_SET) != off) {
        printf("\nfailed to seek to 0x%lx for start of catalog", off);
        return (ret);
    }
    printf("\nCatalog listing:");
    while ((ret = get_cheader(fp, &chead)) > 1) // valid entry
    {
        if (cnt == 236)
            i = 0;              // dummy command for debug

        printf
            ("\n%3d: Directory entry at offset 0x%lx  length 0x%lx  attrib 0x%x",
             ++cnt, off, (long) *flen, chead.unknw1[41]);
        printf("\n      ");
        disp_uni_str(chead.nm1);
        i = 50 - (int) chead.nm1->len / 2;
        while (i-- > 0)
            putchar(' ');
        tv = *((DWORD *) & chead.unknw1[45]);   // 1st timestamp
        printf("%s", ctime(&tv));
//        printf("0x%lx",(long) *t);
        if (mode & DH_SAVE) {
            if ((*ptrl =
                 (struct cat_list *) malloc(sizeof(struct cat_list))) ==
                NULL) {
                printf("\nmemory allocation error saving record");
                ret = -3;
                break;
            } else {
                (*ptrl)->cnt = cnt;
                (*ptrl)->off = off;
                memcpy(&((*ptrl)->chead), &chead, sizeof(struct cat_head));
                (*ptrl)->next = NULL;
                ptrl = &((*ptrl)->next);
            }
        } else
            free_chead(&chead); // release previous uni_str data

        off += ret;             // size of chead with dynamically allocated strings is returned
        toff = lseek(fp, 0L, SEEK_CUR); // get current position
        if (toff != off) {
            printf("\nabort offset error %lx != %lx", off, toff);
            ret = -4;
            break;
        }
    }
    if (ret == 1) {
        printf
            ("\nappears to be end of Catalog with an empty name string at 0x%lx\n",
             off);
    }
    if (ret >= 0)
        return (cnt);
    else
        return (ret);
}


/* test I am most interested in, is which of the fix region unknw? bytes
 * are constant for all records
 * there are 3 of these regions
 *   BYTE  unknw1[69],unknw2[21],unknw3[28];  
 *   // the fixed string "OIMG" occurs starting at unknw3[12]
*/
void init_tests(BYTE unknwn[], BYTE bconst[], BYTE unknw1[], BYTE unknw2[],
                BYTE unknw3[])
{
    int i, j;
    for (i = 0, j = 0; i < 69; i++, j++) {
        unknwn[j] = unknw1[i];
        bconst[j] = 1;          // initialize to true => doesn't change
    }
    for (i = 0; i < 21; i++, j++) {
        unknwn[j] = unknw2[i];
        bconst[j] = 1;          // initialize to true => doesn't change
    }
    for (i = 0; i < 28; i++, j++) {
        unknwn[j] = unknw3[i];
        bconst[j] = 1;          // initialize to true => doesn't change
    }
}

int test_rec(BYTE unknwn[], BYTE bconst[], BYTE unknw1[], BYTE unknw2[],
             BYTE unknw3[])
{
    int i, j, mcnt;
    mcnt = 0;
    for (i = 0, j = 0; i < 69; i++, j++) {
        if (unknwn[j] != unknw1[i])
            bconst[j] = 0;      // change to false => did change
        else
            mcnt++;             // matches in this record
    }
    for (i = 0; i < 21; i++, j++) {
        if (unknwn[j] != unknw2[i])
            bconst[j] = 0;      // change to false => did change
        else
            mcnt++;             // matches in this record
    }
    for (i = 0; i < 28; i++, j++) {
        if (unknwn[j] != unknw3[i])
            bconst[j] = 0;      // change to false => did change
        else
            mcnt++;             // matches in this record
    }

    return (mcnt);
}

int test_results(BYTE unknwn[], BYTE bconst[])
{
    int i, j, mcnt = 0;
    printf("\nthe following locations have fixed values for all data");
    for (i = 0, j = 0; i < 69; i++, j++)
        if (bconst[j]) {
            printf("\nunknw1[%d] = 0x%x", i, unknwn[j]);
            mcnt++;
        }
    for (i = 0; i < 21; i++, j++)
        if (bconst[j]) {
            printf("\nunknw2[%d] = 0x%x", i, unknwn[j]);
            mcnt++;
        }
    for (i = 0; i < 28; i++, j++)
        if (bconst[j]) {
            printf("\nunknw3[%d] = 0x%x", i, unknwn[j]);
            mcnt++;
        }
    printf("\n%d matches for all out of 118 possible unknwn? bytes", mcnt);
    return (mcnt);
}

// run matching unknow tests on on file dir_head entries
int do_tests1(struct dir_list *dlst)
{
    BYTE unknwn[118], bconst[118];
    int i, j;
    struct dir_list *d = dlst;  // temp list pointer
    int cnt = 0, mcnt;
    init_tests(unknwn, bconst, d->dhead.unknw1, d->dhead.unknw2,
               d->dhead.unknw3);
    d = d->next;                // skip ahead
    printf
        ("\ncheck matching bytes between all entries for File archive entries");
    cnt = 2;
    while (d != NULL) {
        mcnt =
            test_rec(unknwn, bconst, d->dhead.unknw1, d->dhead.unknw2,
                     d->dhead.unknw3);
        printf("\n%2d: matches %d", cnt++, mcnt);
        d = d->next;
    }
    mcnt = test_results(unknwn, bconst);
    return (mcnt);
}

// run matchin unknow tests on catalog
int do_tests2(struct cat_list *clst)
{
    BYTE unknwn[118], bconst[118];
    int i, j;
    struct cat_list *d = clst;  // temp list pointer
    int cnt = 0, mcnt;
    init_tests(unknwn, bconst, d->chead.unknw1, d->chead.unknw2,
               d->chead.unknw3);
    d = d->next;                // skip ahead
    printf
        ("\ncheck matching bytes between all entries for catalog entries:");
    cnt = 2;
    while (d != NULL) {
        mcnt =
            test_rec(unknwn, bconst, d->chead.unknw1, d->chead.unknw2,
                     d->chead.unknw3);
        printf("\n%2d: matches %d", cnt++, mcnt);
        d = d->next;
    }
    mcnt = test_results(unknwn, bconst);
    return (mcnt);
}

int do_tests3(struct dir_list *dlst, struct cat_list *clst)
{
    int i, j, recn = 0, cnt = 0;
    BYTE dunkw[118], cunkw[118], bconst[118];
    printf("\ncompare files list to catalog list unknwns[]");
    while (dlst != NULL && clst != NULL) {
        init_tests(dunkw, bconst, dlst->dhead.unknw1, dlst->dhead.unknw2,
                   dlst->dhead.unknw3);
        init_tests(cunkw, bconst, clst->chead.unknw1, clst->chead.unknw2,
                   clst->chead.unknw3);
        printf("\nrec %d", ++recn);
        for (i = 0, j = 0; i < 69; i++, j++)
            if (dunkw[j] != cunkw[j]) {
                printf("\nunknw1[%d] mismatch 0x%x != 0x%d", i, dunkw[j],
                       cunkw[j]);
                cnt++;
            }
        for (i = 0; i < 21; i++, j++)
            if (dunkw[j] != cunkw[j]) {
                printf("\nunknw2[%d] mismatch 0x%x != 0x%d", i, dunkw[j],
                       cunkw[j]);
                cnt++;
            }
        for (i = 0; i < 28; i++, j++)
            if (dunkw[j] != cunkw[j]) {
                printf("\nunknw2[%d] mismatch 0x%x != 0x%d", i, dunkw[j],
                       cunkw[j]);
                cnt++;
            }
        dlst = dlst->next;
        clst = clst->next;
    }
    return (cnt);
}

// debug routines, only enabled in main() if DEBUG is defined
int do_debug(int fp, WORD mode, BYTE * hdr, struct qic_vtbl *vtbl,
             int argc, char *argv[])
{
    int i, ret = 0;
    long off;
    struct cat_list *clist = NULL;
    struct dir_list *ptrl = NULL;
    struct cat_head ch;
    DWORD *flen = (DWORD *) & ch.unknw1[17];    // this apppears to be file length

    for (i = 1; i < argc; i++)
        if (strnicmp(argv[i], "-dc", 3) == 0)   // parse catalog entry
        {
            if (sscanf(argv[i] + 3, "%lx", &off) == 1) {
                ret = parse_cat(fp, mode, &clist, off);
                printf("\nparse_cat() = %d", ret);
            } else
                printf("\nfailed to parse hex offset in %s, cmd ignored",
                       argv[i]);
        } else if (strnicmp(argv[i], "-dd", 3) == 0)    // parse data region after a DIR_SIG
        {
            if (sscanf(argv[i] + 3, "%lx", &off) == 1) {
                // call with a null extract path
                ret = parse_ucomp(fp, mode, off, &ptrl, NULL);
                ret = 1;        // do not continue with program
            } else
                printf("\nfailed to parse hex offset in %s, cmd ignored",
                       argv[i]);
        }
    return (ret);
}

int main(int argc, char *argv[])
{
    int dblk, fp, i, ret = 0;
    unsigned char *hdr, tstn = 1;
    WORD job, disk;             // parsed here from *hdr returned by get_fheader()
    WORD mode = 0;
    long off;
    unsigned long nblk;
    char *xpath = NULL;
    struct qic_vtbl *vtbl = NULL;       // dynamically allocated in get_vtbl()
    struct cat_list *clist = NULL;
    struct dir_list *dlist = NULL;

    printf("\nrd113 version %s compiled for %s ", VERSION, OS_STR);
#if __x86_64__
    printf("64 bit system");
#endif
    putchar('\n');

    for (i = 2; i < argc; i++)
        if (strnicmp(argv[i], "-vv", 3) == 0)
            mode |= VVERB;      // display values of variable unknw?[] fields
        else if (strnicmp(argv[i], "-v", 2) == 0)
            mode |= VERBOSE;    // display all unicode strings even when redundant
        else if (strnicmp(argv[i], "-t", 2) == 0) {
            mode |= DH_SAVE;
            if (*(argv[i] + 2) != 0)
                tstn = *(argv[i] + 2) - '0';
        } else if (strnicmp(argv[i], "-c", 2) == 0)
            mode |= CATALOG;
        else if (strlen(argv[i]) == 2 && strnicmp(argv[i], "-d", 2) == 0)
            // need to distinguish '-d' from debug options '-dc' and '-dd'
            mode |= C_DISP;     // just display decompression when -u set
        else if (strnicmp(argv[i], "-u", 2) == 0) {
            mode |= DECOMP;     // do decompression (unless -d also used) to dcomp.out
            if (strnicmp(argv[i], "-uc", 3) == 0)
                mode |= CAT_ONLY;       // decompress catalog only to cat.out
        } else if (strnicmp(argv[i], "-x", 2) == 0)
            mode |= XTRACT;
        else if (strnicmp(argv[i], "-f", 2) == 0)
            mode |= FORCE;
        else if (strnicmp(argv[i], "-p", 2) == 0)
            xpath = argv[i] + 2;
#ifdef DEBUG
        else if (strnicmp(argv[i], "-dc", 3) == 0) {
            // need this special case here as do not want to read fhead or vtbl
            // when reading a saved catalog file, it contains neither
            if ((fp = open(argv[1], O_RDONLY | O_BINARY)) == EOF)
                printf("open failed source file %s\n", argv[1]);
            else {
                i = do_debug(fp, mode, NULL, NULL, argc, argv);
                printf
                    ("\ndisplay cataog file called as stand alone routine\n\n");
            }
            exit(i);
        }
#endif

    if (argc < 2) {
        printf
            ("\nusage: rd113 <input file> [-c] [-d] [-f] [-p<path>] [-u[c]] [-v] [-vv] [-t] [-x]\n%s\n",
             "attempts to summarize or extract backup file contents");
        printf("\noptional -c display catalog from uncompressed file");
        printf
            ("\noptional -d display compressed segments in a compressed file");
        printf("\noptional -f set flag to force uncompressed file mode");
        printf
            ("\noptional -p set xtract <path>, so only extract files at of below this");
        printf("\noptional -t save dir_list and run tests");
        printf
            ("\noptional -u decompress input file into new file 'dcomp.out'");
        printf
            ("\n         -uc decompress catalog region of input file into new file 'cat.out'");
        printf("\noptional -v set verbose mode to display more fields");
        printf
            ("\noptional -vv set very verbose display independently from -v");
        printf
            ("\noptional -x set xtract flag, => all files unless -p is also set");
#ifdef DEBUG
        printf("\nDEBUG options enabled");
        printf("\n -dc<hex offset>  - attempt catalog listing at offset");
        printf("\n -dd<hex offset>  - attempt raw data listing at offset");
#endif
        putchar('\n');
        return (ret);
    } else
        printf("\n using:file %s", argv[1]);

    if ((fp = open(argv[1], O_RDONLY | O_BINARY)) == EOF) {
        printf("open failed\n");
        ret = fp;
    } else if ((hdr = get_fheader(fp, mode)) == NULL) {
        printf("\nget_fheader() failed");
        ret = -2;
    } else if ((vtbl = get_vtbl(fp)) == NULL) {
        printf("\nget_vtbl() failed");
        ret = -3;
    } else {
        disp_vtbl(vtbl);
        putchar('\n');
        nblk = *((DWORD *) (hdr + 0xa));
        dblk = vtbl->dirSz / SEG_SZ;
        if (vtbl->dirSz % SEG_SZ)
            dblk++;             // total # of directory blocks
        off = SEG_SZ * (nblk - dblk);
        printf
            ("\ntotal block count %ld - %d dir blocks => catalog at offset 0x%lx",
             nblk, dblk, off);
        if ((off = lseek(fp, 0L, SEEK_END)) > 0) {
            printf("\noffset to end of file = 0x%lx => %ld blocks", off,
                   off / BLK_SZ);
        }
        i = get_fhead_sdata(hdr, &job, &disk);
        if (i == 2) {
            printf("\nJob %d, Disk %d", job, disk);
        }
    }
#ifdef DEBUG
    if (ret == 0 || mode & FORCE) {
        i = do_debug(fp, mode, hdr, vtbl, argc, argv);
        if (i != 0) {
            printf("\ndebug routine called and terminated execution\n\n");
            exit(i);
        }
    }
#endif
    if (ret != 0) {
        putchar('\n');
        return (ret);           // exit()
    } else if (vtbl->comp == 0
               && (mode & DECOMP || mode & C_DISP || mode & CAT_ONLY)) {
        printf
            ("\nThis is not a compressed volume, decompression operations not allowed\n");
        return (0);
    } else if (vtbl->comp != 0
               && (mode & CATALOG || mode & DH_SAVE || mode & XTRACT)) {
        printf
            ("\nthis is a compressed volume, it must be decompressed before attempting current options\n");
        return (0);
    } else if (vtbl->flag & 2 && vtbl->seq > 0) {
        printf("\n%s and is sequence # %d", flagbits[1], vtbl->seq);
        printf
            ("\npress 'q' to quit and try -l option to load all files in volume");
        printf
            ("\nany other key to attempt a raw search for files in this image file");
        i = getchar();
        if (i == 'q' || i == 'Q')
            return (0);
        if ((off = lseek(fp, START_DATA, SEEK_SET)) == START_DATA &&
            (off = find_sig(fp, off, DIR_SIG)) > 0) {
            printf
                ("\nfound signature 0x%lx at offset 0x%lx as next file in data region",
                 DIR_SIG, off);
            printf("\nat least one file will be skipped with this method");
            ret = parse_ucomp(fp, mode, off, &dlist, xpath);
        } else
            printf
                ("\ndid not find anything that looks like a DIR_SIG in this file\n");
    }


    if (clist != NULL)
        printf(" clist != NULL");
    if (dlist != NULL)
        printf(" dlist != NULL");

    if (tstn <= 2) {
        if (tstn == 1 && dlist != NULL) // 
            do_tests1(dlist);
        if (tstn == 2 && clist != NULL)
            do_tests2(clist);
    } else if (tstn == 3 && dlist != NULL && clist != NULL)
        do_tests3(dlist, clist);

    // now see what options are enabled [remove DH_SAVE stuff, forget what it is!
    if (mode & CATALOG)
        ret = parse_cat(fp, mode, &clist, (SEG_SZ * (nblk - dblk)));

    putchar('\n');
    return (ret);
}

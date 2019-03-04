/* qicdcomp.h  - include file for QIC decompression structures
  
    See www.willsworks.net/file-format/iomega-1-step-backup  for full project description
    
    Copyright (C) 2017  William T. Kranz

    This code is part of a free software project: you can redistribute it and/or 
    modify it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
    
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

 

   copied from beginning of /Archive/dos/csource/DUMP/msbackup/msqic.h
   these are defines require for the qic decompression, see /Archive/dos/csource/DUMP/msbackup/qicdcomp.c

   defines for msqic work
   made separate file 10/14/03

   need __attribute__ ((packed))
   for linux gcc

note the qic_vtbl seems ok without PACKED, but cseg_head requires!

see section 7.1.3 of QIC 113 Spec for this info, below works in qic80.c
in *.qic there are two related structures which I named ms_qic_fix*

struct dir_fixed {
BYTE len,   // of the rest of the record + vendor data
     attrib;
DWORD datetime,
     size;   // file size in bytes + data header size, or 0 for empty dir
BYTE extra;  // flag byte containing file info
// min length is 0xA, ie include extra but no vendor data
} __attribute__ ((packed));

above not what Win9x MSBackUp uses! 
also note EDAT_SIG and DAT_SIG only occur in data region not dir set

11/31/03 add some bitmap defines and .err field in struct dir_blk
12/14/03
add field struct cseg_head *pseg;
    in struct dir_blk
12/16/03 per Ralf Westram's input try to clean up some of
signed unsigned issues in DWORD verus long for file offsets
If I use as offset should probably be a long!
in struct vtbl_ver change .database and .dirbase
12/22/03 try some FOFFSET logic
 change struct vtbl_ver offset fields back to DWORD

1/7/04 add Ralf's EMPTYDIR define
1/11/04 change params to do_extract()
   remove #define SEARCH 2   may never have been used...
   update tree_node() adding BYTE mode argumnet
   add do_redirect() prototype
1/20/04 add DELIM definition for path separator
1/22/04 change args for do_extract()
4/28/04 add RECREAT to indicate the original VTBL was invalid
    Also requires WIN_MASK and change value of WIN95 and WINME
    Need in do_decompress() if attempt to reconstruct archive
    Note there are a couple places where I should be using
    FOFFSET instead of DWORD, I've left them alone for ease
    in displaying data, but see vtbl_ver and seg_head

    add mk_date() prototype
    add define for VTBL_DATE_FUDGE
10/22/08 in NH add two fields in struct vtbl_ver for v1.12
    -c and -o options
02/15/19 this is the iomega/113/ version of this file.
   today I am adding some 64 bit logic so can build and run on those
   systems.  The structures below already used DWORD, WORD, and BYTE typedefs
   so I just needed to add the 64 bit conditional logic for DWORD so
   it is 8 bytes on a 64 bit system.
   
02/24/10 making the source file qicdcomp.c local and include this file instead
   of original msbackup file msqic.h. This seems to work with no changes here
    if I hide routine do_decompress() with conditional #ifdef MSQIC
   
*/  
    
#if defined(MSDOS) || defined(_WIN32)
#pragma pack(1)
// default for MSVC _WIN32 should be byte packing, but need pack(1)!
#define PACKED ;
#define DELIM '\\'
#else                           // Unix
#define DELIM '/'
#ifdef __CYGWIN__               // note  O_BINARY is defined
#pragma pack(1)                 // Linux gcc won't compile with this, see above
#define PACKED ;
#else   /*  */
// to pack linux structures selectively
#define PACKED __attribute__ ((packed));
#define O_BINARY 0              // this Microsoft mode flag undefined in Linux gcc
#endif  /*  */
#endif  /*  */
typedef unsigned char BYTE;
typedef unsigned short WORD;

#if __x86_64__
// 64-bit system
typedef unsigned int DWORD;

#else   /*  */
// 32 bit system
typedef unsigned long DWORD;

#endif  /*  */
     
#ifdef _4GB
typedef unsigned long FOFFSET;

#define LSEEK_ERR ((DWORD)-1L)
#else   /*  */
#ifdef HAS_INT64                // force this manually for 64 bit access
// for now WIN32 specific
typedef __int64 FOFFSET;

#define lseek _lseeki64         // redirect to 64 bit access routine
#define LSEEK_ERR (-1)          // this works for standard lseek stuff
#else   /*  */
typedef long FOFFSET;           // 2 GB is default std C
#define LSEEK_ERR (-1L)
#endif  /*  */
#endif  /*  */

#define SEG_SZ 29696L           // MSBackUP wants data and dir segs to be multiple of this
// in compressed file each segment including catalog start with cseg_head
     
// from pp9 of QIC113G, 
    struct qic_vtbl {
    BYTE tag[4];               // should be 'VTBL'
    DWORD nseg;                 // # of logical segments
    char desc[44];
     DWORD date;               // date and time created
    BYTE flag;                  // bitmap
    BYTE seq;                   // multi catridge sequence #
    WORD rev_major, rev_minor;  // revision numberrs
    BYTE vres[14];              // reserved for vendor extensions
    DWORD start, end;           // physical QFA block numbers
    /* In Win98 & ME subtract 3 from above for zero based SEGMENT index
       to start first data segment and start first directory segment
     */ 
     BYTE passwd[8];            // if not used, start with a 0 byte
    DWORD dirSz,                // size of file set directory region in bytes
     dataSz[2];                 // total size of data region in bytes
    BYTE OSver[2];              // major and minor #
    BYTE sdrv[16];              // source drive volume lable
    BYTE ldev,                  // logical dev file set originated from
     res,                       // should be 0
     comp,                      // compression bitmap, 0 if not used
     OStype,  res2[2];         // more reserved stuff
} PACKED     
/* If its a compressed volume there will be cseg_head
   records ahead of each segment.  The first immediately
   follows the Volume Table.  
   For the sake of argument, lets assume per QIC133 segments are
   supposed to be < 32K, ie seg_sz high order bit isn't required.
   So its a flag bit, set to indicate raw data?  IE do not 
   decompress this segment.  Use seg_sz to jump to the
   next segment header.
*/ 
#define RAW_SEG 0x8000          // flag for a raw data segment

// 1st cut at defining fields in *.113 headers at start of 1st and 2nd segment
    struct fhead113 {
    DWORD sig;                 // signature value should be 0xAA55AA55 definded in qicdcomp.h as IOHEAD_SIG
    WORD unkwn[3];              // 3 unknow words often {0xFF,0x2,0}
    DWORD blkcnt;               // appears to be a block count, not the same value in 1st & 2nd header, larger in 2nd
    DWORD t1;                   // a qic 113 date time value
    DWORD t2;                   // a qic 113 date time value   
    WORD unkwn2[4];             // unknown data could be 2 DWORDS just as easily  
    char desc[44];              // comment region, job # and disk in 1st header, empty in 2nd?
    DWORD t3;                   // a qic 113 date time value, often a few seconds later, time of last write?
    BYTE unkwn3[60];            // unknown, always zeros?
    DWORD t4;                   // a qic 113 date time value
    WORD unkwn4;                // unknown, often 0x1
} PACKED    struct cseg_head {
    DWORD cum_sz,              // cumlative uncompressed bytes at end this segment
    cum_sz_hi;                  // normally zero. High order DWORD of above for > 4Gb
    WORD seg_sz;                // physical bytes in this segment, offset to next header
} PACKED  
#define IOHEAD_SIG 0xAA55AA55   // segment signature of 1st to 113 file segment headers
#define DAT_SIG 0x33CC33CCL     // signature at start of Data Segment
#define EDAT_SIG 0x66996699L    // just before start of data file
#define UDEF_SIG 0xFFFFFFFFL    // undefined DWORDS in Data Segment

#define VTBL_DATE_FUDGE   63072000L     // date fudge for vtbl date  as of 3/4/18 see vtbl.c qictime() for solution
extern int copy_region(long);
 extern void decomp_seg();


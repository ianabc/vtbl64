#ifndef QIC_H
#define QIC_H 1

#include <stdint.h>

#define FHDR_SZ 0x90            /* File header size */
#define RAW_SEG 0x8000          /* Mask for detecting raw segment */

#define SEG_SZ 29696L           /* Segment size */
#define SEG_HD_SZ 3             /* Segment Header size in bytes */
#define MAX_SEG_SZ 1048576L       /* Maximal compressed size (QIC-113 Rev.G) */
#define HBUF_SZ 2048            /* History buffer */

#define DCOMP_MAX_ITERS 10       /* Maximums number of decompression iterations */

typedef uint8_t BYTE;
typedef uint16_t WORD;
typedef uint32_t DWORD;

extern int debug;

/*
 * When you have time, remove the packing and just populate each member
 * individually
 */
#pragma pack(push, 1)
typedef struct {
    DWORD sig;                  /* 0xAA55AA55 for valid files */
    WORD unkwn[3];              /* 3 unknown words often {0xFF,0x2,0} */
    DWORD blkcnt;               /* appears to be a block count, not the same value in 1st & 2nd header, larger in 2nd */
    DWORD t1;                   /* a qic 113 date time value */
    DWORD t2;                   /* a qic 113 date time value    */
    WORD unkwn2[4];             /* unknown data could be 2 DWORDS just as easily   */
    BYTE desc[44];              /* comment region, job # and disk in 1st header, empty in 2nd? */
    DWORD t3;                   /* a qic 113 date time value, often a few seconds later, time of last write? */
    BYTE unkwn3[60];            /* unknown, always zeros? */
    DWORD t4;                   /* a qic 113 date time value */
    WORD unkwn4;                /* unknown, often 0x1 */
} fhead113;
#pragma pack(pop)


/*
 * When you have time, remove the packing and just populate each member
 * individually
 */
#pragma pack(push, 1)
typedef struct {
    BYTE tag[4];                /* should be 'VTBL' */
    DWORD nseg;                 /* # of logical segments */
    BYTE desc[44];
    DWORD date;                 /* date and time created */
    BYTE flag;                  /* bitmap */
    BYTE seq;                   /* multi cartridge sequence # */
    WORD rev_major, rev_minor;  /* revision numbers */
    BYTE vres[14];              /* reserved for vendor extensions */
    DWORD start, end;           /* physical QFA block numbers */
    BYTE passwd[8];             /* if not used, start with a 0 byte */
    DWORD dirSz;                /* size of file set directory region in bytes */
    DWORD dataSz[2];            /* total size of data region in bytes */
    BYTE OSver[2];              /* major and minor # */
    BYTE sdrv[16];              /* source drive volume label */
    BYTE ldev;                  /* logical dev file set originated from */
    BYTE res;                   /* should be 0 */
    BYTE comp;                  /* compression bitmap, 0 if not used */
    BYTE OStype;
    BYTE res2[2];               /* more reserved stuff */
} vtbl113;
#pragma pack(pop)


/*
 * When you have time, remove the packing and just populate each member
 * individually
 */
#pragma pack(push, 1)
typedef struct {
    DWORD cum_sz;               /* cumulative uncompressed bytes at end this segment */
    DWORD cum_sz_hi;            /* normally zero. High order DWORD of above for > 4Gb */
    WORD seg_sz;                /* physical bytes in this segment, offset to next header */
} cseg_head;
#pragma pack(pop)


int getBit(BYTE *cbuf, unsigned int *bit_pos);

BYTE getByte(BYTE *cbuf, unsigned int *bit_pos);

fhead113 *getFHeader(FILE *fp);

vtbl113 *getVTBL(FILE *fp);

void displayVTBL(vtbl113 *vtbl);

cseg_head *getSegmentHeader(FILE * fp);

void getSegmentData(FILE *infp, BYTE *cbuf, unsigned int sn, unsigned int seg_sz);

unsigned int writeSegment(FILE *outfp, BYTE *dbuf, cseg_head *seg_head, unsigned int decomp_sz);
unsigned int decompressSegment(BYTE *cbuf, BYTE *dbuf, unsigned int seg_sz);
#endif

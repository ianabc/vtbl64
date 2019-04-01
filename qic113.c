#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <time.h>
#include "qic.h"

const char *flagbits[] = {
    "Vendor specific volume",
    "Volume spans multiple cartidges",
    "File sets written without verification",
    "Reserved (should not occur)",
    "Compressed data segment spaning",
    "File Set Directory follow data section"
};

const char *OStype[] = {
    "Extended",
    "DOS Basic",
    "Unix",
    "OS/2",
    "Novell Netware",
    "Windows NT",
    "DOS Extended",
    "Windows 95"
};


fhead113 *getFHeader(FILE * fp, unsigned int segment)
{
    fhead113 *hdr = NULL;
    unsigned long rd;

    if ((hdr = (fhead113 *) malloc(FHDR_SZ)) == NULL) {
        fprintf(stderr, "Failed to allocate space for header\n");
        exit(EXIT_FAILURE);
    }

    fseek(fp, segment * SEG_SZ, SEEK_SET);
    if (ftell(fp) != segment * SEG_SZ) { 
        fprintf(stderr, "Unable to seek to header segment: %d\n", segment);
        exit(EXIT_FAILURE);
    }
    if ((rd = fread(hdr, FHDR_SZ, 1, fp)) != 1) {
        fprintf(stderr, "Only read 0x%lx bytes of 0x%x byte header\n", rd,
                FHDR_SZ);
        exit(EXIT_FAILURE);
    }

    if (hdr->sig != 0xAA55AA55) {
        fprintf(stderr, "Invalid header signature 0x%x != 0xAA55AA55\n",
                hdr->sig);
        exit(EXIT_FAILURE);
    }

    return (hdr);
}


vtbl113 *getVTBL(FILE * fp)
{
    vtbl113 *vtbl = NULL;
    unsigned long sz, rd;

    sz = sizeof(vtbl113);
    if ((vtbl = (vtbl113 *) malloc(sz)) == NULL) {
        fprintf(stderr, "Failed to allocate space for vtbl\n");
        exit(EXIT_FAILURE);
    }
    
    fseek(fp, 2 * SEG_SZ, SEEK_SET);
    if (ftell(fp) != 2 * SEG_SZ) { 
        fprintf(stderr, "Unable to seek to vtbl header\n");
        exit(EXIT_FAILURE);
    }
    if ((rd = fread(vtbl, sz, 1, fp)) != 1) {
        fprintf(stderr, "Only read 0x%lx bytes of 0x%lx byte vtbl\n", rd,
                sz);
        exit(EXIT_FAILURE);
    }

    if (strncasecmp((const char *) vtbl->tag, "VTBL", 4) != 0) {
        fprintf(stderr,
                "Missing 'VTBL' tag, invalid record at offset 0x%lx ",
                ftell(fp));
        exit(EXIT_FAILURE);
    }

    return (vtbl);
}


void displayVTBL(vtbl113 * vtbl)
{
    unsigned long i, rd;
    char date[64];
    time_t timestamp;
    struct tm *tm;

    fprintf(stdout,
            "Label: %.44s  \nVTBL volume contains %u logical segments\n",
            vtbl->desc, vtbl->nseg);

    timestamp = (time_t) vtbl->date;
    tm = localtime(&timestamp);
    strftime(date, sizeof(date), "%m/%Y/%d %H:%M:%S", tm);
    fprintf(stdout, "created: %s\n", date);

    fprintf(stdout, "flag 0x%x:\n", vtbl->flag);
    for (i = 0, rd = 1; i < 5; i++) {
        if (rd & vtbl->flag) {
            fprintf(stdout, "\n%s", flagbits[i]);
            if (rd == 2)        /* its multi-volume, display seq */
                fprintf(stdout, "   sequence #: %d: ", vtbl->seq);
        }
        rd = rd << 1;
    }

    if ((vtbl->flag & 1) == 0) {        /* generic, not vendor specific */
        /*
         * fields after flag not valid if vendor specific
         * ignore quad word, assume vtbl->dataSz[1] == 0
         */
        fprintf(stdout, "version: %0x:%0x\n", vtbl->rev_major,
                vtbl->rev_minor);
        fprintf(stdout, "dir size 0x%x data size 0x%x\n", vtbl->dirSz,
                vtbl->dataSz[0]);
        fprintf(stdout, "QFA physical start block 0x%x end block 0x%x\n",
                vtbl->start, vtbl->end);
        fprintf(stdout, "compression byte 0x%x\n", vtbl->comp);
        if (vtbl->comp & 0x80)
            fprintf(stdout, "Compression used, type 0x%x\n",
                    vtbl->comp & 0x3f);
        if (vtbl->OStype < 8)
            fprintf(stdout, "OS type: d => %s\n", OStype[vtbl->OStype]);
    }
}


cseg_head* getSegmentHeader(FILE * fp, unsigned int sn)
{
    unsigned long sz, rd;
    cseg_head *seg_head;

    sz = sizeof(cseg_head);
    if ((seg_head = (cseg_head *) malloc(sz)) == NULL) {
        fprintf(stderr, "Failed to allocate space for cseg_head\n");
        exit(EXIT_FAILURE);
    }
    fseek(fp, sn * SEG_SZ, SEEK_SET);
    if (ftell(fp) != sn * SEG_SZ) {
        fprintf(stderr, "Unable to seek to compressed segment\n");
    }
    if ((rd = fread(seg_head, sz, 1, fp)) != 1) {
        fprintf(stderr, "Only read 0x%lx bytes of 0x%lx byte segment header\n", rd,
                sz);
        exit(EXIT_FAILURE);
    }

    return (seg_head);
}


void getSegmentData(FILE * infp, BYTE * cbuf, unsigned int sn) {
    /*
     * Read the whole segment, minus the header
     */
    unsigned long rd;
    fseek(infp, sn * SEG_SZ + (unsigned int)sizeof(cseg_head), SEEK_SET);
    if (ftell(infp) != sn * SEG_SZ + (unsigned int)sizeof(cseg_head)) {
        fprintf(stderr,
                "Unable to seek to compressed segment: %ld (%ud)\n",
                ftell(infp), sn * SEG_SZ + (unsigned int)sizeof(cseg_head));
        exit(EXIT_FAILURE);
    }

    if ((rd = fread(cbuf, SEG_SZ - (unsigned int)sizeof(cseg_head), 1, infp)) != 1) {
        fprintf(stderr,
                "Only read 0x%lx bytes of 0x%ux compressed segment\n", rd,
                SEG_SZ);
        exit(EXIT_FAILURE);
    }
}


unsigned int writeSegment(FILE *outfp, BYTE *dbuf, cseg_head *seg_head, 
        unsigned int decomp_rd, WORD seg_sz) {

    unsigned int wr = 0;
    /*
     * Header information for new decompressed data
     */
    wr += (unsigned int)fwrite(&(seg_head->cum_sz),    sizeof(seg_head->cum_sz),    1, outfp);
    wr += (unsigned int)fwrite(&(seg_head->cum_sz_hi), sizeof(seg_head->cum_sz_hi), 1, outfp);
    wr += (unsigned int)fwrite(&seg_sz, sizeof(seg_sz), 1, outfp);
    if (wr != 3)
        fprintf(stderr, "Failed to write segment header.\n");
    
    if ((wr += (unsigned int)fwrite(dbuf, 1, decomp_rd, outfp)) != (decomp_rd+3)) {
        fprintf(stderr, "Failed to write segment data.\n");
        exit(EXIT_FAILURE);
    }
    return wr;
}

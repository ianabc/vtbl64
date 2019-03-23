#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <strings.h>
#include <ctype.h>
#include <time.h>
#include "vtbl64.h"

static int debug = 0;

fhead113 *getFHeader(FILE * fp)
{
    fhead113 *hdr = NULL;
    unsigned int rd;

    if ((hdr = (fhead113 *) malloc(FHDR_SZ)) == NULL) {
        fprintf(stderr, "Failed to allocate space for header\n");
        exit(1);
    }
    if ((rd = fread(hdr, FHDR_SZ, 1, fp)) != 1) {
        fprintf(stderr, "Only read 0x%x bytes of 0x%x byte header\n", rd,
                FHDR_SZ);
        exit(1);
    }
    if (hdr->sig != 0xAA55AA55) {
        fprintf(stderr, "Invalid header signature 0x%x != 0xAA55AA55\n",
                hdr->sig);
        exit(1);
    }

    return (hdr);
}


vtbl113 *getVTBL(FILE * fp)
{
    vtbl113 *vtbl = NULL;
    unsigned int sz, rd;

    sz = sizeof(vtbl113);
    if ((vtbl = (vtbl113 *) malloc(sz)) == NULL) {
        fprintf(stderr, "Failed to allocate space for vtbl\n");
        exit(1);
    }
    if ((rd = fread(vtbl, sz, 1, fp)) != 1) {
        fprintf(stderr, "Only read 0x%x bytes of 0x%x byte vtbl\n", rd,
                sz);
        exit(1);
    }
    if (strncasecmp((const char *) vtbl->tag, "VTBL", 4) != 0) {
        fprintf(stderr,
                "Missing 'VTBL' tag, invalid record at offset 0x%lx ",
                ftell(fp));
        exit(1);
    }

    return (vtbl);
}


void displayVTBL(vtbl113 * vtbl)
{
    unsigned int i, rd;
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


cseg_head* getSegmentHeader(FILE * fp)
{
    unsigned int sz, rd;
    cseg_head *seg_head;

    sz = sizeof(cseg_head);
    if ((seg_head = (cseg_head *) malloc(sz)) == NULL) {
        fprintf(stderr, "Failed to allocate space for cseg_head\n");
        exit(1);
    }
    if ((rd = fread(seg_head, sz, 1, fp)) != 1) {
        fprintf(stderr, "Only read 0x%x bytes of 0x%x byte vtbl\n", rd,
                sz);
        exit(1);
    }

    return (seg_head);
}


void getSegmentData(FILE * infp, BYTE * cbuf, unsigned int sn,
                 unsigned int seg_sz)
{
    /*
     * Read the whole segment, header included
     */
    unsigned int rd;

    fseek(infp, (sn + 3) * SEG_SZ, SEEK_SET);
    if (ftell(infp) != (sn + 3) * SEG_SZ) {
        fprintf(stderr,
                "Unable to seek to compressed segment: %ld (%ld)\n",
                ftell(infp), (sn + 3) * SEG_SZ);
        exit(1);
    }

    if ((rd = fread(cbuf, SEG_SZ, 1, infp)) != 1) {
        fprintf(stderr,
                "Only read 0x%x bytes of 0x%lx compressed segment\n", rd,
                SEG_SZ);
        exit(1);
    }
}


int getBit(BYTE *cbuf, unsigned int *bit_pos)
{
    /*
     * Get the value of the single bit at position bit_pos. This treats the
     * entire data region as a bit string.
     *
     */
    unsigned char byte, shift;

    shift = (8 - *bit_pos % 8) - 1;
    byte = cbuf[*bit_pos / 8];

    (*bit_pos)++;
    return (byte & ( 1 << shift )) >> shift;
}


BYTE getByte(BYTE *cbuf, unsigned int *bit_pos)
{
    int i, ret;
    
    for(i = 0, ret=0; i < 8; i++) {
       ret = (ret << 1) + getBit(cbuf, bit_pos);
    }
    if((debug > 2) && (ret > ' '))
        fprintf(stderr, "%c ", ret);
    if(debug > 2) fprintf(stderr, "0x%x ", ret);

    return ret;
}


unsigned int decompressSegment(BYTE * cbuf, BYTE * dbuf, unsigned int seg_sz)
{
    /*
     * Decompress binary data according to QIC-122
     *
     * Start of data occurs at the third byte of cbuf
     * End of data marked by 0x180 within 18 bytes of end of cbuf, right padded
     * with zeros to fill seg_sz should match the difference of these values
     * Scan backward from the end for marker then check size
     */

    /*
     * Scan bits for 0x180
     */
    unsigned int bit_pos;
    unsigned int off, off_bits, len, nibble;
    unsigned int i, j;
    int inraw = 0;
    
    unsigned int didx = 0;
    /*
     * Skip over header to 10th byte
     */
    bit_pos = 80;


    while(1) {
        if (getBit(cbuf, &bit_pos) == 0) {
            /* Raw Byte */
            if ( (debug > 2) && (!inraw) ) 
                fprintf(stderr, "\nRaw ") && (inraw = 1);
            if(debug > 2) fprintf(stderr, "[%d]", didx);
            dbuf[didx++] = getByte(cbuf, &bit_pos);
        }
        else {
            /* A String */
            inraw = 0;
            if (getBit(cbuf, &bit_pos) == 0) {
               off_bits = 11;
            }
            else
               off_bits = 7;


            for(i = 0, off = 0; i < off_bits; i++) 
               off = (off << 1) + getBit(cbuf, &bit_pos);

            if ((off_bits == 7) && (off == 0)) {
                /* End of compression marker is 110000000 
                 * i.e. A string with a 7 bit offset of zero
                 */
                if(debug > 2) fprintf(stderr, "\nEnd of compression marker found at %d\n", didx);
                break;
            }

            /* Examine the length of the match
             * 00 - length 2
             * 01 - length 3
             * 10 - length 4
             * 11 00 - length 5
             * 11 01 - length 6
             * 11 10 - length 7
             * 11 11 0000 - length 8
             * 11 11 0001 - length 9
             * ...
             * 11 11 1110 - length 22
             * 11 11 1111 0000 - length 23
             * ...
             *
             * So for strings longer than 7, just check if the last 4 bits are less
             * than 15.
             */
            len = 2;
            for (j = 0; j < 2; j++) {
                nibble = 0;
                for (i = 0; i < 2; i++) {
                    nibble = (nibble << 1) + getBit(cbuf, &bit_pos);
                }
                if (nibble < 3) {
                    len += nibble;
                    break;
                }
                else
                    len += 3;
            }
            if (len == 8) {
                while (1) {
                    nibble = 0;
                    for (i = 0; i < 4; i++) {
                        nibble = (nibble << 1) + getBit(cbuf, &bit_pos);
                    }
                    if (nibble < 15) {
                        len += nibble;
                        break;
                    }
                    else
                        len += 15;
                    j++;
                }
            }

            if(debug > 2) fprintf(stderr, "\nString: len = %u offset %u, sending %u to %d", 
                    len, off, didx, didx - off);

            for (i = 0; i < len; i++, didx++) {
                dbuf[didx] = dbuf[(didx - off)];
                if(debug > 2) fprintf(stderr, " 0x%x", dbuf[didx]);
            }
        }
    }
    return didx;
}


unsigned int writeSegment(FILE *outfp, BYTE *dbuf, cseg_head *seg_head, unsigned int decomp_sz) {

    unsigned int wr = 0;
    /*
     * Header information for new decompressed data
     */
    wr += fwrite(&(seg_head->cum_sz),    sizeof(seg_head->cum_sz),    1, outfp);
    wr += fwrite(&(seg_head->cum_sz_hi), sizeof(seg_head->cum_sz_hi), 1, outfp);
    wr += fwrite(&(seg_head->seg_sz),    sizeof(seg_head->seg_sz),    1, outfp);

    if (wr != 3)
        fprintf(stderr, "Failed to write segment header.\n");
    
    if ((wr = fwrite(dbuf, seg_head->seg_sz, 1, outfp)) != 1) {
        fprintf(stderr, "Only wrote 0x%x bytes of 0x%x byte buffer\n", wr, seg_head->seg_sz);
        exit(1);
    }

    return (wr + 3);
    
}



int main(int argc, char **argv)
{

    FILE *infp, *outfp;

    fhead113 *fhead1, *fhead2;
    vtbl113 *vtbl;
    cseg_head *seg_head;
    BYTE *cbuf;
    BYTE *dbuf;
    unsigned int sn = 0;
    unsigned int lseg_sz, decomp_rd;
    unsigned int decomp_sz = 0;
    int c;

    while((c = getopt(argc, argv, "d")) != -1) {
        switch(c) {
            case 'd':
                debug++;
                break;
            default:
                exit(1);
        }
    }

    if (!(infp = fopen("../Image.113", "rb"))) {
        fprintf(stderr, "Can't open input file '../Image.113'\n");
        exit(1);
    }

    fhead1 = getFHeader(infp);

    fseek(infp, SEG_SZ, SEEK_SET);
    if (ftell(infp) != SEG_SZ) {
        fprintf(stderr, "Unable to seek to second header: %ld (%ld)\n",
                ftell(infp), SEG_SZ);
        exit(1);
    }
    fhead2 = getFHeader(infp);



    fseek(infp, 2 * SEG_SZ, SEEK_SET);
    if (ftell(infp) != 2 * SEG_SZ) { fprintf(stderr, "Unable to seek to vtbl\n");
    }
    vtbl = getVTBL(infp);
    displayVTBL(vtbl);

    /*
     * Iterate through segments
     */
    if (vtbl->comp) {
        fprintf(stderr, "File is compressed\n");

        if (!(outfp = fopen("dcomp.out", "wb"))) {
            fprintf(stderr, "Can't ouput dcomp.out for output\n");
            exit(1);
        }
        /**
         * Maybe dump the existing headers here, but with the compression flag
         * off
         */
        if ((cbuf = (BYTE *) malloc(SEG_SZ)) == NULL) {
            fprintf(stderr,
                    "Failed to allocate space for compressed buffer\n");
            exit(1);
        }
        if ((dbuf = (BYTE *) malloc(MAX_SEG_SZ)) == NULL) {
            fprintf(stderr,
                    "Failed to allocate space for uncompressed buffer\n");
            exit(1);
        }
        while (sn < fhead2->blkcnt) {

            fseek(infp, (3 + sn) * SEG_SZ, SEEK_SET);
            if (ftell(infp) != (3 + sn) * SEG_SZ) {
                fprintf(stderr, "Unable to seek to compressed segment\n");
            }
            seg_head = getSegmentHeader(infp);
            fprintf(stderr, "Reading compressed segment %d, %u, %u, %u\n",
                    sn, seg_head->cum_sz, seg_head->cum_sz_hi,
                    seg_head->seg_sz);

            if (sn != 0 && seg_head->cum_sz == 0
                && seg_head->cum_sz_hi == 0) {
                fprintf(stderr, "Catalog found in segment %u\n", sn);
                break;
            }

            getSegmentData(infp, cbuf, sn, seg_head->seg_sz);
            decomp_rd = decompressSegment(cbuf, dbuf, seg_head->seg_sz);
            
            /*
             * The total size decompressed should match the header of in the
             * next compressed segment (eventually we need to care about
             * overflows here for 4GB+ archives.
             *
             * We will need to:
             *   1. split dbuf into new 32k segments
             *   2. Create a new segment header
             *   3. Write Segment header and data
             *
             */
            decomp_sz += decomp_rd;
            writeSegment(outfp, dbuf, seg_head, decomp_rd);

            lseg_sz += decomp_rd;


            if (seg_head->seg_sz & RAW_SEG) {
                fprintf(stderr, "Raw Segment, not handled\n");
                exit(1);
            }
            sn++;
            free(seg_head);
        }
        free(cbuf);
        free(dbuf);
        /*
         * Still have the catalog to deal with
         */
    } else {
        fprintf(stderr, "File is not compressed\n");
    }

    free(fhead1);
    free(fhead2);
    free(vtbl);
    fclose(infp);
    fclose(outfp);

    return EXIT_SUCCESS;
}

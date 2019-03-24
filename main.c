#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include "qic.h"


int debug = 0;


int main(int argc, char **argv)
{

    FILE *infp, *outfp;

    fhead113 *fhead1, *fhead2;
    vtbl113 *vtbl;
    cseg_head *seg_head, *next_seg_head;
    BYTE *cbuf;
    BYTE *dbuf;
    unsigned int sn = 0;
    unsigned long decomp_sz = 0;
    unsigned int decomp_rd, decomp_target;
    int c, pass, i;

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
        
        if ((cbuf = (BYTE *) malloc(MAX_SEG_SZ)) == NULL) {
            fprintf(stderr,
                    "Failed to allocate space for compressed buffer\n");
            exit(1);
        }
        if ((dbuf = (BYTE *) malloc(MAX_SEG_SZ)) == NULL) {
            fprintf(stderr,
                    "Failed to allocate space for uncompressed buffer\n");
            exit(1);
        }

        /*
         * Main decompression loop. Iterate over data segments, decompress and
         * write to file.
         */
        while (sn < fhead2->blkcnt) {

            /*
             * Collect the next two segment headers. Each segment header tells
             * you the cumulative count of decompressed bytes seen in all of
             * the previous segments. This means that we need the current and
             * the _next_ segment headers to figure out how many bytes we
             * should be expecting.
             */
            fseek(infp, (3 + sn) * SEG_SZ, SEEK_SET);
            if (ftell(infp) != (3 + sn) * SEG_SZ) {
                fprintf(stderr, "Unable to seek to compressed segment\n");
            }
            seg_head = getSegmentHeader(infp);

            fseek(infp, (3 + sn + 1) * SEG_SZ, SEEK_SET);
            if (ftell(infp) != (3 + sn + 1) * SEG_SZ) {
                fprintf(stderr, "Unable to seek to next compressed segment\n");
            }
            next_seg_head = getSegmentHeader(infp);
            decomp_target = ((next_seg_head->cum_sz_hi * UINT32_MAX + next_seg_head->cum_sz) - 
                    (seg_head->cum_sz_hi * UINT32_MAX + seg_head->cum_sz));


            fprintf(stderr, "Reading compressed segment %d, %u, %u, %u\n",
                    sn, seg_head->cum_sz, seg_head->cum_sz_hi,
                    seg_head->seg_sz);

            if (sn != 0 && seg_head->cum_sz == 0
                && seg_head->cum_sz_hi == 0) {
                fprintf(stderr, "Catalog found in segment %u\n", sn);
                break;
            }

            getSegmentData(infp, cbuf, sn, seg_head->seg_sz);

            pass = 0;
            decomp_rd = 0;
            while (( decomp_rd < decomp_target ) && ( pass < DCOMP_MAX_ITERS )) {

                if (debug > 1) fprintf(stderr, "Decompressing pass %d,", pass);
                decomp_rd = decompressSegment(cbuf, dbuf, decomp_rd);

                /* Fake a 10 byte header */
                for(i = 0; i < 10; i++)
                    cbuf[i] = '\0';

                for(i = 10; i < (MAX_SEG_SZ-10); i++) 
                    cbuf[i] = dbuf[i-10];

                pass++;
                if (debug > 1) fprintf(stderr, " decomp_rd %d of target %d\n",
                        decomp_rd, decomp_target);

            } 

            decomp_sz += decomp_rd;
            writeSegment(outfp, dbuf, seg_head, decomp_rd);

            if (seg_head->seg_sz & RAW_SEG) {
                fprintf(stderr, "Raw Segment, not handled\n");
                exit(1);
            }
            sn++;
            free(seg_head);
            free(next_seg_head);
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

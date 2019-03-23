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
    cseg_head *seg_head;
    BYTE *cbuf;
    BYTE *dbuf;
    unsigned int sn = 0;
    unsigned int lseg_sz, decomp_rd;
    unsigned long decomp_sz = 0;
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

        /*
         * Main decompression loop. Iterate over data segments, decompress and
         * write to file.
         */
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
            /*
             * Will says this should be iterated on. Check the decompressed
             * bytes count against the value in the header of the next segment.
             */
            if (decomp_sz == (seg_head->cum_sz_hi * UINT32_MAX + seg_head->cum_sz)) {
                decomp_rd = decompressSegment(cbuf, dbuf, seg_head->seg_sz);
                decomp_sz += decomp_rd;
            }
            else {
                fprintf(stderr, "Decompression of previous segment (%d) failed: wanted %d, decompressed %ld.\n",
                        sn - 1, decomp_rd, decomp_sz - decomp_rd);
                exit(1);
            }

            /*
             * The total size decompressed should match the header of in the
             * next compressed segment (eventually we need to care about
             * overflows here for 4GB+ archives.
             *
             * We will need to:
             *   1. split dbuf into new 32k segments
             *   2. Create a new segment header
             *   3. Write Segment header and data
             *   4. Pad with zeros
             *
             * We will be lazy with the dbuf split and just break the result of
             * a call to decompressSegment into as many new segments as
             * necessary, i.e. don't worry about wasting space by trying to
             * stitch together segments.
             *
             */
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

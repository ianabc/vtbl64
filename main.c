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
    WORD lseg_sz, cum_seg_sz;
    unsigned int sn = 0;
    int startsn = -1, endsn = -1;
    unsigned long decomp_sz = 0;
    unsigned int rd, decomp_rd, decomp_target;
    int c, cframe;

    while((c = getopt(argc, argv, "ds:t:")) != -1) {
        switch(c) {
            case 'd':
                debug++;
                break;
            case 's':
                startsn = atoi(optarg);
                break;
            case 't':
                endsn = atoi(optarg);
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
        
        if ((cbuf = (BYTE *) calloc(MAX_SEG_SZ, 1)) == NULL) {
            fprintf(stderr,
                    "Failed to allocate space for compressed buffer\n");
            exit(1);
        }
        if ((dbuf = (BYTE *) calloc(MAX_SEG_SZ, 1)) == NULL) {
            fprintf(stderr,
                    "Failed to allocate space for uncompressed buffer\n");
            exit(1);
        }

        /*
         * Main decompression loop. Iterate over data segments, decompress and
         * write to file.
         */
        if (startsn > 0) sn = startsn;
        if (endsn < 0) endsn = fhead2->blkcnt;

        while (sn < endsn) {

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
            if (sn != 0 && next_seg_head->cum_sz == 0
                && next_seg_head->cum_sz_hi == 0) {
                fprintf(stderr, "Catalog found in segment %u\n", sn);
                decomp_target = UINT32_MAX;
            }
            else {
                decomp_target = ((next_seg_head->cum_sz_hi * (UINT32_MAX + 1) + next_seg_head->cum_sz) - 
                     (seg_head->cum_sz_hi * (UINT32_MAX + 1) + seg_head->cum_sz));
            }


            fprintf(stderr, "Reading compressed segment %d, %u, %u, %u\n",
                    sn, seg_head->cum_sz, seg_head->cum_sz_hi,
                    seg_head->seg_sz);

            getSegmentData(infp, cbuf, sn, seg_head->seg_sz);

            decomp_rd = 0;
            /*
             * Some segments have more than one compressed block. They are
             * concatenated one after the other.
             */
            cum_seg_sz = seg_head->seg_sz;

            cframe = 1;
            decomp_rd = decompressSegment(cbuf, dbuf, seg_head->seg_sz);
            
            /*
             * Handle any additonal compressed frames
             */
            while (( decomp_rd < decomp_target ) && ( cframe < DCOMP_MAX_EXTENTS )) {
                
                cframe++;

                /* Seek to new seg_sz marker and read */
                fseek(infp, (3 + sn) * SEG_SZ + sizeof(*seg_head) + cum_seg_sz, SEEK_SET);
                if (ftell(infp) != (3 + sn) * SEG_SZ + sizeof(*seg_head) + cum_seg_sz) {
                    fprintf(stderr, "Unable to seek to next compressed seg_sz\n");
                }
                if ((rd = fread(&lseg_sz, 2, 1, infp)) != 1) {
                    fprintf(stderr, "Only read 0x%x bytes of 0x%x byte seg_sz\n", rd, 2);
                    exit(1);
                }
                /* 
                 * lseg_sz can be zero if this is the last compressed
                 * segment, because we don't know next_seg_head->cum_sz. If
                 * it is zero, we are done.
                 */
                if (lseg_sz == 0) {
                    fprintf(stderr, "lseg_sz set to zero. end decompress loop\n");
                    decomp_target = decomp_rd;
                    break;
                }

                if (debug) fprintf(stderr, "Decompress frame %d at 0x%lx with new lseg_sz 0x%x at 0x%x \n",
                        cframe, (3 + sn) * SEG_SZ + sizeof(seg_head) + cum_seg_sz + sizeof(lseg_sz), 
                        lseg_sz, cum_seg_sz);
                
                decomp_rd += decompressSegment(&cbuf[cum_seg_sz + sizeof(lseg_sz)], &dbuf[decomp_rd], lseg_sz);
                cum_seg_sz += lseg_sz + sizeof(lseg_sz);
            }
            if ((lseg_sz != 0) && (decomp_rd != decomp_target)) {
                fprintf(stderr, "Segment %d: Decompressed failed. Wanted %d got %d in %d frames\n",
                        sn, decomp_target, decomp_rd, cframe);
                exit(1);
            }
            else 
                if (debug) fprintf(stderr, "Segment %d: Decompressed %d of %d in %d frames\n",
                        sn, decomp_rd, decomp_target, cframe);

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

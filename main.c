#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include "qic.h"

extern char *__progname;

static void usage(void);

int debug = 0;

int main(int argc, char **argv)
{

    FILE *infp, *outfp;

    char *infilename = NULL;
    char *outfilename = "dcomp.out";

    int overwrite = 0;

    fhead113 *fhead1, *fhead2;
    fhead113 fhead1_out, fhead2_out;
    vtbl113 *vtbl, vtbl_out;
    cseg_head *seg_head, *next_seg_head;
    BYTE *cbuf;
    BYTE *dbuf;
    unsigned int sn = 0;
    int startsn = -1, endsn = -1;
    unsigned long decomp_sz = 0, decomp_wr_sz = 0;
    unsigned long data_wr_sz = 0, dir_wr_sz = 0;
    unsigned int decomp_rd, decomp_wr, decomp_target;
    int incatalog = 0;
    int c;

    while((c = getopt(argc, argv, "dfi:o:s:t:")) != -1) {
        switch(c) {
            case 'd':
                debug++;
                break;
            case 'i':
                infilename = optarg;
                break;
            case 'o':
                outfilename = optarg;
                break;
            case 'f':
                overwrite = 1;
                break;
            case 's':
                startsn = atoi(optarg);
                break;
            case 't':
                endsn = atoi(optarg);
                break;
            case 'h':
                usage();
                break;
            case '?':
            default:
                fprintf(stderr, "%s: option '-%c' is invalid: ignored\n",
                        argv[0], optopt);
                break;
        }
    }
    
    if (infilename == NULL) {
        fprintf(stderr, "No input file specified\n");
        usage();
    }

    if (!(infp = fopen(infilename, "rb"))) {
        fprintf(stderr, "Can't open input file '%s'\n", infilename);
        exit(EXIT_FAILURE);
    }

    sn = 0;
    fhead1 = getFHeader(infp, sn++);
    fhead2 = getFHeader(infp, sn++);
    vtbl = getVTBL(infp);
    sn++;
    
    displayVTBL(vtbl);

    /*
     * Iterate through segments
     */
    if (vtbl->comp) {

        fprintf(stderr, "File is compressed\n");

        if ( (access(outfilename, R_OK) != -1) && !(overwrite)) {
            fprintf(stderr, "Output file \"%s\" exists! Remove it or use -f to overwrite!\n", 
                    outfilename);
            exit(EXIT_FAILURE);
        }
        else {
            if (!(outfp = fopen(outfilename, "wb"))) {
                fprintf(stderr, "Can't ouput dcomp.out for output\n");
                exit(EXIT_FAILURE);
            }
           
            /*
             * Zero block counts and sizes, we will update these headers once
             * we know more about the archive data.
             */
            fhead1_out = *fhead1;
            fhead1_out.blkcnt = 0;
            decomp_wr_sz += writeFHeader(outfp, &fhead1_out, 0);
            decomp_wr_sz += zeroPadSegment(outfp, decomp_wr_sz);

            fhead2_out = *fhead2;
            fhead2_out.blkcnt = 0;
            decomp_wr_sz += writeFHeader(outfp, &fhead2_out, 1);
            decomp_wr_sz += zeroPadSegment(outfp, decomp_wr_sz);

            vtbl_out = *vtbl;
            vtbl_out.end = 0;
            vtbl_out.dirSz = 0;
            vtbl_out.dataSz[0] = vtbl_out.dataSz[1] = 0;
            decomp_wr_sz += writeVTBL(outfp, &vtbl_out, 2);
            decomp_wr_sz += zeroPadSegment(outfp, decomp_wr_sz);
        }

        if ((cbuf = (BYTE *) calloc(SEG_SZ, 1)) == NULL) {
            fprintf(stderr,
                    "Failed to allocate space for compressed buffer\n");
            exit(EXIT_FAILURE);
        }
        if ((dbuf = (BYTE *) calloc(MAX_SEG_SZ, 1)) == NULL) {
            fprintf(stderr,
                    "Failed to allocate space for uncompressed buffer\n");
            exit(EXIT_FAILURE);
        }

        /*
         * Main decompression loop. Iterate over data segments, decompress and
         * write to file.
         */
        if (startsn > 0) sn = startsn;
        if (endsn < 0) endsn = fhead1->blkcnt;

        while (sn < endsn) {

            /*
             * Collect the next two segment headers. Each segment header tells
             * you the cumulative count of decompressed bytes seen in all of
             * the previous segments. This means that we need the current and
             * the _next_ segment headers to figure out how many bytes we
             * should be expecting.
             */
            seg_head = getSegmentHeader(infp, sn);
            next_seg_head = getSegmentHeader(infp, sn+1);

            if (sn != 0 && seg_head->cum_sz == 0 && seg_head->cum_sz_hi == 0)
                incatalog = 1;

            if (sn != 0 && next_seg_head->cum_sz == 0 && next_seg_head->cum_sz_hi == 0) {
                if (sn < endsn - 1)
                    if (debug) fprintf(stderr, "Catalog found in next segment %u\n", sn);

                /*
                 * We don't really have a target here since we don't have the
                 * total decompressed size available.
                 */
                decomp_target = UINT32_MAX;
            }
            else {
                decomp_target = (next_seg_head->cum_sz_hi * (UINT32_MAX + 1) + next_seg_head->cum_sz) - 
                     (seg_head->cum_sz_hi * (UINT32_MAX + 1) + seg_head->cum_sz);
            }


            fprintf(stderr, "Reading compressed segment %d, %u, %u\n",
                    sn, seg_head->cum_sz, seg_head->cum_sz_hi);

            getSegmentData(infp, cbuf, sn);

            decomp_rd = decompressExtent(cbuf, dbuf);
            decomp_sz += decomp_rd;
            
            if (debug) fprintf(stderr, "Decompress: Expected %u, produced %u\n", decomp_target, decomp_rd);

            decomp_wr = writeSegment(outfp, dbuf, decomp_rd);
            decomp_wr_sz += decomp_wr;
            if(incatalog)
                dir_wr_sz += decomp_wr;
            else
                data_wr_sz += decomp_wr;

            /*
            if (seg_head->seg_sz & RAW_SEG) {
                fprintf(stderr, "Raw Segment, not handled\n");
                exit(EXIT_FAILURE);
            }
            */
            sn++;
            free(seg_head);
            free(next_seg_head);
        }
        free(cbuf);
        free(dbuf);
        /*
         * Still have the catalog to deal with
         * Check decomp_wr_sz and zero pad to the next 0x7400 boundary
         */
        fprintf(stderr, "%lu\n", decomp_wr_sz);
        decomp_wr = zeroPadSegment(outfp, decomp_wr_sz);
        decomp_wr_sz += decomp_wr;

        fhead1_out.blkcnt = decomp_wr_sz / SEG_SZ;
        decomp_wr = writeFHeader(outfp, &fhead1_out, 0);
        decomp_wr_sz += decomp_wr;
        decomp_wr = zeroPadSegment(outfp, decomp_wr_sz);
        decomp_wr_sz += decomp_wr;

        fhead2_out.blkcnt = decomp_wr_sz / SEG_SZ;
        decomp_wr = writeFHeader(outfp, &fhead2_out, 1);
        decomp_wr_sz += decomp_wr;
        decomp_wr = zeroPadSegment(outfp, decomp_wr_sz);
        decomp_wr_sz += decomp_wr;

        vtbl_out.end = sn - 1;
        vtbl_out.dirSz = dir_wr_sz;
        vtbl_out.dataSz[0] = data_wr_sz % (UINT32_MAX);
        vtbl_out.dataSz[1] = data_wr_sz / (UINT32_MAX);
        decomp_wr = writeVTBL(outfp, &vtbl_out, 2);
        decomp_wr_sz += decomp_wr;
        decomp_wr = zeroPadSegment(outfp, decomp_wr_sz);
        decomp_wr_sz += decomp_wr;

        fclose(outfp);
    } else {
        fprintf(stderr, "File is not compressed\n");
    }

    fprintf(stderr, "File successfully decompressed\n");

    free(fhead1);
    free(fhead2);
    free(vtbl);
    fclose(infp);

    return EXIT_SUCCESS;
}


static void usage(void)
{
    fprintf(stderr, "Usage: %s -i <input file> [OPTIONS...] \n", __progname);
    fprintf(stderr, "   OPTIONS\n");
    fprintf(stderr, "      -i <file>\n");
    fprintf(stderr, "            Specify the input file. This option is MANDATORY.\n");
    fprintf(stderr, "      -o <file>\n");
    fprintf(stderr, "            Specify the output file. The default is a file called 'dcomp.out'\n");
    fprintf(stderr, "            in the current working directory.\n");
    fprintf(stderr, "      -s N\n");
    fprintf(stderr, "            Start decompressing at data segment N. Data segments are counted\n");
    fprintf(stderr, "            from zero beginning after after the two header segments and the\n");
    fprintf(stderr, "            Default is the first segment, i.e `-s 0`.\n");
    fprintf(stderr, "      -f\n");
    fprintf(stderr, "            Overwrite existing output. %s will refuse to overwrite existing\n",
            __progname);
    fprintf(stderr, "            files unless this option is specified. If you are not interested\n");
    fprintf(stderr, "            in the file output, try `-o /dev/null -f`.\n");
    fprintf(stderr, "      -t N\n");
    fprintf(stderr, "            Stop compressing at data segment N. This is exclusive, so N-1 will\n");
    fprintf(stderr, "            be the last segment to decompressed. The default is to decompress\n");
    fprintf(stderr, "            decompress all remaining extents in archive.\n");
    fprintf(stderr, "      -d\n");
    fprintf(stderr, "            Debug output. This option may be specified multiple times to\n");
    fprintf(stderr, "            increase verbosity.\n");
    fprintf(stderr, "      -h\n");
    fprintf(stderr, "            Display this message\n");

    exit(EXIT_FAILURE);
}


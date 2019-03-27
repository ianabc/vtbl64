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

    while((c = getopt(argc, argv, "dfi:s:t:")) != -1) {
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

    fhead1 = getFHeader(infp);

    fseek(infp, SEG_SZ, SEEK_SET);
    if (ftell(infp) != SEG_SZ) {
        fprintf(stderr, "Unable to seek to second header: %ld (%ld)\n",
                ftell(infp), SEG_SZ);
        exit(EXIT_FAILURE);
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
                    exit(EXIT_FAILURE);
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
                exit(EXIT_FAILURE);
            }
            else 
                if (debug) fprintf(stderr, "Segment %d: Decompressed %d of %d in %d frames\n",
                        sn, decomp_rd, decomp_target, cframe);

            decomp_sz += decomp_rd;
            writeSegment(outfp, dbuf, seg_head, decomp_rd);

            if (seg_head->seg_sz & RAW_SEG) {
                fprintf(stderr, "Raw Segment, not handled\n");
                exit(EXIT_FAILURE);
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


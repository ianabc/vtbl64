#include <stdio.h>
#include <stdlib.h>
#include "qic.h"

unsigned int decompressExtent(BYTE *cbuf, BYTE *dbuf) {
    /*
     * Decompress the contents of cbuf into dbuf.
     *
     * A compressed extent is composed of one or more compressed frames
     * concatenated one after the other in cbuf.
     *
     * Each compressed frame is prefixed by a 2 byte segment size seg_sz,
     * followed by seg_sz bytes of data compressed using the algorithm
     * described in QIC-122b.
     
     * After reading each frame, if there are more than 18 bytes remaining in
     * the segment, read the next two bytes. If they are non-zero, they give
     * the seg_sz for another compression frame in this segment.
     *
     */

    unsigned int decomp_sz = 0, decomp_frame_rd = 0;
    unsigned int comp_sz = 0, comp_rd = 0;
    unsigned int frame = 0;

    /*
     * Treat the first two bytes of cbuf as a DWORD segment size
     */
    while((comp_sz = (DWORD)cbuf[comp_rd] | (DWORD)(cbuf[comp_rd + 1] << 8)) != 0) {

        comp_rd += comp_sz + 2;

        if (debug > 1) fprintf(stderr, 
                "Decompress frame in %u, %u in, %u decompressed so far\n", 
                frame, comp_sz, decomp_sz);

        decomp_frame_rd = decompressFrame(&(cbuf[comp_rd - comp_sz]), 
                &(dbuf[decomp_sz]), comp_sz);
        decomp_sz += decomp_frame_rd;

        if (debug > 1) fprintf(stderr, "Decompress frame out %u, %u, in, %u out, %u total\n", 
                frame, comp_sz, decomp_frame_rd, decomp_sz);
        frame++;

    }

    return decomp_sz;
}


unsigned int decompressFrame(BYTE * cbuf, BYTE * dbuf, unsigned int seg_sz)
{
    /*
     * Decompress binary data according to QIC-122b
     *
     * cbuf should be positioned at the start of the compressed data. It will
     * contain a continuous stream of variable bit-width fields of three types
     *
     * 1. Raw byte: 0[0-1]{8}
     * 2. Compressed String:
     *   a) 11[0-1]{7}([0-1]+), 7 byte offset, length $1
     *   b) 10[0-1]{11}([0-1]+), 11 byte offset, length $1
     * 3. End of compression marker: 110000000 (0x180)
     *
     * The length field of 2 uses a peculiar encoding which is described in the
     * README.md document.
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
    bit_pos = 0;


    while(1) {
        if (getBit(cbuf, &bit_pos) == 0) {
            /* Raw Byte */
            if ( (debug > 2) && (!inraw) ) 
                fprintf(stderr, "\nRaw ") && (inraw = 1);
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
                if(debug > 2) fprintf(stderr, "\nEnd of compression marker found at %d (0x%x)\n", didx, didx);
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

    /*
     * Check we read the entire frame, i.e. ciel(bit_pos/8) == seg_sz
     */
    if (1 + ((bit_pos - 1) / 8) != seg_sz) {
        fprintf(stderr, "Decompress frame error, target %u, bit_pos %u\n", seg_sz, bit_pos);
        exit(EXIT_FAILURE);
    }
    return didx;
}

#include <stdio.h>
#include "qic.h"

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

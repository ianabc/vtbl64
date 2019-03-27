#include <stdio.h>
#include "qic.h"


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

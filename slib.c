/*
   slib.c  -  1st try at parsing a *.lib file found in some Iomega installation files
   See www.willsworks.net/file-format/iomega-1-step-backup  for full project description
    
    Copyright (C) 2017  William T. Kranz

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.   
    
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

2/8/17 last modified
*/

#include <stdio.h>
#include <time.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>     // for malloc() and exit() to surpress warning about implicit declaration
#include <ctype.h>      // for tolower
#if  defined(MSDOS) || defined(_WIN32)
# ifdef MSDOS
#  define OS_STR "MSDOS"
# else
#  define OS_STR "WIN32"
# endif
#define IS_UNIX 0
#include <io.h>         // for lseek
#include <sys\types.h>
#include <sys\stat.h>       // this include must follow types.h for MSC
#include <sys\utime.h>
#include <direct.h>     // for mkdir()
#else
// its a unix like environment
#define O_BINARY 0      // required for DOS, this makes that bit Linux compatible
#define IS_UNIX 1
#define OS_STR "LINUX"
#include <unistd.h>     // for lseek, read, write
#include <sys/stat.h>
#include <sys/types.h>
#include <utime.h>

#define strnicmp strncasecmp
#define stricmp  strcasecmp
#endif

/* http://www.sandersonforensics.com/Files/A%20brief%20history%20of%20time%20stamps.pdf
   as pointed out in above sec below only has 2 sec resolution and to display presumably multiply by 2

*/
struct tmap {
    unsigned sec:5,     /* time bit map */
     min:6, hr:5;
};


struct dmap {
    unsigned day:5,     /* date bit map */
     mon:4, yr:6;
};


#define VERBOSE 1

#define BUF_SZ 0x400
unsigned char buf[BUF_SZ];

int main(int argc, char *argv[])
{
    int cnt = 1, fp, i, j, rd, suc = 0;
    char *ch, *nm;
    unsigned char mode = 0;
    unsigned short nent, nlen;
    long off, dlen, doff, flen;
    struct dmap *ptrd = (struct dmap *) (buf + 15);
    struct tmap *ptrt = (struct tmap *) (buf + 17);
    time_t t;
    if (argc < 2) {
    printf("\nuseage: slib <filename> [-v]");
    printf("\ndisplay contents of setup library file: <filename>");
    printf
        ("\ncurrently only option is -v to enable verbose data display\n");
    return (0);
    }
    for (i = 2; i < argc; i++)
    if (strnicmp(argv[i], "-v", 2) == 0) {
        mode |= VERBOSE;
    }

    if ((fp = open(argv[1], O_RDONLY | O_BINARY)) <= 0) {
    printf("\nopen failed for input file: %s", argv[1]);
    return (fp);
    }
    printf("\nslib Version 1.00 compiled for %s using data file: %s",
       OS_STR, argv[1]);

    if (read(fp, buf, 0x40) != 0x40) {
    printf("\nerror reading file header");
    } else {
    doff = *((long *) (buf + 0x29));
    off = *((long *) (buf + 0x33));
    printf
        ("\noffset to catalog header 0x%lx and name list region 0x%lx",
         doff, off);
    if (lseek(fp, doff, SEEK_SET) != doff) {
        printf("\nfailed to seek to start of name list region");
        return (-1);
    } else if (read(fp, &nent, 2) != 2) {
        printf("\nerror # of entries in list");
        return (-1);
    } else
        printf("\nlibrary contains %d entries", nent);
    }

    if (lseek(fp, off, SEEK_SET) != off) {
    printf("\nfailed to seek to start of name list region");
    return (-1);
    }

    for (i = 0; suc == 0 && i < nent; i++) {
    if (read(fp, buf, 0x1E) != 0x1E) {
        printf("\nerror reading 0x1E bytes at offset 0x%lx", off);
        suc = -1;
    } else {
        off += 0x1E;
// confirmed flen below.  doff probably correct
        flen = *((long *) (buf + 3));   // uncomprssed file length
        dlen = *((long *) (buf + 7));   // once thought compressed data length, doubtful
        doff = *((long *) (buf + 11));  // offset to start data
        ch = &buf[0x1E];
        nlen = *((unsigned char *) (buf + 0x1D));   // start of variable length name string
        if (read(fp, ch, (int) nlen) != nlen) {
        printf
            ("\nerror reading 0x%x bytes of variable length name at 0x%lx",
             nlen, off);
        suc = -1;
        } else {
        *(ch + nlen) = 0;
        printf
            ("\n%2d: %-25s @ 0x%06lx len %8ld %02d-%02d-%4d %02d:%02d",
             cnt++, ch, off, flen, ptrd->mon, ptrd->day,
             ptrd->yr + 1980, ptrt->hr, ptrt->min);

        doff += dlen;

        nm = ch;    // save current name location for giggles
        ch += nlen; // advance to next location
        off += nlen;
        if (read(fp, ch, 13) != 13) {
            printf("\nfailed to read 13 trailer bytes");
            suc = -1;
        } else {
            if (mode & VERBOSE) {
            printf("\noffset to compressed data 0x%lx", doff);
            // try treating 1st data region as 16 shorts
            for (j = 0; j < 0x1D; j++) {
                if (j % 12 == 0)
                printf("\n0x%02x:", j);
                printf(" 0x%02x", *(buf + j));
            }

            // look at 2nd data region as shorts also
            printf("\n0x%02x:", 0x1E + nlen);
            for (j = 0; j < 13; j++)
                printf(" 0x%02x",
                   *((unsigned char *) (ch + j)));
            putchar('\n');
            }
            off += 13;
        }
        }
    }

    }
    putchar('\n');
    return (suc);
}

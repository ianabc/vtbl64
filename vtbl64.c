#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <time.h>
#include "vtbl64.h"

fhead113 *get_fheader(FILE *fp)
{
    fhead113 *hdr = NULL;
    int sz, rd;

    sz = sizeof(fhead113);
    if ((hdr = (fhead113 *) malloc(sz)) == NULL) {
        fprintf(stderr, "Failed to allocate space for header\n");
        exit(1);
    }
    if ((rd = fread(hdr, FHDR_SZ, 1, fp)) != 1) {
        fprintf(stderr, "Only read 0x%x bytes of 0x%x byte header\n", rd, sz);
		exit(1);
	}
    if (hdr->sig != 0xAA55AA55) {
        fprintf(stderr, "Invalid header signature 0x%x != 0xAA55AA55\n", hdr->sig);
		exit(1);
	}

    return (hdr);
}


vtbl113 *get_vtbl(FILE *fp)
{
    vtbl113 *vtbl = NULL;
    int sz, rd;

    sz = sizeof(vtbl113);
    if ((vtbl = (vtbl113 *) malloc(sz)) == NULL) {
        fprintf(stderr, "Failed to allocate space for vtbl\n");
        exit(1);
    }
    if ((rd = fread(vtbl, sizeof(vtbl), 1, fp)) != 1) {
        fprintf(stderr, "Only read 0x%x bytes of 0x%x byte vtbl\n", rd, sz);
		exit(1);
	}

    return (vtbl);
}


void disp_vtbl(vtbl113 *vtbl)
{
    int i, rd;
    char date[64];
    time_t timestamp;
    struct tm *tm;

    fprintf(stdout, "Label: %.44s  \nVTBL volume contains %u logical segments\n",
           vtbl->desc, vtbl->nseg);
    
    timestamp = (time_t)vtbl->date;
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
   
    if ((vtbl->flag & 1) == 0)  /* generic, not vendor specific */
    {
        /*
         * fields after flag not valid if vendor specific
         * ignore quad word, assume vtbl->dataSz[1] == 0
         */
        fprintf(stdout, "version: %0x:%0x\n", vtbl->rev_major, vtbl->rev_minor);
        fprintf(stdout, "dir size 0x%x data size 0x%x\n", vtbl->dirSz, vtbl->dataSz[0]);
        fprintf(stdout, "QFA physical start block 0x%x end block 0x%x\n", vtbl->start, vtbl->end);
        fprintf(stdout, "compression byte 0x%x\n", vtbl->comp);
        if (vtbl->comp & 0x80)
            fprintf(stdout, "Compression used, type 0x%x\n", vtbl->comp & 0x3f);
        if (vtbl->OStype < 8)
            fprintf(stdout, "OS type: d => %s\n", OStype[vtbl->OStype]);
    }
}


int main(void) {

	FILE *fp;
   
	fhead113 *fhead1, *fhead2;
	vtbl113 *vtbl;

	if (!(fp = fopen("../Image.113", "rb"))) {
		fprintf(stderr, "Can't open input file '../Image.113'\n");
		exit(1);
	}

	fhead1 = get_fheader(fp);

	fseek(fp, SEG_SZ, SEEK_SET);
    if(ftell(fp) != SEG_SZ) {
    	fprintf(stderr, "Unable to seek to second header: %ld (%ld)\n", ftell(fp), SEG_SZ);
		exit(1);
	}

	fhead2 = get_fheader(fp);


	vtbl = get_vtbl(fp);

    fseek(fp, 2 * SEG_SZ, SEEK_SET);
    if(ftell(fp) != 2 * SEG_SZ ) {
        fprintf(stderr, "Unable to seek to vtbl\n");
    }
	if (fread(vtbl, sizeof(*vtbl), 1, fp) != 1) {
       	printf("\nfailed to get VTBL");
        exit(1);
    }
    if (strncasecmp((const char *)vtbl->tag, "VTBL", 4) != 0) { 
        fprintf(stderr, "Missing 'VTBL' tag, invalid record at offset 0x%lx ", ftell(fp));
        exit(1);
    }
    disp_vtbl(vtbl); 

	free(fhead1);
    free(fhead2);
	free(vtbl);
	fclose(fp);

	return EXIT_SUCCESS;
}

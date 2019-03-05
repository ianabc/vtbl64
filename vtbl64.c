#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "vtbl64.h"

struct fhead113 *get_fheader(FILE *fp)
{
    struct fhead113 *hdr = NULL;
    int sz, rd;

    sz = sizeof(struct fhead113);
    if ((hdr = (struct fhead113 *) malloc(sz)) == NULL) {
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


struct vtbl113 *get_vtbl(FILE *fp)
{
    struct vtbl113 *vtbl = NULL;
    int sz, rd;

    sz = sizeof(struct vtbl113);
    if ((vtbl = (struct vtbl113 *) malloc(sz)) == NULL) {
        fprintf(stderr, "Failed to allocate space for vtbl\n");
        exit(1);
    }
    if ((rd = fread(vtbl, FHDR_SZ, 1, fp)) != 1) {
        fprintf(stderr, "Only read 0x%x bytes of 0x%x byte vtbl\n", rd, sz);
		exit(1);
	}

    return (vtbl);
}


int main(void) {

	FILE *fp;
   
	struct fhead113 *fhead1, *fhead2;
	struct vtbl113 *vtbl;

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
    if(ftell(fp) != 2 * SEG_SZ )
		if (fread(vtbl, sizeof(vtbl), 1, fp) != sizeof(vtbl)) {
        	printf("\nfailed to get VTBL");
            exit(1);
		}

	free(fhead1);
    free(fhead2);
	free(vtbl);
	fclose(fp);

	return EXIT_SUCCESS;
}

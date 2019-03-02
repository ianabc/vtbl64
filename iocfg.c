/*  iocfg.c  1st trial routine to parse *.113 configuration data base files.
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

02/16/17 add parsing new 1-STEP backup ver 4.4 '1-STEP.FSS'  file
  although I have little idea what it is. Only one sample to date.
  In my sample the path to all files were preceded by "r\d\m\"
  It contained 18 paths, but the backup selection had something
  like 80 files selected, so no clue as to which files are listed here.
*/
#include <stdio.h>
#include <time.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>		// for malloc() and exit() to surpress warning about implicit declaration


#if  defined(MSDOS) || defined(_WIN32)
# ifdef MSDOS
#  define OS_STR "MSDOS"
# else
#  define OS_STR "WIN32"
# endif
/* search routines from msqic compiled for large file access
*/
#define IS_UNIX 0
#include <io.h>			// for lseek
#include <sys\types.h>
#include <sys\stat.h>		// this include must follow types.h for MSC
#include <sys\utime.h>
#include <direct.h>		// for mkdir()
#else
// its a unix like environment
#define IS_UNIX 1
#define OS_STR "LINUX"
#define O_BINARY 0		// required for DOS, this makes that bit Linux compatible
#include <unistd.h>		// for lseek, read, write
#include <sys/stat.h>
#include <sys/types.h>
#include <utime.h>

#define strnicmp strncasecmp
#define stricmp  strcasecmp

// linux subsitute for DOS function time()
long time(long *t)
{
    struct timespec ts;
    long ret = 0;
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0)
	ret = ts.tv_sec;
    if (t != NULL)
	*t = ret;
    return (ret);
}
#endif

/* below works in linux but may not in DOS
 *  where the file names are upper case?
 * for my sample version 4.5 *.113 backup program the data below is valid
*/
struct iocfg_files {
    char *name;
    int nfields;		// number of fields (including entry 0 which isn't a field!
    int ent_sz;			// data size of each entry, ie bytes to read
} dbf_files[] = {
    {
    "fileinfo.dbf", 10, 55}, {
    "files.dbf", 6, 34}, {
    "tapes.dbf", 8, 147}, {
    "volumes.dbf", 23, 242}
};

struct sdef {
    char fname[11];
    char type;
    long off;			// offset in structure
    unsigned short len;
    unsigned short pad[7];
};

#define VERSION "0.91"

#define BUFSZ 0x400

unsigned char buf[BUFSZ];	// global working buffer

/* convert on of the TIME_DATE fields to a time_t value
 * all time fields in the *.dbf files are 22 bytes of character data
 * convert these to struct tm format, call mktime() to get time_t value
 * field format is yyyymmddhhmmss00+zzzz
 * where 4 chars for year, with base 0,and two each for:
 * month with base 1, dat with base 1, and hour,min,sec with base 0
 * last two chars before '+' have always been zero, for 100th of sec?
 * after the '+' its often 4 zeros, but seems to be timezone in fileinfo.dbf_files
 * I currently ignore timezone below
 * ie 2017012713272000+0300 
*/
time_t tfld2time_t(unsigned char *tstr)
{
    int i;
    char lbuf[6];
    struct tm tmt;
    time_t t;
    tmt.tm_isdst = -1;
    for (i = 0; i < 4; i++)
	lbuf[i] = *tstr++;
    lbuf[i] = 0;
    tmt.tm_year = atoi(lbuf) - 1900;	// base year for tmt is 1900
    for (i = 0; i < 2; i++)
	lbuf[i] = *tstr++;
    lbuf[i] = 0;
    tmt.tm_mon = atoi(lbuf) - 1;

    for (i = 0; i < 2; i++)
	lbuf[i] = *tstr++;
    lbuf[i] = 0;
    tmt.tm_mday = atoi(lbuf);

    for (i = 0; i < 2; i++)
	lbuf[i] = *tstr++;
    lbuf[i] = 0;
    tmt.tm_hour = atoi(lbuf);

    for (i = 0; i < 2; i++)
	lbuf[i] = *tstr++;
    lbuf[i] = 0;
    tmt.tm_min = atoi(lbuf);

    for (i = 0; i < 2; i++)
	lbuf[i] = *tstr++;
    lbuf[i] = 0;
    tmt.tm_sec = atoi(lbuf);
    t = mktime(&tmt);
    return (t);
}

/* attempt a generic data entry display routine
   this ignores type M  not sure about it, so far they all seemed to be spaces, but
   now have some long names in Job 4
   as of 1/12/17 treat any type 'C' with sd->len == 1, 2 or 4 as numeric, and sd->len ==  3 as error 
   as of 1/28/17 2nd arg is removed, create 'struct sdef *' here from ndx
   1/29 introduced an error when did this, left the ndx in most of printf()
   statements below, but removed from format spec cause now just want it to
   print string, if want ndx printed do before calling this routine
   this corrected by segmentation fault problem this morning
   1/30/17 add char fmt[] and parameter w so can format field display width
     this was late in game, would cure some of other format issues...
*/
void disp_srec(int ndx, int w, unsigned char *def, unsigned char *dat)
{
    struct sdef *sd = (struct sdef *) (def + 32 * ndx);	// struct def for this rec
    unsigned char *ch = dat + sd->off;	// start of this data entry
    unsigned char *nch, ori;
    char fmt[10], *s;
    int i, len = 0, loff = 0;
    long n;

    if ((sd->type == 'C' && sd->len > 4) || sd->type == 'M') {
	for (i = 0; i < sd->len; i++)
	    if (*(ch + i) != ' ')
		if (sd->type == 'M')	// its type 'M' just count non ' ' chars for now
		    loff++;
		else
		    loff = i + 1;	// find ndx to printable char +1 in type 'C' data, ie str terminator offset
    }

    if (sd->type == 'L')	// its a logical, just 1 char true or false
    {
	if (w <= 0)
	    strcpy(fmt, " %s");
	else
	    sprintf(fmt, " %%%ds", w);
	if (*ch)
	    printf(fmt, " true");
	else
	    printf(fmt, " false");
    } else if (sd->type == 'C' && sd->len <= 4)	// treat a binary number
    {
	if (sd->len == 3)
	    printf("\n error length of %d for type %c field", ndx, sd->len,
		   sd->type);
	else if (sd->len == 4)
	    n = *((long *) ch);
	else if (sd->len == 2)
	    n = *((unsigned short *) ch);	// not sure this happens
	else			// sd->len == 1
	    n = *((unsigned char *) ch);	// this does happen in volumes.dbf!
	if (w < 1)
	    strcpy(fmt, " %ld");
	else
	    sprintf(fmt, " %%%ld", w);
	printf(fmt, n);
    } else if (sd->type == 'C')	// display as characters if sd->len > 4
    {				// all these fields are <= 44 bytes long
	nch = ch + loff;
	ori = *nch;		// save next char
	*nch = 0;		// temporarily Nul terminate this string
	s = ch;
	if (w <= 0)
	    strcpy(fmt, " %-60s");
	else
	    sprintf(fmt, " %%-%ds", w);
	printf(fmt, s);
	*nch = ori;		// restore original data to buf[]
    } else if (sd->type == 'M' && loff > 0) {
	printf(" 'M' data contains %d chars:", loff);	// ignore width param
	for (i = sd->len - 1; i >= 0 && loff-- > 0; i--)
	    printf(" 0x%02x", *(ch + i));	// grows down from end?
    }
}



/* add argument targ, and logic that if mode == 2
 * we open and scan the records, but don't display data.
 * instead exit when rec # == targ has been loaded
 * 1/31/17 add some bitmaps in mode 
*/
#define DISPFLD 0x80		// high order bit set to display fields
		     // clear if no display
#define DISPDAT 0x40		// bit to force parsing of data records
		     // clear if just want to see fields
#define RECNUM  0x20		// bit to force rec# search for target
#define FLDMSK  0x1f		// mask to obtain field # for target from mode
		     // clear if there is no field target
int parse_dbf(int ndbf, unsigned char mode, long targ, int *fh)
{
    int cnt = 1, fp, fld, i, j, len, rd;
    unsigned char *dat;
    struct sdef *sd;
    if (ndbf < 0 | ndbf > 3) {
	printf("\nndbf = %d invalid", ndbf);
	return (-3);
    }
    fld = mode & FLDMSK;
    printf("\n%s with %d fields", dbf_files[ndbf].name,
	   dbf_files[ndbf].nfields - 1);
    if ((fp = open(dbf_files[ndbf].name, O_RDONLY | O_BINARY)) == EOF) {
	printf("\nopen failed");
	return (fp);
    }
    len = dbf_files[ndbf].nfields * 32 + 1;	// total length struct def + 0xd terminator
    if ((rd = read(fp, buf, len)) != len || buf[len - 1] != 0xD) {
	printf
	    ("read error obtaining structure definition, read %d bytes\n",
	     rd);
	return (-2);
    }
    // skip 1st definition, not sure of its function but its not a field
    if (mode & DISPFLD)		// mimic old logic where always display fields
	printf("\n#  field nm  type  offset len");
    for (i = 1, len = 32; i < dbf_files[ndbf].nfields; i++, len += 32) {
	sd = (struct sdef *) &buf[len];
	if (i == 1)
	    rd = sd->off;	// starting offset
	if (mode & DISPFLD)	// mimic old logic where always display fields
	    printf("\n%2d %-10s  %c    0x%03x  %02d", i, sd->fname,
		   sd->type, sd->off, sd->len);
	rd += sd->len;		// inc total bytes used
    }
    if (mode & DISPFLD)		// mimic old logic where always display fields
	printf("\nusing total of %d bytes per entry\n", rd);
    if (buf[len] != 0xD) {
	printf("\nFailed to find structure terminator at file offset 0x%x",
	       len);
	return (-3);
    } else if ((mode & DISPDAT) == 0)
	return (0);		// just do fields, do not look at data (return(rd) till 1/31/17)
    else if (len + rd >= BUFSZ) {
	printf("\nBUFSZ < %d bytes, not enough room to buffer data",
	       len + rd);
	return (-4);
    }
    // successfully parsed structure, now attempt to display generically
    // or if mode & RECNUM or fld > 0 find a target in record in buf[]
    dat = &buf[len];		// read data into buf[] after sdef data
    while (read(fp, dat, rd) == rd) {
	if (mode & DISPFLD)	// mimic old logic where always display fields
	{
	    printf("\ndata record %d", cnt);
	    for (i = 1; i < dbf_files[ndbf].nfields; i++) {
		printf("\n%2d:", i);
		disp_srec(i, -1, buf, dat);
	    }
	} else if ((mode & RECNUM) && cnt == targ)	// target is a record #
	{
	    *fh = fp;		// return open file handle in fh
	    return (cnt);	// this is the record # of the data currently in memory
	} else if (fld > 0 && fld <= dbf_files[ndbf].nfields) {
	    sd = (struct sdef *) &buf[fld * 32];
	    // ids are always 4 bytes which are used to link to other *.dbf files
	    if (sd->type != 'C' || sd->len != 4) {
		printf("\ninvalid field %d for a long numeric target",
		       fld);
		return (-5);
	    } else if (*((long *) (dat + sd->off)) == targ) {
		*fh = fp;	// return open file handle in fh
		return (cnt);	// this is the record # of the data currently in memory
	    }

	}
	cnt++;
    }

    if (*dat != 0x1a)		// did we see the terminator
    {
	printf("\ndata read error for entry %d, no data terminator found",
	       cnt + 1);

    }
    if (fp > 0)
	close(fp);
    if (mode & RECNUM || fld > 0)
	return (0);		// target was not found
    else
	return (cnt);		// otherwise return # of records read
    // note a call to parse_dbf() with mode = 0 will return the # of data records.   
    // It can also be determined from the file length...

}

// display iocfg database DATE_TIME field as ctime output
void disp_time(unsigned char *tstr)
{
    time_t t = tfld2time_t(tstr);
    if ((long) t < 0)
	printf(" mktime() error\n");
    else
	printf("%s", ctime(&t));	// can't figure out how to surpress LF at end of string
}

// parse new file, '1-STEP.FSS', added in ver 4.4 of 1-STEP backup
int parse_fss()
{
    int cnt = -1, fp, i, rd;
    long num;
    printf("\nparsing version 4.4 '1-STEP.FSS' files list");
    if ((fp = open("1-STEP.FSS", O_RDONLY | O_BINARY)) == EOF) {
	printf("\nunable to open '1-STEP-FSS'");
    } else if (read(fp, buf, 12) != 12) {
	printf("\nfailed to read file header");
    } else {
	num = *((long *) (buf + 5));
	printf("\ncontains %ld entries", num);
	cnt = 0;
	while (read(fp, buf, 10) == 10) {
	    rd = buf[9];
	    if (read(fp, buf, rd) == rd) {
		buf[rd] = 0;
		printf("\n%3d: %s", ++cnt, buf);
	    }
	}
    }
    return (cnt);
}


int disp_bkup(int vol_id, unsigned char mode)
{
    int cnt = 0, fcnt = 0, ff, fmax, fp, i, ldsk =
	-1, len, rd, ret, tape_id = -1;
    unsigned char *ch, *dat, *fdat, *fimg, *tstr, haveff = 0;
    long atrib, fid, flen, cflen = 0;	// save file lengths here
    time_t t, vt;
    /* see if the entire files.dbf can be loaded for random access display
       of files names.  If not just use file_id in detailed listing 
     */
    if (mode & DISPDAT)		// display of file details requested
    {
	if ((ff = open(dbf_files[1].name, O_RDONLY | O_BINARY)) == EOF) {
	    printf("\nunable to open 'files.dbf'");
	} else {
	    if ((len = (int) lseek(ff, 0L, SEEK_END)) < 0 ||
		lseek(ff, 0L, SEEK_SET) < 0)
		printf("\nunable to determine file length");
	    else if ((fimg = malloc(len)) == NULL)
		printf("\nunable to allocate space for 'files.dbf'");
	    else if (read(ff, fimg, len) != len)
		printf("\nunable to read all data from 'files.dbf'");
	    else {
		fdat = fimg + 32 * dbf_files[1].nfields + 1;	// starts after 0xd terminator
		len -= 32 * dbf_files[1].nfields + 1;
		fmax = len / dbf_files[1].ent_sz - 1;	// max index size given data size
		// might be better to scan for 0x1a terminator?
		haveff++;	// try to display names
	    }
	}
	if (ff > 0)
	    close(ff);
	if (haveff == 0)
	    printf("\nwill only display file_id not names");
    }
    // load volumes.dbf my record # = vol_id+1
    // but have added search target by field # which is 1 for the volume ID
    ret = parse_dbf(3, DISPDAT + 1, vol_id, &fp);

    if (ret < 1) {
	printf
	    ("\nabort: did not load 'volumes.dbf' rec containing ID = %d",
	     vol_id);
	return (-1);
    }
    if (fp > 0)
	close(fp);		// done with this file
    fp = -1;

    printf("\ndata for VOL_ID %d in volumes.dbf", vol_id);
    len = dbf_files[3].nfields * 32;	// total length struct def, over write term with data
    dat = buf + len;
    tstr = dat + 0x37;		// know offset to TIME_DATE
    vt = tfld2time_t(tstr);	// convert field to time_t value and save
    printf("\nTape ID:     ");
    disp_srec(2, -1, buf, dat);
    printf("\nDescription: ");
    disp_srec(5, -1, buf, dat);
    printf("\nTIME_DATE:   ");
    disp_srec(6, -1, buf, dat);
    printf("\nCompression: ");
    disp_srec(13, -1, buf, dat);
    printf("\nData size:   ");
    disp_srec(20, -1, buf, dat);
    printf("\nDir size:    ");
    disp_srec(22, -1, buf, dat);

    // 1/31/17 switch to search by TAPE_ID which is field 1 in tapes.dbf
    // I include the timestamp test to validate as my database seems corrupted
    tape_id = *((unsigned long *) (dat + 5));	// known offset to tape_id in volumes.dbf
    // tape_id++; was doing increment so it matches tapes.dbf ID
    if ((ret = parse_dbf(2, DISPDAT + 1, tape_id, &fp)) < 1) {
	printf("\nunable to load 'tapes.dbf' record with 'ID' = %d",
	       tape_id);
	printf("\nJob and disk data can not be displayed");
    } else {
	len = dbf_files[2].nfields * 32;	// total length struct def overwrite terminator
	dat = buf + len;
	printf("\nID:          ");
	disp_srec(1, -1, buf, dat);
	printf("\nNAME:        ");
	disp_srec(2, -1, buf, dat);	// job # and last disk #
	printf("\nLFTIME:      ");
	disp_srec(4, -1, buf, dat);	// 1st of 4 time stamps
	tstr = dat + 0x3b;	// known offset to LFTIME
	t = tfld2time_t(tstr);
	if ((long) t > 0 && (long) vt > 0
	    && ((long) (t - vt) > 300 || (long) (t - vt) < -300))
	    printf
		("\nwarning volumes and tapes record timestamps differ by more than 5 minutes");
	ch = dat + 5;		// "Backup Job #, Disk #" can parse # of disks from this string
	for (i = 0; i < 16 && *ch != ','; i++, ch++);
	if (*ch == ',') {
	    ch += 6;
	    if (sscanf(ch, "%d", &ldsk) != 1)	// ldsk is last disk # in backup
		ldsk = -1;
	}
	if (ldsk < 1) {
	    printf("\nfailed to parse valid # of disks in backup");
	    return (-3);
	}
    }
    if (fp > 0)
	close(fp);		// done with this file
    fp = -1;


    if ((ret = parse_dbf(0, DISPDAT + RECNUM, 1, &fp)) != 1)	// rec #1 is ID 0, the first
    {
	printf("\nabort: unable to open and load 'fileinfo.dbf' record 1");
	return (-4);
    }

    len = dbf_files[0].nfields * 32;	// total length struct def overwrite terminator
    rd = dbf_files[0].ent_sz;	// entry size, ie number of bytes in data record
    dat = buf + len;
// now read records for all fields, 1st try ignore multiple disks
// note the vol_id is zero based so 1 less then my record number which is 1 based
// in vol_id in files 

    if (mode & DISPDAT)
	printf("\ncnt    NAME_ID   SIZE_LO  ATTRIB   TIME_DATE\n");
    do {
	if (*((unsigned long *) (dat + 5)) == vol_id)	// fileinfo.dbf vol_id field matches
	{
	    if ((flen = *((unsigned long *) (dat + 13))) > 0) {
		fcnt++;		// entries with file length > 0
		cflen += flen;
	    }
	    atrib = *((long *) (dat + 39));
	    fid = *((long *) (dat + 1));	// NAME_ID in fileinfo.dbf

	    if ((mode & DISPDAT) && haveff == 0)
		printf("%3d : %4ld", cnt + 1, fid);	// NAME_ID maps to files.dbf
	    else if (mode & DISPDAT) {
		printf("%3d: ", cnt + 1);
		if (fid < 1 || fid > fmax)	// id is out of range display id not name
		    printf(" id=%3d ", fid);
		else
/* origianlly did below, but it assumeds a string len of 60 chars, scews up display format
 *so add decicated routine disp_fname()  1st try just default 12 char display 
 *   but default in disp_srec() displays 60!
	       disp_srec(3,-1,fimg,fdat+dbf_files[1].ent_sz * (fid -1));
*/
		    disp_srec(3, 14, fimg,
			      fdat + dbf_files[1].ent_sz * (fid - 1));
	    }
	    if (mode & DISPDAT) {
		printf("    %10ld  0x%03lx  ", flen, atrib);
		// convert 5: TIME_DAT and 6: ATTRIBUTES to format used by rd113
		tstr = dat + 17;
		disp_time(tstr);	// this provides the LF for display
	    }
	    cnt++;		// number of records that match
	}
    } while (read(fp, dat, rd) == rd && *dat != 0x1a);
    printf("\nfound %d records with VOL_ID = %d", cnt, vol_id);
    printf("\n%d have associated data with %ld total data bytes", fcnt,
	   cflen);
    if (fp > 0)
	close(fp);		// done with this file
    return (cnt);
}

// search in file dbf_files[dndx].name for targets matching long value targ
// in fld and list them, ie this is a file ID match
int find(int ndbf, int fld, long targ)
{
    int cnt = 0, fp, len, rd, rec = 1, ret;
    unsigned char *dat;
    struct sdef *sd;

    if (fld < 0 && fld > dbf_files[ndbf].nfields) {
	printf("\n invalid field # for file");
	return (-3);
    } else if ((ret = parse_dbf(ndbf, DISPDAT + RECNUM, 1, &fp)) != 1)	// rec #1 is ID 0, the first
    {
	printf("\nabort: unable to open and load '%s' record 1",
	       dbf_files[ndbf].name);
	return (-4);
    }

    len = dbf_files[ndbf].nfields * 32;	// total length struct def overwrite terminator
    rd = dbf_files[ndbf].ent_sz;	// entry size, ie number of bytes in data record
    dat = buf + len;
    sd = (struct sdef *) &buf[fld * 32];
    if (sd->type != 'C' || sd->len != 4) {
	printf("\ninvalid field %d for a long numeric target", fld);
	return (-5);
    }
    printf("\nsearching %s, field %d for value = %ld",
	   dbf_files[ndbf].name, fld, targ);
    // copied from search routine in parse_dbf() just lets me test independatly
    while (1) {
	// ids are always 4 bytes which are used to link to other *.dbf files
	if (*((long *) (dat + sd->off)) == targ) {
	    cnt++;
	    printf("\n%3d match at record %d", cnt, rec);
	}
	rec++;
	if (read(fp, dat, rd) != rd || *dat == 0x1a) {
	    if (*dat == 0x1a) {
		printf("\neof for data, found %d matches", cnt);
		return (rec);
	    } else {
		printf("\nread error, found %d matches", cnt);
		return (-2);
	    }
	}
    }
}

void main(int argc, char *argv[])
{
    int i, fnum = -1, fp = 0;
    unsigned char mode = DISPFLD;	// default, display fields
    printf("\niocfg version %s compiled for %s\n", VERSION, OS_STR);

    for (i = 2; i < argc; i++)
	if (strnicmp("-d", argv[i], 2) == 0)
	    mode |= DISPDAT;	// display data fields also

    if (argc < 2) {
	printf("\nusage: iocfg ? [-d]");
	printf("\nwhere ? maybe '1' to '4' for one of *.dbf:\n      ");
	for (i = 0; i < 4; i++)
	    printf("  %d: %s", i + 1, dbf_files[i].name);
	printf("\n use '5' to parse version 4.4 '1-STEP.FSS'");

	printf("\n       or 'a' for all *.dbf,");
	printf("\n       or 'b#' to summarize backup # in volumes.dbf");
	printf("\n       or 'f#' to find VOL_ID == # in fileinfo.dbf");
	printf("\n       of 'fn#' to find NAME_ID == # in fileinfo.dbf");
	printf
	    ("\n       optional -d to display data in addition to field definitions");
	printf("\n       optional f searchs fileinfo.dbf for:");
	printf("\n           f#  matches with VOL_ID == #");
	printf("\n           fn# matches with NAME == #");
    } else if ((i = *argv[1] - '1') >= 0 && i < 4) {
	parse_dbf(i, mode, 0, &fp);
    } else if (i == 4)
	parse_fss();
    else if (*argv[1] == 'a' || *argv[1] == 'A') {
	for (i = 0; i < 4; i++)
	    parse_dbf(i, mode, 0, &fp);
    } else if ((*argv[1] == 'b' || *argv[1] == 'B')
	       && sscanf(argv[1] + 1, "%d", &i) == 1) {
	disp_bkup(i, mode);
    } else if ((*argv[1] == 'f' || *argv[1] == 'F')) {
	// tried to make find() routine generic, but only interested in fileinfo.dbf now
	if ((*(argv[1] + 1) == 'n' || *(argv[1] + 1) == 'N')) {
	    if (sscanf(argv[1] + 2, "%d", &i) == 1)
		find(0, 1, (long) i);	// find field NAME_ID matches
	} else if (*(argv[1] + 1) >= '0' && *(argv[1] + 1) <= '9') {
	    if (sscanf(argv[1] + 1, "%d", &i) == 1)
		find(0, 2, (long) i);	// find file VOL_ID matches
	}
    }



/*
   printf("\nsizeof(struct sdef) %d  sizeof(struct iocfg_files) %d",
	  sizeof(struct sdef),sizeof(struct iocfg_files));
*/

    putchar('\n');
}

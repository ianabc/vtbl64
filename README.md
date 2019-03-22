# rd133

This is another copy of Will Kranz's code for working with IOMega Backups.

  * [Will Kranz's site](http://packages.sw.be/rpmforge-release/rpmforge-release-0.5.2-2.el6.rf.x86_64.rpm)
  * [1-step backup](http://www.willsworks.net/file-format/iomega-1-step-backup)
  * [113-format](http://www.willsworks.net/file-format/iomega-1-step-backup/113-format)
  * [1-Step
  format](http://www.willsworks.net/file-format/iomega-1-step-backup/1-step-format)
  * [1-Step
  info](http://www.willsworks.net/file-format/iomega-1-step-backup/1step-info)
  * [rd113
  info](http://www.willsworks.net/file-format/iomega-1-step-backup/rd113-info)

There are a number of win32 console programs on Will's site along which can be
used to examine the Image.113 archives generated by IOMega backup.

## Archive Structure

This README deals only with the archive I was asked to extract; a 3.2GB
compressed .113 file. For more information on other formats, please see [Will's
site](http://packages.sw.be/rpmforge-release/rpmforge-release-0.5.2-2.el6.rf.x86_64.rpm).

0x7400 == 29696 == 32K - 3K 

As far as I can tell (QIC-113 Rev. G p39) the 3K is "reserved for an error
correction code (ECC)". On the same page, the also say that the frame size is actually

  32K - ECC - 2 - "frame offset"

They explicitly list frame offset as 2.

"Compression Frames may be concatenated together to fill a segment. However if a
frame ends within the last 18 bytes of available space in a segment (after
accounting for ECC), these remaining bytes are to be null flled and remain
unused. If a given frame has a Frame size that leaves more than 18 bytes
available, another frame is expected to immediately follow

### Headers
The file has two headers in the first two segments of the file (each segment is
an 0x7400 block). It corresponds to

### Compressed Data Segments

### Uncompressed Data Segments
From the QIC documentation I *think* we can safely assume a 1MB maximum buffer
size here. This simplifies handling for the history buffer etc. Here is my
argument.

The best/worst case scenario for the compression ratio is a very long string of
identical bytes. e.g. 'AAAAAAA....'. The history buffer is 2048 bytes long and
the offset is encoded in either 7 or 11 bits. For the pathalogical example given
above, the worst case scenario is offset of 1 and maximal length. The lengths
are encoded as

Length         Bit Pattern
2              00
3              01
4              10
5              11 00
6              11 01
7              11 10
8              11 11 0000
9              11 11 0001
10             11 11 0010
11             11 11 0011
12             11 11 0100
13             11 11 0101
14             11 11 0110
15             11 11 0111
16             11 11 1000
17             11 11 1001
18             11 11 1010
19             11 11 1011
20             11 11 1100
21             11 11 1101
22             11 11 1110
23             11 11 1111 0000
24             11 11 1111 0001
25             11 11 1111 0010
...
37             11 11 1111 1110
38             11 11 1111 1111 0000
39             11 11 1111 1111 0001
...

Ignoring the first 6 lengths above as a special case, the right most 4 digits
are just the numbers $`1, 2, ..., 15`$ (or equivalently $`0x1, 0x2, ..., 0xe`$). So
labeling these four digits as $`l`$, and the number of $`1111`$ blocks as $`i`$, the
length is

```math
L = 15\times (i - 1) + 8 + l
```

The maximum value of $`L`$ with a $`2k`$ history buffer is $`2047`$, which
corresponds to $`i = 136, l=14`$, which is a total of $`136\times 4 + 4 = 548`$
bits. Adding the $`1`$ bit string marker, the $`1`$ bit offset length indicator
and a $`7`$ bit offset gives us a compressed string length of $`557`$ bits. Each
of these strings represents $`2047`$ uncompressed bytes. For our pathological
case we are assuming that the compressed buffer contains as many of these
compressed strings as possible which is bounded above by

```math
 \lfloor{ \frac{ 32 * 1024 * 8 }{557} }\rfloor = 470 
```

If each compressed string expands to $`2047`$ bytes, this means that the
uncompressed buffer is bounded above by $`470 * 2047 = 962090 < 2^10`$. i.e.
Everything will fit in a $`1MB`$ buffer.

### Catalog

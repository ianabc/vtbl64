# vtbl64

This repository contains a derivative copy of Will Kranz's code for
decompressing and extracting IOMega Backups. If you are actually trying to
decompress one of these backups then you almost certainly want [Will's
code](http://www.willsworks.net/downloads) _not_ this one. The code in this
repository is my attempt to follow along with his ideas and to understand his
code, and isn't intended for practical use.

## Useful Links

  * [Will Kranz's site](http://www.willsworks.net/home) There are a number of
  win32 console programs and some portable C code on Will's site as well as
  extensive information on his investigations into the archive format.
    * [1-step backup](http://www.willsworks.net/file-format/iomega-1-step-backup)
    * [113-format](http://www.willsworks.net/file-format/iomega-1-step-backup/113-format)
    * [1-Step
  format](http://www.willsworks.net/file-format/iomega-1-step-backup/1-step-format)
    * [1-Step
  info](http://www.willsworks.net/file-format/iomega-1-step-backup/1step-info)
    * [rd113
  info](http://www.willsworks.net/file-format/iomega-1-step-backup/rd113-info)
  * [QIC-113revG](https://www.qic.org/html/standards/11x.x/qic113g.pdf) This
  standard describes many of the structures and features inside IOMega backups
  * [QIC-122](https://www.qic.org/html/standards/12x.x/qic122b.pdf) This
  standard describes the compression method.
  * [Data Compression - The Complete
  Reference](https://books.google.ca/books?id=ujnQogzx_2EC&lpg=PA184&ots=FqmwuF6smT&dq=QIC-122%20compression&pg=PA184#v=onepage&q&f=false),
  David Salomon. This book has a lot of useful information. It describes
  [LZ77](https://en.wikipedia.org/wiki/LZ77_and_LZ78) (of which QIC-122 is a
  variant) in detail, and has a small section on QIC-122.

## Building vtbl64

The code should be valid ANSI C, but I assume linux in few places so it might
not work on other systems. To compile `vtbl64` simply run `make` in the root
directory. Running `vtbl64 -h` will give you a summary of the available options.

## Why?

My experience with these files started with a colleague of mine turning up at my
office with an IOMega Peerless 20GB disk and a story about some TeX files for a
textbook he wrote in the early 2000s. Mounting the disk revealed a single large
file called Image.113, in my case of around 3.2GB in size. In principal the
IOMega Peerleess backup software can be used to work with these backups, but
that assumes you have a(n old) windows installation and access to the software.
Incidentally, I eventually managed to get a copy of Windows98 running via
virtualbox, with the 1 step backup program and the IOMega drive passed through
as a USB device, but I wouldn't recoommend it, and it'll probably continue to
get harder to do as the software ages.

That was the problem, and the eventual solution was to use the code Will wrote
(along with some patches he was kind enough to create for me!) to handle the
extraction on my (Linux) desktop.

## Image.113 - Archive Structure

After reading up a bit the archive format seems to follow some of the QIC-113
standard (see the links above). The file is split into segments of 0x7400 bytes,
where

0x7400 == 29696 == 32K - 3K 

As far as I can tell (QIC-113 Rev. G p39) the 3K is "reserved for an error
correction code (ECC)", but that doesn't seem to have been implemented in the
IOMega product.

### Header 1
### Header 2
### VTBL
### Data Segments
After running will's vtbl program, I found that the archive I was playing with
was compressed. As far as I understand it, the compression is implemented by
considering the uncompressed files as one long byte stream. That stream is
broken up into pieces, compressed and written into the segments described above.
These data segments consist of a small header followed by chunks of compressed
data called frames. Together, the frames inside a segment make up a "compressed
extent". Here is what the standard says


>Compression Frames may be concatenated together to fill a segment. However if a
frame ends within the last 18 bytes of available space in a segment (after
accounting for ECC), these remaining bytes are to be null filled and remain
unused. If a given frame has a Frame size that leaves more than 18 bytes
available, another frame is expected to immediately follow


#### Data Segment Headers
Ahead of the compressed extents there is a short header (see cseg_head). The
first 4 bytes are a "Cumulative Size", this is the number of _decompressed_
bytes which precede this header in the archive. This might seem odd, but makes
more sense for the tapes for which the QIC standards were originally written.
Along with the catalog, it allows you to extract a particular file by seeking to
the relevant compressed segment without decompressing the preceding segments.
The next 4 bytes are "Higher order Bits" for the Cumulative Size. The are
incremented to let you know that you have passed the 4GB maximum value of the
4-byte cumulative size. The last two bytes in the segment header give the length
of the next compressed frame.

#### Data Segment Compressed Frames
Immediately following the header the compressed
data starts using the QIC-122 format described below. Each frame is ended with
an "End of compression Marker" which looks like "110000000". In the QIC-122
standard, this is a "Compressed string, with a 7 bit offset, with an offset
value of zero". 

Following the first compressed frame, if the segment still has more than 18
remaining free bytes, more compressed frames will be concatenated. Similar to
the first frame, each frame is prefixed by its length in a 2 byte mini header.
Once we are within 18 bytes of the end of the segment or if there is no more
data in the input stream the segment is filled with zeros.

### Catalog

## Notes
#### Decompressed Data Segments
From the QIC documentation I *think* we can safely assume a 1MB maximum buffer
size here. This simplifies handling for the history buffer etc. Here is my
argument.

The best/worst case scenario for the compression ratio is a very long string of
identical bytes. e.g. 'AAAAAAA....'. The history buffer is 2048 bytes long and
the offset is encoded in either 7 or 11 bits. For the pathalogical example given
above, the worst case scenario is offset of 1 and maximal length. The lengths
are encoded as

| Length |        Bit Pattern |
|--------|--------------------|
| 2      |  00 |
| 3      |  01 |
| 4      |  10 |
| 5      |  11 00 |
| 6      |  11 01 |
| 7      |  11 10 |
| 8      |  11 11 0000 |
| 9      |  11 11 0001 |
| 10     |  11 11 0010 |
| 11     |  11 11 0011 |
| 12     |  11 11 0100 |
| 13     |  11 11 0101 |
| 14     |  11 11 0110 |
| 15     |  11 11 0111 |
| 16     |  11 11 1000 |
| 17     |  11 11 1001 |
| 18     |  11 11 1010 |
| 19     |  11 11 1011 |
| 20     |  11 11 1100 |
| 21     |  11 11 1101 |
| 22     |  11 11 1110 |
| 23     |  11 11 1111 0000 |
| 24     |  11 11 1111 0001 |
| 25     |  11 11 1111 0010 |
| ...    |  ... |
| 37     |  11 11 1111 1110 |
| 38     |  11 11 1111 1111 0000 |
| 39     |  11 11 1111 1111 0001 |
| ...    |  ... |

Ignoring the first 6 lengths above as a special case, the right most 4 digits
are just the numbers $`1, 2, ..., 15`$ (or equivalently $`\mathtt{0x1, 0x2, ...,
0xe}`$). So
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
 \left\lfloor{ \frac{ 32 * 1024 * 8 }{557} }\right\rfloor = 470 
```

If each compressed string expands to $`2047`$ bytes, this means that the
uncompressed buffer is bounded above by $`470 * 2047 = 962090 < 2^{10}`$. i.e.
Everything will fit in a 1MB buffer.


#ifndef img2_h
#define img2_h

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>


#define _2MG_DATA_START_OFFSET 32

typedef struct _2mg_s{
    FIL fil; 
                     
    uint8_t mounted;
    uint8_t writeable;
    char * filename;


    char creator[4];
    uint16_t version;
    uint32_t imageFormat;   /*      00= DOS 3.3 sector order
                                    01= ProDOS sector order
                                    02= NIB data 
                            */
    uint8_t isDoubleSided;
    uint32_t flags;                             /**< DOS Volume */

                            /* 0010-0013: 00 00 00 00  (Flags & DOS 3.3 Volume Number)

                                The four-byte flags field contains bit flags and data relating to
                                the disk image. Bits not defined should be zero.

                                Bit   Description

                                31    Locked? If Bit 31 is 1 (set), the disk image is locked. The
                                    emulator should allow no changes of disk data-- i.e. the disk
                                    should be viewed as write-protected.

                                8     DOS 3.3 Volume Number? If Bit 8 is 1 (set), then Bits 0-7
                                    specify the DOS 3.3 Volume Number. If Bit 8 is 0 and the
                                    image is in DOS 3.3 order (Image Format = 0), then Volume
                                    Number will be taken as 254.

                                7-0   The DOS 3.3 Volume Number, usually 1 through 254,
                                    if Bit 8 is 1 (set). Otherwise, these bits should be 0.
                            */

    uint32_t blockCount;     /* 0014-0017: 18 01 00 00  (ProDOS Blocks = 280 for 5.25")
                                The number of 512-byte blocks in the disk image- this value
                                should be zero unless the image format is 1 (ProDOS order).
                                Note: ASIMOV2 sets to $118 whether or not format is ProDOS.
                             */
    
    const uint8_t dataOffset;    //0018-001B: 40 00 00 00  (Offset to disk data = 64 bytes)

    /*  001C-001F: 00 30 02 00  (Bytes of disk data = 143,360 for 5.25")

        Length of the disk data in bytes. (For ProDOS should be
        512 x Number of blocks)


        0020-0023: 00 00 00 00  (Offset to optional Comment)

        Offset to the first byte of the image Comment- zero if there
        is no Comment. The Comment must come after the data chunk,
        but before the creator-specific chunk. The Comment, if it
        exists, should be raw text; no length byte or C-style null
        terminator byte is required (that's what the next field is for).


        0024-0027: 00 00 00 00  (Length of optional Comment)

        Length of the Comment chunk- zero if there's no Comment.


        0028-002B: 00 00 00 00  (Offset to optional Creator data)

        Offset to the first byte of the Creator-specific data chunk-
        zero if there is none.


        002C-002F: 00 00 00 00  (Length of optional Creator data)

        Length of the Creator-specific data chunk- zero if there is no
        creator-specific data.


        0030-003F: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00

        Reserved space- at present all must be zero.
    */
                                
    const char *creatorData;
    const char *commentData;
    
    
} _2mg_t;


int get2mgTrackFromPh(int phtrack);
unsigned int get2mgTrackSize(int trk);

long get2mgSDAddr(int trk,int block,int csize, long database);
enum STATUS get2mgTrackBitStream(int trk,unsigned char * buffer);
enum STATUS set2mgTrackBitStream(int trk,unsigned char * buffer);
enum STATUS mount2mgFile(char * filename);
enum STATUS diskTrack2Nib(unsigned char *buffer,unsigned char * nibBuffer,uint8_t trk);

enum STATUS img22Nic(unsigned char *src,unsigned char *buffer,uint8_t trk);
enum STATUS nic22mg(char *rawByte,unsigned char *buffer,uint8_t trk);

#endif 
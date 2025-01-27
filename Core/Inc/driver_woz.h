
#ifndef woz_h
#define woz_h

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

int getWozTrackFromPh(int phtrack);
unsigned int getWozTrackSize(int trk);

long getWozSDAddr(int trk,int block,int csize, long database);
enum STATUS getWozTrackBitStream_fopen(int trk,unsigned char * buffer);
enum STATUS getWozTrackBitStream(int trk,unsigned char * buffer);
enum STATUS setWozTrackBitStream(int trk,unsigned char * buffer);


enum STATUS mountWozFile(char * filename);

typedef struct woz_info_s {
    uint8_t     version;                // Version number of the INFO chunk.
                                        //      WOZ1 => 1
                                        //      WOZ2 => 2
                                        //      WOZ  2.1 => 3
    uint8_t     disk_type;              // 1 = 5.25, 2 = 3.5
    int     is_write_protected;         // 1 = Floppy is write protected
    uint8_t     sync;                   // 1 = Cross track sync
    uint8_t     cleaned;                // 1 = MC3470 fake bits removed
    char        creator [32];           // Name of software created this file (UTF-8, 0x20 padded, NOT zero terminated)
    uint8_t     disk_sides;             // The number of disk sides contained within this image. A 5.25 disk will always be 1. A 3.5 disk can be 1 or 2.
    uint8_t     boot_sec_format;        // The type of boot sector found on this disk. This is only for 5.25 disks. 3.5 disks should just set this to 0.
                                        //      0 = Unknown
                                        //      1 = Contains boot sector for 16-sector
                                        //      2 = Contains boot sector for 13-sector
                                        //      3 = Contains boot sectors for both
    uint8_t     opt_bit_timing;         // The ideal rate that bits should be delivered to the disk controller card.
                                        // This value is in 125 nanosecond increments, so 8 is equal to 1 microsecond.
                                        // And a standard bit rate for a 5.25 disk would be 32 (4µs).
    uint16_t    compatible_hw;          // Bit field with a 1 indicating known compatibility. Multiple compatibility flags are possible.
                                        // A 0 value represents that the compatible hardware list is unknown.
                                        //      0x0001 = Apple ][
                                        //      0x0002 = Apple ][ Plus
                                        //      0x0004 = Apple //e (unenhanced)
                                        //      0x0008 = Apple //c
                                        //      0x0010 = Apple //e Enhanced
                                        //      0x0020 = Apple IIgs
                                        //      0x0040 = Apple //c Plus
                                        //      0x0080 = Apple ///
                                        //      0x0100 = Apple /// Plus
    uint16_t    required_ram;           // Minimum RAM size needed for this software. This value is in K (1024 bytes).
                                        // If the minimum size is unknown, this value should be set to 0. So, a requirement
                                        // of 64K would be indicated by the value 64 here.
    uint16_t    largest_track;          // The number of blocks (512 bytes) used by the largest track.
                                        // Can be used to allocate a buffer with a size safe for all tracks.
} woz_info_t;


#endif
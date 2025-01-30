

#define _VERSION "v0.79.4"

#define SPLASHSCREEN_DURATION       1000

#define MAX_FAVORITES               10

#define MAX_FILENAME_LENGTH         64
#define MAX_FULLPATH_LENGTH         256
#define MAX_PATH_LENGTH             64
#define MAX_FULLPATHIMAGE_LENGTH    320 
#define JSON_BUFFER_SIZE            2048

#define CONFIGFILE_NAME             "/sdiskConfig.json"

#define SCREEN_MAX_LINE_ITEM        4
#define SCREEN_LINE_HEIGHT          9

#define SCREENSHOT                  0

#define RAW_SD_TRACK_SIZE           8192                          // Maximuum track size on NIC & WOZ to load from SD
#define WEAKBIT                     1

#define MAX_TRACK                   59                                    // Max Number of track for a given Apple II Floppy,

#define MAX_PARTITIONS              4                               // Number of 32MB Prodos partions supported
#define SP_PKT_SIZE                 604
// SMARTPORT MESSAGE OFFSET
/*

The following packet 
shows the timing and content of a SmartPort READBLOCK call.  For further 
explanation of the structure, please see the Apple IIGS Hardware Reference and 
the Apple IIGS Firmware Reference.


DATA            MNEMONIC  DESCRIPTION                           TIME
(SmartPort Bus)           (Relative)
_____________________________________________________________________________
FF              SYNC      SELF SYNCHRONIZING BYTES              0                   0
3F              :         :                                     32 micro Sec.       1
CF              :         :                                     32 micro Sec.       2
F3              :         :                                     32 micro Sec.       3
FC              :         :                                     32 micro Sec.       4
FF              :         :                                     32 micro Sec.       5
C3              PBEGIN    MARKS BEGINNING OF PACKET             32 micro Sec.       6
81              DEST      DESTINATION UNIT NUMBER               32 micro Sec.       7
80              SRC       SOURCE UNIT NUMBER                    32 micro Sec.       8
80              TYPE      PACKET TYPE FIELD                     32 micro Sec.       9
80              AUX       PACKET AUXILLIARY TYPE FIELD          32 micro Sec.      10
80              STAT      DATA STATUS FIELD                     32 micro Sec.      11
82              ODDCNT    ODD BYTES COUNT                       32 micro Sec.      12
81              GRP7CNT   GROUP OF 7 BYTES COUNT                32 micro Sec.      13
80              ODDMSB    ODD BYTES MSB's                       32 micro Sec.      14
81              COMMAND   1ST ODD BYTE = Command Byte           32 micro Sec.      15
83              PARMCNT   2ND ODD BYTE = Parameter Count        32 micro Sec.      16
80              GRP7MSB   MSB's FOR 1ST GROUP OF 7              32 micro Sec.      17
80              G7BYTE1   BYTE 1 FOR 1ST GROUP OF 7             32 micro Sec.      18
98              G7BYTE2   BYTE 2 FOR 1ST GROUP OF 7             32 micro Sec.      19
82              G7BYTE3   BYTE 3 FOR 1ST GROUP OF 7             32 micro Sec.      20
80              G7BYTE4   BYTE 4 FOR 1ST GROUP OF 7             32 micro Sec.      21
80              G7BYTE5   BYTE 5 FOR 1ST GROUP OF 7             32 micro Sec.      22
80              G7BYTE5   BYTE 6 FOR 1ST GROUP OF 7             32 micro Sec.      23
80              G7BYTE6   BYTE 7 FOR 1ST GROUP OF 7             32 micro Sec.      24
BB              CHKSUM1   1ST BYTE OF CHECKSUM                  32 micro Sec.      25
EE              CHKSUM2   2ND BYTE OF CHECKSUM                  32 micro Sec.      26
C8              PEND      PACKET END BYTE                       32 micro Sec.      27
00              FALSE     FALSE IWM WRITE TO CLEAR REGISTER     32 micro Sec.      28
_____________________________________________________________________________
*/
#define SP_PBEGIN   0
#define SP_DEST     1
#define SP_SRC      2
#define SP_TYPE     3
#define SP_AUX      4
#define SP_STAT     5
#define SP_ODDCNT   6
#define SP_GRP7CNT  7
#define SP_ODDMSB   8
#define SP_COMMAND  9
#define SP_PARMCNT  10
#define SP_GRP7MSB  11
#define SP_G7BYTE1  12

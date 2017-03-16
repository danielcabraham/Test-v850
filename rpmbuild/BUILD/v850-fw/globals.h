#ifndef _GLOBALS_H
#define _GLOBALS_H

#include "v850_Macro.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#ifndef LINUX
/* Global Type definitions */
typedef          char  sbyte;     /* 1 byte signed,   type identifier: sb   */
typedef unsigned char  ubyte;     /* 1 byte unsigned, type identifier: ub   */
typedef   signed short sword;     /* 2 byte signed,   type identifier: sw   */
typedef unsigned short uword;     /* 2 byte unsigned, type identifier: uw   */
typedef   signed long  slword;    /* 4 byte signed,   type identifier: slw  */
typedef unsigned long  ulword;    /* 4 byte unsigned, type identifier: ulw  */
#else
#include <stdint.h>
#define __int64 int64_t

typedef int8_t sbyte;
typedef uint8_t ubyte;
typedef int16_t sword;
typedef uint16_t uword;
typedef int32_t slword;
typedef uint32_t ulword;

typedef unsigned short WORD;
typedef unsigned long  LONG;
#endif

#define  TRUE 1
#define FALSE 0

#define   SET 1
#define CLEAR 0

#define  ON 0x00
#define  OFF !ON

#define YES 0x00
#define  NO !YES

#define  GUI_OK                1
#define  GUI_NOK               0
#define  GUI_CFILE_OPEN_ERROR  2

/* union for byte access of uword */
typedef union
                {
                    uword x;                /* Word access                            */
                    struct                  /* structure for Byte access              */
                    {
                        ubyte lsb;          /* Least significant Byte                 */
                        ubyte msb;          /* Most significant Byte                  */
                    }h;                     /* name of structure (h = half)           */
                }dbyte;                     /* type name of union                     */

typedef union
                {
                    ulword x;               /* Word access                            */
                    struct                  /* structure for Byte access              */
                    {
                        ubyte byte0;        /* Least significant Byte                 */
                        ubyte byte1;
                        ubyte byte2;
                        ubyte byte3;        /* Most significant Byte                  */
                    }h;                     /* name of structure (h = half)           */
                }ddword;                    /* type name of union                     */



/* Flash Info Structure */

typedef  struct{
                  FILE  *BootStrapCodeFile; /* Mandatory                                  */
                  FILE  *FlashWriteFile;    /* Mandatory                                  */
                  FILE  *SerialEEPFile;     /* Optional; NULL if programming not required */
                  FILE  *SerialFlashFile;   /* Optional; NULL if programming not required */
                  FILE  *ParallelFlashFile; /* Optional; NULL if programming not required */
                  FILE  *InternalFlashFile; /* Optional; NULL if programming not required */
                  ubyte ubBaudRate;         /* Optional; If '0xff'  - default 19.2kbps    */
                  ubyte ubVerify;           /* 0x00 - ON; 0xFF - ON; default - ON         */
                  uword uwTimeout;          /* In ms; Max-10000ms                         */
                  ubyte ubRetry;            /* default - 10                               */
                  ubyte ubAutoClose;        /* 0x00 - NO; 0xFF - YES                      */
                  ubyte ubCommPort;         /* 1,2,3 or 4                                 */
                  FILE  *SummaryFile;       /* Optional; NULL if not required             */
                  FILE  *FlashBinFile     ; /* Optional; NULL if not required             */
                  sbyte *psbCustomerNumber; /* Mandatory; Needed to load Kithara DLLs     */
                  uword uwEraseTimeOut;     /* in seconds */
                  uword uwDelayTime;        /* in milli seconds */
                  ubyte ubEEPDownload;      /* 0:Startup, 1:Delivery */
                } FlashInfo;


/****** Flash Info Structure for reading from Config File**********/
typedef  struct  {
      ubyte *pubBootStrapCodeFile;  /* Mandatory                                  */
                  ubyte *pubFlashWriteFile;  /* Mandatory                                  */
                  ubyte *pubSerialEEPFileStUp;  /* Delivery file Optional; NULL if programming not required */
                  ubyte *pubSerialEEPFileDel;  /* Startup file  Optional; NULL if programming not required */
                  ubyte *pubSerialFlashFile;  /* Optional; NULL if programming not required */
                  ubyte *pubParallelFlashFile;  /* Optional; NULL if programming not required */
                  ubyte *pubInternalFlashFile;  /* Optional; NULL if programming not required */

                  ulword ulwBaudRate;    /* Optional; If '0xff'  - default 19.2kbps    */
                  ubyte *pubVerify;    /* 0x00 - ON; 0xFF - ON; default - ON         */
                  uword uwTimeout;    /* In ms; Max-10000ms                         */
                  ubyte ubRetry;    /* default - 10                               */
                  ubyte *pubAutoClose;    /* 0x00 - NO; 0xFF - YES                      */
                  ubyte ubCommPort;    /* 1,2,3 or 4                                 */

                  ubyte *pubSummaryFile;  /* Optional; NULL if not required             */
                  ubyte *pubFlashBinFile;       /* Optional; NULL if not required             */
                  sbyte *acbCustomerNumber;     /* Mandatory; Needed to load Kithara DLLs     */
                  uword uwErsTime;              /* in seconds                                 */
                  uword uwDloadDelayTime;       /* in milli seconds                           */
                  ubyte *pubEEPDownload;        /* "Startup" or "Delivery"                    */
                } FlashConfig;

typedef struct {
                 ulword ulwAddress;
                 ubyte ubSize;
                 ubyte aubBinData[TMP_ARR_SIZE13];
               } DataRecord;


typedef enum   {
                  FPR_BAUD_9600,
                  FPR_BAUD_14400,
                  FPR_BAUD_19200,
                  FPR_BAUD_28800,
                  FPR_BAUD_38400,
                  FPR_BAUD_57600,
                  FPR_BAUD_115200
               }  BaudRate;



typedef enum   {
                 PORT_COMM1 = NEXT_INDEX,
                 PORT_COMM2,
                 PORT_COMM3,
                 PORT_COMM4
               } COMM_PORT;

/* Flash Area/ Device IDs defines */
#define DEVICE_ID_INTERNAL_FLASH      0x40
#define DEVICE_ID_SERIAL_FLASH        0X44
#define DEVICE_ID_EXT_SERIAL_EEP      0X34
#define DEVICE_ID_EXT_PARALLEL_FLASH  0X42

/* File Code Defines */
#define BOOTSTRAP_FILE                0x01
#define FLASHWRITER_FILE              0x02
#define SERIAL_EEP_FILE               0x04
#define SERIAL_FLASH_FILE             0x08
#define PARALLEL_FLASH_FILE           0x10
#define INTERNAL_FLASH_FILE           0x20
#define SUMMARY_FILE                  0x40
#define FLASH_BIN_FILE                0x80


/*V22*/
#define FMG_PROGRAM                   0x01
#define FMG_CONNECT                   0x02
#define FMG_DISCONNECT                0x03
#define FMG_PROGRAM_MASK              0x7F

#define FMG_EEPDOWNLOAD_STARTUP       0x00
#define FMG_EEPDOWNLOAD_DELIVERY      0x80
#define FMG_EEPDOWNLOAD_MASK          0x80

#endif



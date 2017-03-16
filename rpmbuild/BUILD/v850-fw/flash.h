/*****************************************************************************/
#ifndef _FLASH_H
#define _FLASH_H

#ifdef LINUX
  #define __stdcall
#endif


/*------------------------------------------------------- timing definitions */

#define kT1                               1000                     /*   1 ms */
#define kT2                               10000                    /*  10 ms */
#define kT3                               1000                     /*   1 ms */
#define kT4                               15000                     /* 15 ms */
#define kT5                               150000                   /* 150 ms */
#define kT6                               1000                     /*   1 ms */

#define kCOM                              "COM1"
#define kIntroBaud                        9600
#define kTuaregIntroBaud                  38400
#define kConnectBaud                      19200
#define kFlashBaud                        115200


#define INTROL_START_V850E2M              0xFEDFDC00


#define INTROL_START_V850                 0x03FFC000

/*------------------------------------------------------ program definitions */

#define FLASH_MAUS_TIMEOUT                250
#define FLASH_RESET                       true   // Value to set Reset active
#define FLASH_VPP                         true      // Value to switch on VPP

#define DNL_CRC_DEFAULT_CRC               0x00000000
#define DNL_CRC_GREEN_POLYNOM             0x10211021  // polynom for CRC generation

#define FLASH_MAX_DEV                     32     // max. supported devicetypes
#define FLASH_VERSION                     0x52   // Version der Flash-Library
#define LOG_FILE_WRITE(a)                 LogFileWrite(a) // enable logfile-fct
//#define LOG_FILE_WRITE(a)
#define DF_PLAUSI_CHECK
                                           // disable VPP and VCC-Drop monitor
#define FLASH_DROP_OFF                    (FLASH_MODE_VCC+FLASH_MODE_VPP)

#define FLASH_MAUS_LVL_0                  0
#define FLASH_MAUS_LVL_1                  1
#define FLASH_MAUS_LVL_0P                 2
/*------------------------------------------------------ type definitions */

typedef struct
{
  ulword ulwMode;                                 /* Programming Method, etc */
  ulword ulwRNDNum;                                         /* Random Number */
  ulword ulwMaxFrameSize;           /* max. Frame size of Chip communication */
  uword  uwTestManager;                     /* Testmanager to be downloaded */
  FILE * f_DFile;                           /* File pointer to Download File */
  ubyte  *pIntroLoader;                           /* ptr to Introloader data */
  ubyte  *pFlashWriter;                           /* ptr to Flashwriter data */
  vect   pProgressInfo;                           /* ptr to backcall function*/
  vect   pComDebug;                               /* ptr to backcall function*/
  vect   pErrMsg;                                 /* ptr to backcall function*/
  vect   pGetMKey;                                /* ptr to backcall function*/
  vect   pCDMBXchg;                               /* ptr to backcall function*/
  vect   pKDSXchg;                                /* ptr to backcall function*/
} Flash_TYInternalParms;

typedef struct
{ uword  Typ;                                               /* Typ of Header */
  long   Pos;                                     /* File position of Header */
  ulword Start;                                    /* Startaddress of Device */
  ulword Length;                                     /* Length of Data block */
  ulword CS;                                       /* Checksum of Data block */
} Flash_TYInfo;

typedef struct
{ Flash_TYInfo Header[FLASH_MAX_DEV];
  unsigned char Count;
} Flash_strTYInfo;

#endif                                                   /* #ifndef _FLASH_H */
/*****************************************************************************/

#ifndef _FLASH850_H
#define _FLASH850_H

#include "globals.h"
#ifdef FTDI_SUPPORT
#include "usbserial.h"
#else
#include "serial.h"
#endif
/*--------------------------------------------------------- Type definitions */
                                         /*  Umrechnung von Dezimal nach BCD */
#define  DEZ_BCD(i)   ( ((i / 10) << 4)  |  i % 10 )
                                         /*  Umrechnung von BCD nach Dezimal */
#define  BCD_DEZ(i)   ( (i & 0xF) + (10 * ((i & 0xF0) >> 4)) )


                                            // Type for process-Status bc-fc
typedef ulword (*vect)(char *Data, bool newline);
typedef struct                              // struct for parameter interchange
{
  uword uwTelegramTimeOut;                  // [100 ...1000ms], typical 250ms
  ubyte ubTelegramLength;                   // [1 ..255], typical 0x70 [0x80]
  ubyte ubVerify;                           // [ON = 1, OFF = 0), typical is ON
  ubyte ubReadSignature;                    // [ON = 1, OFF = 0), typical is OFF
  ubyte ubTelegramRetries;                  // [1..10], typical 5
  ubyte ubProgrammingRetries;               // [1..10], typical 2
  ubyte ubEraseTimeOut;                     // [1 ..100s], typical 20s
  ubyte ubWriteTimeOut;                     // [1 .. 100s], typical 5s
  uword uwPreDnlDelay;                      // [10..1000ms], typical 100 ms
  uword uwFLWTimeOut;                       // [100..2000ms], typical 250 ms
  ubyte ubWriteRandomEnable;                // [0..1], typical 1
  ubyte ubLogEnable;                        // [0..1], typical 1
  ubyte ubIgnoreOptionFlags;                // [ON = 1, OFF = 0), typical is OFF
  RS232Typ *pRS232;                         // ptr to RS232-Connection Parms
  char  *LogPath;                           // Path for Logfilewrite
  char  *GPIO_Reset;                        // GPIO-Pin for Reset
  char  *GPIO_VPP;                          // GPIO-Pin for DNL-Enable
} Flash_TYParms;

typedef struct                              // struct for Device parameter
{
  ubyte ubToggleCounts;                     // # of Togglecounts
  ulword ulwRomSize;                        // max. Romsize of Device
  ulword ulwSysClock;                       // Systemclock des Devices
  ulword ulwOscClock;                       // Oscillatorclock des Devices
  ubyte aubSysOpt[4];                       // System Option Bytes des Devices
  ubyte aubSysSec[6];                       // System Security Bytes des Devices
  ubyte aubSysOCD[12];                      // System Debug Key des Devices
  union
  { ubyte aubDevice[18];                    // Silicon Signature Info
    struct
    { ubyte aubVendor[3];
      ubyte aubRomSize[4];
      ubyte aubSignature[11];
    } Parts;
  } uDevSign;
  ulword ulwPM_F0;                          // Vdd -> Vdd2 Wait
  ulword ulwPM_F1;                          // Vdd -> FLMD Wait
  ulword ulwPM_F2;                          // FLMD -> Reset Wait
  ulword ulwPM_F3;                          // Reset -> Start Toggle Wait
  ulword ulwPM_F4;                          // FLMD0 puls width
  ulword ulwPM_F5;                          // FLMD0 puls_end sync detection Wait
  ulword ulwPM_F6;                          // FLMD0 puls_end sync detection Timeout
  ulword ulwPM_F7;                          // Reset Cmd Wait
  ulword ulwPM_F8;                          // Reset Cmd Ack Timeout
  ulword ulwPM_F9;                          // Number of reset retries
  ulword ulwPM_F10;                         // wait for frequency calc
  ulword ulwPM_F11;                         // Reset -> Vpp off
  ulword ulwPM_01;                          // Recv Ack -> Send Cmd/Data Wait
  ulword ulwPM_08;                          // Send Cmd/Data Wait->Recv Ack Timeout
  ulword ulwMaxBlindBuf;                    // Rambuffer size of BlindROM
  ulword ulwBlindBuf;                       // useable Rambuffer size of BlindROM
  ulword ulwPM_14;                          // Interbyte Time Data->Data Wait
  ulword ulwPM_18;                          // Interbyte Timeout Data->Data
  ulword ulwMAUS_Buf;                       // Rambuffer size of MAUS
  ulword ulwPM_M4;                          // Interbyte Time Data->Data Wait
  ulword ulwPM_M8;                          // Interbyte Timeout Data->Data
} Flash_TYDevParms;

/*----------------------------------------------------- Return value defines */

/*--------------------------------------------------- From connect functions */
#define FLASH_SUCCESS                     0x00   // no error
#define FLASH_NO_SUCCESS                  -1     // common error
#define FLASH_DF_FILE_HEADER_ERROR        0x30   // Unknown Type in DF-Header
#define FLASH_DF_FILE_CRC_ERROR           0x31   // Header- or Data block- mismatch
#define FLASH_DF_FILE_IO_ERROR            0x32   // File-Open Error in DF
#define FLASH_DF_FILE_CONTENT_ERROR       0x33   // DataBlock not found in DF-Header
#define FLASH_CONNECT_FAILED              0x34   // Device Connection did not response
#define FLASH_BOOTSTRAP_FAILED            0x35   // No response on Bootstrap request
#define FLASH_REQ_MAUS_FAILED             0x36   // No response from Maus request
#define FLASH_INTROLOADER_FAILED          0x37   // No response from Introloader
#define FLASH_FLASHWRITER_FAILED          0x38   // No response from Flashwriter
#define FLASH_INTROLOADER_DF_ERROR        0x39   // Error in Introloader Data-Block
#define FLASH_INTROLOADER_SIZE_TOO_BIG    0x3A   // Introloader > 256 Bytes
#define FLASH_FLASHWRITER_DF_ERROR        0x3B   // Error in Flashwriter Data-Block
#define FLASH_DEVINFO_FAILED              0x3C   // Request for DevInfo failed
#define FLASH_DEVINFO_REJECT              0x3D   // reject cmd, because Bootstrap
#define FLASH_BAUDRATE_SET_FAILED         0x3E   // cannot set Bautrate to init UART
#define FLASH_HISTORY_WRITE_FAILED        0x3F   // cannot write History to E2Prom
#define FLASH_VPP_DROP                    0x40   // Vpp drop detected by Hardware Interface
#define FLASH_VCC_DROP                    0x41   // Vcc drop detected by Hardware Interface
#define FLASH_ACCESS_DENIED               0x42   // no access during active tranfer
#define FLASH_NO_MKEY_EXE                 0x43   // Mkey.exe not found for connect
#define FLASH_WRONG_MKEYCODE              0x44   // wrong Mkey code generated
#define FLASH_NO_MKEY_SERVER              0x45   // Maus-Server not found
#define FLASH_DF_FILE_UNKNOWN_SIB_DATA    0x46   // Invalid data in device information (Version conflict!)
#define FLASH_DF_FILE_INVALID_SIB_VERSION 0x47   // not supported SIB-Version
#define FLASH_ABORTED                     0x50   // abort last command
#define FLASH_SEQ_ERROR                   0x60   // error during Maus-Sequence file
#define FLASH_MAUS_SEQ_FILE_IO_ERROR      0x61   // Maus-Sequence file open error
#define FLASH_INTROLOADER_CS_ERROR        0x70   // Error in CS for Introloader
#define FLASH_COMP_NUM_INVALID            0x71   // Compatibility Nr is different
#define FLASH_TESTMANAGER_FAILED          0x72   // No response from TestManager
#define FLASH_IDB_MISMATCH                0x80   // No success in comparing IDB
#define FLASH_INVALID_OPTIONSETTINGS      0x90   // FX4 has invalid option
#define FLASH_REJECT_OPTIONSETTINGS       0x91   // FX4 can't read Security Setting

/*------------------------------------------------------ From Maus functions */

#define FLASH_VPP_ERROR                   0x01   // Vpp error
#define FLASH_POWER_FAIL                  0x02   // gen. Pwr fail (low voltage)
#define FLASH_ERASE_FAIL                  0x03   // Device cannot be erased
#define FLASH_WRITE_FAIL                  0x04   // Device cannot be written
#define FLASH_ADDRESS_ERROR               0x05   // Address out of range
#define FLASH_ACCESS_ERROR                0x06   // Invalid Address access
#define FLASH_DEVICE_NOT_PRESENT          0x07   // Device not present
#define FLASH_DEVICE_PROTECTED            0x08   // Device protected by Hardware
#define FLASH_REJECT                      0x09   // command rejected
#define FLASH_OTHER_ERROR                 0x0A   // not yet defined error

/*----------------------------------------------------- From other functions */

#define FLASH_COM_SET_FAIL                0x10
#define FLASH_COM_WRITE_FAIL              0x11
#define FLASH_COM_READ_FAIL               0x12
#define FLASH_COM_TIMEOUT                 0x13
#define FLASH_PERFORM_TMR_NAV             0x14
#define FLASH_SYSTEM_ERROR                0x15   // common system error (alloc etc)
#define FLASH_PARM_ERROR                  0x16   // Port Parm error

#define FLASH_DEVICE_CRC_ERROR            0x23   // CRC-Error on verify with DNL
#define FLASH_DEVICE_NOT_BLANK            0x22
#define FLASH_DOWNLOAD_ERROR              0x21   // CRC-Error,
                                                 // Wrong Data Block Type or Nr

#define FLASH_MODE_VPP                    0x00000001 // disable VPP-Drop monitor
#define FLASH_MODE_VCC                    0x00000002 // disable VCC-Drop monitor
#define FLASH_MODE_HISTORY                0x00000008 // disable write Error History
#define FLASH_MODE_DATAFLASH_ERASE        0x00200000 // disable do dataflash erase before programming or connect
#define FLASH_MODE_CHIP_ERASE             0x00400000 // disable do chip erase before programming or connect
#define FLASH_MODE_KDS_SAVE               0x00800000 // disable KDS Save after virgin Flash
#define FLASH_MODE_TESTMODE               0x01000000 // disable Testmode
#define FLASH_MODE_FIX_LOAD_ADDR          0x02000000 // disable fix load address for introloader
#define FLASH_MODE_NON_CROSS_SCI          0x04000000 // disable normal connection of Rx-Tx-Signal State
#define FLASH_MODE_RESET_CONN_CB          0x08000000 // disable Reset during connection via Connector Block
#define FLASH_MODE_DFINFO                 0x10000000 // disable DF_INFO-Read
#define FLASH_MODE_UPDATE                 0x20000000 // disable Update Mode
#define FLASH_MODE_FAST_PROG              0x40000000 // disable Fast programming
#define FLASH_MODE_GLOB_ERASE             0x80000000 // disable Global erase

/*-------------------------------------------------- Device Type definitions */

#define DEVICE_V850                       00
#define DEVICE_TUAREG                     10
#define DEVICE_V850E                      20
#define DEVICE_V850E2M                    30
#define DEVICE_UNKNOWN                    0xFF

/*-------------------------------------------------- Derivat Type definitions */

#define DERIVAT_UNKNOWN                   0
#define DERIVAT_SF1                       1
#define DERIVAT_SC3                       2
#define DERIVAT_SF1_SC3                   3
#define DERIVAT_SG2                       4
#define DERIVAT_SJ2                       5
#define DERIVAT_SG3                       6
#define DERIVAT_SJ3                       7
#define DERIVAT_FL4H                      8
#define DERIVAT_FJ                        9
#define DERIVAT_FL                        10

/*----------------------------------------------------- Funktion definitions */

#ifdef LINUX
  #define DLLEXPORT
  #define __stdcall
  #include <unistd.h>
  #define FileExists(a) (0==access(a,F_OK))
#else
  #ifdef CppBuilder
    #ifdef FLASH_CPP
      #define DLLEXPORT extern "C" __declspec (dllexport)
    #else
      #define DLLEXPORT extern "C" __declspec (dllimport)
    #endif
  #else
    #ifdef __cplusplus
     #ifdef FLASH_CPP
      #define DLLEXPORT extern "C" __declspec (dllexport)
     #else
      #define DLLEXPORT extern "C" __declspec (dllimport)
     #endif
    #else
     #ifdef FLASH_CPP
      #define DLLEXPORT __declspec (dllexport)
     #else
      #define DLLEXPORT __declspec (dllimport)
     #endif
    #endif
  #endif
#endif

DLLEXPORT void   __stdcall Flash_vSetProgressBC(vect func);
DLLEXPORT void   __stdcall Flash_vSetDebugBC(vect func);
DLLEXPORT void   __stdcall Flash_vSetErrMsgBC(vect func);
DLLEXPORT void   __stdcall Flash_vSetMKeyBC(vect func);
DLLEXPORT void   __stdcall Flash_vSetCDMBXchgBC(vect func);
DLLEXPORT void   __stdcall Flash_vSetKDSXchgBC(vect func);
DLLEXPORT void   __stdcall Flash_vSetFrameSize(ulword FrameSize);
DLLEXPORT void   __stdcall Flash_vSetTestManager(uword BlockNumber);
DLLEXPORT ulword __stdcall Flash_ulwInit(ubyte  *pubCom, Flash_TYParms *pstFlashParms);
DLLEXPORT ulword __stdcall Flash_ulwShutDown (void);
DLLEXPORT ulword __stdcall Flash_ulwDoConnect (ubyte  *pubCom, ubyte *pubName, ulword ulwMde, ubyte ubCnt);
DLLEXPORT ulword __stdcall Flash_ulwDoDisconnect (void);
DLLEXPORT ulword __stdcall Flash_ulwDoReset (void);
//DLLEXPORT ulword __stdcall Flash_ulwGetKDSInfo (ulword *KDSStart, ulword *KDSSize);
DLLEXPORT ulword __stdcall Flash_ulwGetDeviceInfo (ubyte *DeviceInfo);
DLLEXPORT ulword __stdcall Flash_ulwGetParms (Flash_TYParms *FlashParms);
DLLEXPORT ulword __stdcall Flash_ulwSetParms (Flash_TYParms *FlashParms);
DLLEXPORT ulword __stdcall Flash_ulwDoDownloadFileTransfer(uword *Blk, ulword Mode, __int64 RNDNum);
DLLEXPORT ulword __stdcall Flash_ulwGetDFDeviceContent (ubyte *pubDF_Filename, uword  *Blk, ulword ulwMode, ubyte ubInit, ubyte **Memory, ulword *Start, ulword *Size);
DLLEXPORT ulword __stdcall Flash_ulwFreeMemory (ubyte **Memory);
DLLEXPORT ulword __stdcall Flash_ulwGetDFContents (ubyte *pubFile, ubyte **pubText);
DLLEXPORT ulword __stdcall Flash_ulwGetDFParms (ubyte *pubFile, Flash_TYDevParms * pstDevParms);
DLLEXPORT ulword __stdcall Flash_ulwSetDFParms (Flash_TYDevParms * pstDevParms);
DLLEXPORT ulword __stdcall Flash_ulwWriteMemory (ubyte Type, ubyte *Src, ulword DestAdr, ulword Length);
DLLEXPORT ulword __stdcall Flash_ulwReadMemory (ubyte Type, ubyte *Dest, ulword SrcAdr, ulword Length);
DLLEXPORT ulword __stdcall Flash_ulwBlankCheck (ubyte Type);
DLLEXPORT ulword __stdcall Flash_ulwEraseMemory (ubyte Type, ulword StartAdr, ulword Length);
DLLEXPORT ulword __stdcall Flash_ulwGetCRCValue (ubyte Type, ulword StartAdr, ulword Length, ulword *CRC);
DLLEXPORT ulword __stdcall Flash_ulwDoVerify (uword  *Blk, uword *uwError);
DLLEXPORT ulword __stdcall Flash_ulwMausSend (ubyte ubCmd, ubyte *pubBuf, ubyte ubLen);
DLLEXPORT ulword __stdcall Flash_ulwMausReceive (ubyte ubCmd, ubyte *pubBuf, ubyte *ubLen, ulword ulwRetries);
DLLEXPORT ulword __stdcall Flash_ulwMausChangeParms (ubyte *pubCom);
DLLEXPORT ulword __stdcall FLASH_ulwGetRSASignature(uword Block,ubyte **RetBuf);
DLLEXPORT char*  __stdcall FLASH_pcGetOptLogInfo(void);
#ifdef DROP_READ_TEST
DLLEXPORT ulword __stdcall Flash_ulwReadCTS (void);
DLLEXPORT ulword __stdcall Flash_ulwReadDSR (void);
#endif
DLLEXPORT void   __stdcall Flash_vXmData (char *Buf, ulword Len);
DLLEXPORT bool   __stdcall Flash_bRmData (char *Buf, ulword ulMaxSize, ulword *ulRecCount);


#define MAX_DEVICE_TYPES  60

typedef struct
{
  char Name[11];
  int Type;
  int Derivat;
} stDevice;


#ifdef OPTIONFORM_CPP
static stDevice cstDevice[MAX_DEVICE_TYPES] = { {"D70F3079Y",  DEVICE_V850,  DERIVAT_SF1}
                                        ,{"D70F3089Y",  DEVICE_V850,  DERIVAT_SF1}
                                        ,{"D70F3261Y",  DEVICE_V850E, DERIVAT_SG2}
                                        ,{"D70F3263Y",  DEVICE_V850E, DERIVAT_SG2}
                                        ,{"D70F3281Y",  DEVICE_V850E, DERIVAT_SG2}
                                        ,{"D70F3283Y",  DEVICE_V850E, DERIVAT_SG2}
                                        ,{"D70F3264Y",  DEVICE_V850E, DERIVAT_SJ2}
                                        ,{"D70F3266Y",  DEVICE_V850E, DERIVAT_SJ2}
                                        ,{"D70F3284Y",  DEVICE_V850E, DERIVAT_SJ2}
                                        ,{"D70F3286Y",  DEVICE_V850E, DERIVAT_SJ2}
                                        ,{"D70F3288Y",  DEVICE_V850E, DERIVAT_SJ2}
                                        ,{"D70F3335",   DEVICE_V850E, DERIVAT_SG3}
                                        ,{"D70F3336",   DEVICE_V850E, DERIVAT_SG3}
                                        ,{"D70F3337",   DEVICE_V850E, DERIVAT_SJ3}
                                        ,{"D70F3339",   DEVICE_V850E, DERIVAT_SJ3}
                                        ,{"D70F3345",   DEVICE_V850E, DERIVAT_SJ3}
                                        ,{"D70F3347",   DEVICE_V850E, DERIVAT_SJ3}
                                        ,{"D70F3349",   DEVICE_V850E, DERIVAT_SJ3}
                                        ,{"D70F3350",   DEVICE_V850E, DERIVAT_SG3}
                                        ,{"D70F3351",   DEVICE_V850E, DERIVAT_SG3}
                                        ,{"D70F3352",   DEVICE_V850E, DERIVAT_SG3}
                                        ,{"D70F3353",   DEVICE_V850E, DERIVAT_SG3}
                                        ,{"D70F3354",   DEVICE_V850E, DERIVAT_SJ3}
                                        ,{"D70F3355",   DEVICE_V850E, DERIVAT_SJ3}
                                        ,{"D70F3356",   DEVICE_V850E, DERIVAT_SJ3}
                                        ,{"D70F3357",   DEVICE_V850E, DERIVAT_SJ3}
                                        ,{"D70F3358",   DEVICE_V850E, DERIVAT_SJ3}
                                        ,{"D70F3359",   DEVICE_V850E, DERIVAT_SJ3}
                                        ,{"D70F3364",   DEVICE_V850E, DERIVAT_SJ3}
                                        ,{"D70F3365",   DEVICE_V850E, DERIVAT_SJ3}
                                        ,{"D70F3366",   DEVICE_V850E, DERIVAT_SJ3}
                                        ,{"D70F3367",   DEVICE_V850E, DERIVAT_SJ3}
                                        ,{"D70F3368",   DEVICE_V850E, DERIVAT_SJ3}
                                        ,{"D70F3369",   DEVICE_V850E, DERIVAT_SJ3}
                                        ,{"MASK_V850_", DEVICE_V850,  DERIVAT_SF1_SC3}
                                        ,{"MASK850SG2", DEVICE_V850E, DERIVAT_SG2}
                                        ,{"MASK850SJ2", DEVICE_V850E, DERIVAT_SJ2}
                                        ,{"MASK850SG3", DEVICE_V850E, DERIVAT_SG3}
                                        ,{"MASK850SJ3", DEVICE_V850E, DERIVAT_SJ3}
                                        ,{"EMULATOR_",  DEVICE_V850,  DERIVAT_SF1_SC3}
                                        ,{"EMUV850SX2", DEVICE_V850E, DERIVAT_UNKNOWN}
                                        ,{"EMUV850SX3", DEVICE_V850E, DERIVAT_UNKNOWN}
                                        ,{"EMUV850SG2", DEVICE_V850E, DERIVAT_SG2}
                                        ,{"EMUV850SJ2", DEVICE_V850E, DERIVAT_SJ2}
                                        ,{"TUAREG",     DEVICE_TUAREG,DERIVAT_UNKNOWN}
                                        ,{"STA2052",    DEVICE_TUAREG,DERIVAT_UNKNOWN}
                                        ,{"D70F3551",   DEVICE_V850E2M,DERIVAT_FJ}
                                        ,{"D70F3552",   DEVICE_V850E2M,DERIVAT_FJ}
                                        ,{"D70F3553",   DEVICE_V850E2M,DERIVAT_FJ}
                                        ,{"D70F3554",   DEVICE_V850E2M,DERIVAT_FJ}
                                        ,{"D70F4003",   DEVICE_V850E2M,DERIVAT_FJ}
                                        ,{"D70F4004",   DEVICE_V850E2M,DERIVAT_FJ}
                                        ,{"D70F4005",   DEVICE_V850E2M,DERIVAT_FJ}
                                        ,{"D70F4006",   DEVICE_V850E2M,DERIVAT_FJ}
                                        ,{"D70F3559",   DEVICE_V850E2M,DERIVAT_FL}
                                        ,{"D70F3560",   DEVICE_V850E2M,DERIVAT_FL}
                                        ,{"D70F4011",   DEVICE_V850E2M,DERIVAT_FL}
                                        ,{"D70F4012",   DEVICE_V850E2M,DERIVAT_FL}
                                        ,{"D70F3564",   DEVICE_V850E2M,DERIVAT_FL4H}
                                        ,{"D70F3565",   DEVICE_V850E2M,DERIVAT_FL4H}
                                       };
#endif
#ifdef FLASH_CPP
static stDevice cstDevice[MAX_DEVICE_TYPES] = { {"D70F3079Y",  DEVICE_V850,  DERIVAT_SF1}
                                        ,{"D70F3089Y",  DEVICE_V850,  DERIVAT_SF1}
                                        ,{"D70F3261Y",  DEVICE_V850E, DERIVAT_SG2}
                                        ,{"D70F3263Y",  DEVICE_V850E, DERIVAT_SG2}
                                        ,{"D70F3281Y",  DEVICE_V850E, DERIVAT_SG2}
                                        ,{"D70F3283Y",  DEVICE_V850E, DERIVAT_SG2}
                                        ,{"D70F3264Y",  DEVICE_V850E, DERIVAT_SJ2}
                                        ,{"D70F3266Y",  DEVICE_V850E, DERIVAT_SJ2}
                                        ,{"D70F3284Y",  DEVICE_V850E, DERIVAT_SJ2}
                                        ,{"D70F3286Y",  DEVICE_V850E, DERIVAT_SJ2}
                                        ,{"D70F3288Y",  DEVICE_V850E, DERIVAT_SJ2}
                                        ,{"D70F3335",   DEVICE_V850E, DERIVAT_SG3}
                                        ,{"D70F3336",   DEVICE_V850E, DERIVAT_SG3}
                                        ,{"D70F3337",   DEVICE_V850E, DERIVAT_SJ3}
                                        ,{"D70F3339",   DEVICE_V850E, DERIVAT_SJ3}
                                        ,{"D70F3345",   DEVICE_V850E, DERIVAT_SJ3}
                                        ,{"D70F3347",   DEVICE_V850E, DERIVAT_SJ3}
                                        ,{"D70F3349",   DEVICE_V850E, DERIVAT_SJ3}
                                        ,{"D70F3350",   DEVICE_V850E, DERIVAT_SG3}
                                        ,{"D70F3351",   DEVICE_V850E, DERIVAT_SG3}
                                        ,{"D70F3352",   DEVICE_V850E, DERIVAT_SG3}
                                        ,{"D70F3353",   DEVICE_V850E, DERIVAT_SG3}
                                        ,{"D70F3354",   DEVICE_V850E, DERIVAT_SJ3}
                                        ,{"D70F3355",   DEVICE_V850E, DERIVAT_SJ3}
                                        ,{"D70F3356",   DEVICE_V850E, DERIVAT_SJ3}
                                        ,{"D70F3357",   DEVICE_V850E, DERIVAT_SJ3}
                                        ,{"D70F3358",   DEVICE_V850E, DERIVAT_SJ3}
                                        ,{"D70F3359",   DEVICE_V850E, DERIVAT_SJ3}
                                        ,{"D70F3364",   DEVICE_V850E, DERIVAT_SJ3}
                                        ,{"D70F3365",   DEVICE_V850E, DERIVAT_SJ3}
                                        ,{"D70F3366",   DEVICE_V850E, DERIVAT_SJ3}
                                        ,{"D70F3367",   DEVICE_V850E, DERIVAT_SJ3}
                                        ,{"D70F3368",   DEVICE_V850E, DERIVAT_SJ3}
                                        ,{"D70F3369",   DEVICE_V850E, DERIVAT_SJ3}
                                        ,{"MASK_V850_", DEVICE_V850,  DERIVAT_SF1_SC3}
                                        ,{"MASK850SG2", DEVICE_V850E, DERIVAT_SG2}
                                        ,{"MASK850SJ2", DEVICE_V850E, DERIVAT_SJ2}
                                        ,{"MASK850SG3", DEVICE_V850E, DERIVAT_SG3}
                                        ,{"MASK850SJ3", DEVICE_V850E, DERIVAT_SJ3}
                                        ,{"EMULATOR_",  DEVICE_V850,  DERIVAT_SF1_SC3}
                                        ,{"EMUV850SX2", DEVICE_V850E, DERIVAT_UNKNOWN}
                                        ,{"EMUV850SX3", DEVICE_V850E, DERIVAT_UNKNOWN}
                                        ,{"EMUV850SG2", DEVICE_V850E, DERIVAT_SG2}
                                        ,{"EMUV850SJ2", DEVICE_V850E, DERIVAT_SJ2}
                                        ,{"TUAREG",     DEVICE_TUAREG,DERIVAT_UNKNOWN}
                                        ,{"STA2052",    DEVICE_TUAREG,DERIVAT_UNKNOWN}
                                        ,{"D70F3551",   DEVICE_V850E2M,DERIVAT_FJ}
                                        ,{"D70F3552",   DEVICE_V850E2M,DERIVAT_FJ}
                                        ,{"D70F3553",   DEVICE_V850E2M,DERIVAT_FJ}
                                        ,{"D70F3554",   DEVICE_V850E2M,DERIVAT_FJ}
                                        ,{"D70F4003",   DEVICE_V850E2M,DERIVAT_FJ}
                                        ,{"D70F4004",   DEVICE_V850E2M,DERIVAT_FJ}
                                        ,{"D70F4005",   DEVICE_V850E2M,DERIVAT_FJ}
                                        ,{"D70F4006",   DEVICE_V850E2M,DERIVAT_FJ}
                                        ,{"D70F3559",   DEVICE_V850E2M,DERIVAT_FL}
                                        ,{"D70F3560",   DEVICE_V850E2M,DERIVAT_FL}
                                        ,{"D70F4011",   DEVICE_V850E2M,DERIVAT_FL}
                                        ,{"D70F4012",   DEVICE_V850E2M,DERIVAT_FL}
                                        ,{"D70F3564",   DEVICE_V850E2M,DERIVAT_FL4H}
                                        ,{"D70F3565",   DEVICE_V850E2M,DERIVAT_FL4H}
                                       };
#endif
#endif


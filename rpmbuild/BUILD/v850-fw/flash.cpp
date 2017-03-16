/*************************************************************************@DA*
* @Filename          : flash.cpp
* @Module            : DLL-Interface
* @Project           : flash850.dll
*----------------------------------------------------------------------------
* @Copyright         : (c) 2001 Robert Bosch GmbH, Hildesheim
* @Author            : CM-CR / ESD4  Peter Tuschik
*----------------------------------------------------------------------------
* @Description       : exporting functions:
*                      Flash_vSetProgressBC()
*                      Flash_vSetDebugBC()
*                      Flash_vSetErrMsgBC()
*                      Flash_vSetMKeyBC()
*                      Flash_vSetCDMBXchgBC()
*                      Flash_vSetFrameSize()
*                      Flash_vSetTestManager()
*                      Flash_ulwInit ()
*                      Flash_ulwDoConnect ()
*                      Flash_ulwDoDisconnect ()
*                      Flash_ulwDoReset ()
*                      Flash_ulwGetDeviceInfo ()
*                      FLASH_ulwGetDeviceContent ()
*                      FLASH_ulwFreeMemory ()
*                      Flash_ulwGetParms ()
*                      Flash_ulwSetParms ()
*                      Flash_ulwShutdown ()
*
*                      Flash_ulwDoDownloadFileTransfer ()
*                      Flash_ulwGetDFContents ()
*                      Flash_ulwGetDFParms ()
*                      Flash_ulwSetDFParms ()
*                      Flash_ulwGetProcessStatus ()
*                      Flash_ulwGetDFDeviceContent ()
*
*                      Flash_ulwWriteMemory ()
*                      Flash_ulwReadMemory ()
*                      Flash_ulwEraseMemory ()
*                      Flash_ulwGetCRCValue ()
*                      Flash_ulwDoVerify ()
*
*                      Flash_ulwMausSend ()
*                      Flash_ulwMausReceive ()
*                      Flash_ulwMausChangeParms()
*                      FLASH_ulwGetRSASignature()
*                      FLASH_pcGetOptLogInfo()
*
*----------------------------------------------------------------------------
* @Runtime environment: Win95/WinNT
*----------------------------------------------------------------------------
*
* @History           : Initial Version
****************************************************************************/
#define  FLASH_CPP


#ifdef WINDOWS_CE
long filelength(FILE *DF_file)
{
  fseek(DF_file,0,SEEK_END);
  long FileLen = ftell(DF_file);
  fseek(DF_file,0,0);
  return FileLen;
}
#endif
#ifdef LINUX
  #include <stdio.h>
  #include <ctype.h>
  #include <sys/time.h>
  #include <sys/resource.h>
  #include <sys/stat.h>

  #define GetCurrentProcess()    getpid()
  #define SetPriorityClass(a,b)  setpriority(PRIO_PROCESS, a, (b == REALTIME_PRIORITY_CLASS) ? -10 : 0);
  #define GetLocalTime(a)        GetSystemTime(a)
  #define SYSTEMTIME timeval
  #define REALTIME_PRIORITY_CLASS 0
  #define NORMAL_PRIORITY_CLASS 1

long filelength(FILE *DF_file)
{
  fseek(DF_file,0,SEEK_END);
  long FileLen = ftell(DF_file);
  fseek(DF_file,0,0);
  return FileLen;
}

void GetSystemTime (SYSTEMTIME *stime)
{
   gettimeofday(stime,0);
}
#else
  #include <windows.h>
  #include <io.h>
  #define filelength(a) filelength(fileno(a))
#endif
#include <math.h>
#include <stdarg.h>
#include "malloc.h"

/********************************************************** Module Includes */
#include "globals.h"
#ifdef FTDI_SUPPORT
  #include "usbserial.h"
#else
  #include "serial.h"
  #define PortSwitch(a,b,c)
#endif
#include "MausBus.h"
#include "Timing.h"
#include "Flash850.h"
#include "flash.h"

/******************************************************* Internal variables */

RS232Typ              *pFlashPort;              /* ptr to RS232 Port-config */
RS232Typ              *pConnectPort;            /* ptr to RS232 Port-config */
Flash_TYParms         FlashParms;                  /* Application-parameter */
Flash_TYDevParms      DevParms;                         /* Device-parameter */
Flash_TYInternalParms IntParms;                            /* DLl-parameter */
Flash_strTYInfo       DFInfo;                      /* array for header data */
unsigned char         *DFInfoBuffer;          /* buffer for DF-Content Info */
bool                  blFlashActive;         /* active flashing in progress */
bool                  blAbort,blConnected;     /* flag to abort last action */
bool                  blUpdate;                       /* Update Programming */
static  unsigned char ucBuffer[1024];                 /* global Maus-buffer */
ubyte                 aubEraseDev[FLASH_MAX_DEV]; /* Info of erased devices */
bool                  blProgSuppress;/* suppress programming of erased data */
ubyte                 aubDevInfo[120];             /* read device Info Data */
static char           LogFileName[1024];       /* Pfad und Name für LogFile */
static ubyte          ubDevice;              /* Merker für aktuelles Device */
ulword                OS_Delay,DebOS_Delay;/* Wartezeit fürs Betriebssystem */
bool                  ASCI;                /* ASCI, statt SCI Kommunikation */
bool                  blDebug;                   /* Debugging mode for Roli */
bool                  blErased;     /* Device blank because chip was erased */
bool                  blProgInvalid;           /* Int. Flash is MASK or EMU */
bool                  BlockEra40_Ena;     /* Support Blockerase für Dev. 40 */
uword                 *BlockList;        /* Liste mit gültigen Blocknummern */
ubyte                 *ApplMemory;   /* Pointer für DFContentRead der Appl. */
ulword                SysClock;        /* System-Clock frequency for Device */
bool                  SX2, SX3, FX4; /* SX2 needs multiple of 16 for Tx-buf */
                                              /* Ptr to Software Info Block */
unsigned long         CDMB_Offset;                   /* Offset of CDMB_Data */
unsigned long         KDS_Offset;                     /* Offset of KDS_Data */
unsigned long         RSA_Offset;                     /* Offset of RSA Keys */
unsigned char         aubComNr[15];                     /* Compatibility Nr */
unsigned char         aubDFNr[11];                      /* Download File Nr */
unsigned int          EchoCom; /* Echo communication because of 1 wire comm */
unsigned char         FX4Virgin, FX4Default;   /* Flags for FX4 Optionstate */
unsigned char         *KDS_Buf;
unsigned long         KDS_Size;
unsigned char         OptLog[512];  /* Buffer for received option byte data */
ulword                ulwDataFlash;           /* Start-Address of DataFlash */
/******************************************************* External variables */

/********************************************************** Local Functions */
ulword DFInfo_ulwInit(FILE *file,Flash_strTYInfo *DF);
ulword DFInfo_ulwGetLen(Flash_strTYInfo *DFInfo,uword Typ);
ulword DFInfo_ulwGetStart(Flash_strTYInfo *DFInfo,uword Typ);
ulword DFInfo_ulwGetCRC(Flash_strTYInfo *DFInfo,uword Typ);
int    DFInfo_blReadData(FILE *DF_file,Flash_strTYInfo *DFInfo,ubyte **ptr, uword Typ);
ulword DFInfo_blReadDevParms(FILE *DF_file,Flash_strTYInfo *Info,Flash_TYDevParms *pstDevParms);
ulword ReqMausLevel(ulword DropCheck, ulword InitPort, ubyte &MausLevel);
void   ComSettings(char *pubStr,RS232Typ **ptr);
bool   Flash_blInitBootstrap(void);
bool   Flash_vIntroLoaderPatchBaud(RS232Typ *pPort);
ulword Flash_ulwDownload(uword Typ,__int64 ulwRNDNum);
ulword Flash_ulwWrite(ubyte Typ,unsigned char *ptr,ulword Dest,ulword Len);
ulword Flash_ulwErase(ubyte Typ,ulword Dest,ulword Len,__int64 ulwRNDNum);
ulword Flash_ulwReadDeviceInfo(ubyte *);
ulword SetDropDetection(ulword Set);
void   LogFileWrite(const char* arg_list, ...);
void   ProgressUpdate(const char* arg_list, ...);
void   ProgressRefresh(const char* arg_list, ...);
char   *GetTimeDifference(SYSTEMTIME t0);
ulword SetBlindBuf(ulword Size);

void   Flash_vWriteErrHistory(uword Mem,ulword Result);
bool   Flash_blReadErrHistory(unsigned char *buffer,unsigned char **ptr);
unsigned int Flash_ulwGetCRC(unsigned char ubData, unsigned int ulwPrevCRC);
unsigned int Flash_ulwVAGCRC(unsigned char ubData, unsigned int ulwPrevCRC, unsigned int ulwPolynom);
unsigned int Flash_ulwBPCRC(unsigned char ubData, unsigned int ulwPrevCRC, unsigned int ulwPolynom);

unsigned int Flash_uiGetBlank( unsigned char ucMemType);
unsigned int Flash_uiWriteRandom(unsigned char MemTyp,__int64 ulwRNDNum);
unsigned int Flash_uiErase  (unsigned char ucMemType,
                             unsigned int  uiStart,
                             unsigned int  uiLength,
                             __int64       uint64RandomNo);
unsigned int Flash_uiGetCRC (unsigned char ucMemType,
                             unsigned int  uiStartAddress,
                             unsigned int  uiLength,
                             unsigned int  *uiValue);
unsigned int Flash_uiFill   (unsigned char ucMemType,
                             unsigned int  uiStartAddress,
                             unsigned int  uiLength,
                             unsigned char ucValue);
unsigned int Flash_uiRead   (unsigned char ucMemType,
                             unsigned int  uiStartAddress,
                             unsigned char ucLength,
                             unsigned char *ucValue);
unsigned int Flash_uiWrite  (unsigned char ucMemType,
                             unsigned int  uiStartAddress,
                             unsigned char ucLength,
                             unsigned char *pucValue);
ulword Flash_ulwMausResult(ubyte ucMemType, unsigned int start, unsigned int len);
ulword MausSendWaitAck(RS232Typ *pPort, unsigned char cmd,
                                        unsigned int XmLen,
                                        unsigned long ulTries,
                                        unsigned int uiMode);
bool ChkMausSend (RS232Typ *pPort, unsigned char cCommand,
                                   unsigned char *pcDataBuffer,
                                   unsigned char cLength);
bool WaitChkMausSend (RS232Typ *pPort, unsigned char cCommand,
                                   unsigned char *pcDataBuffer,
                                   unsigned char cLength);
bool SendFrame(char *pcBuffer, unsigned long lLength);
bool SendFrameMsg(unsigned char Start, int len, unsigned char *pcBuffer, unsigned char End=0x3, int Ack=1, int TimeOut=1);
ulword SendChipErase(void);
unsigned long GetV850Info(char * Text, unsigned char code);
unsigned long SetV850Info(unsigned char code);
ulword ReadPortSendResult(void);
unsigned char DecodeDevice(char *pcBuffer, int *Derivat=NULL);
unsigned char *ConvertSignature(unsigned char *DeviceInfo, unsigned char *Buffer);
bool bCheckValidBlock(uword Typ);
bool bCheckCompatibility(void);
ulword ulwGetDeviceSize(unsigned char ucMemType);
ulword CreateConfigDataMemBlock(bool blUpdate);
ulword CreateKDSDataMemBlock(void);
ulword FGS_ulwDownload(ulword ulwMode);
void CopyU32ToBuf(unsigned char *ptr, unsigned long value);
const char* toUpper(const char* str);
ulword GetMyKDSData(char *ptrKDS, bool blDummy);


typedef struct
{ ulword baud;
  unsigned char val1;
  unsigned char val2;
  unsigned char val3;
} BaudStruct;
                            // Baud    KReg  MReg1 MReg0
BaudStruct V850_Set[] =  { { 9600,   0xD0, 0x00, 0x03 },
                           { 14400,  0x8B, 0x00, 0x03 },
                           { 19200,  0xD0, 0x00, 0x02 },
                           { 28800,  0x8B, 0x00, 0x02 },
                           { 38400,  0xD0, 0x00, 0x01 },
                           { 57600,  0x8B, 0x00, 0x01 },
                           { 115200, 0x45, 0x00, 0x01 },
                           { 230400, 0x23, 0x00, 0x01 },
                           { 460800, 0x11, 0x00, 0x01 },
                           { 615300, 0x0D, 0x00, 0x01 },
                           { 806400, 0x0A, 0x00, 0x01 },
                           { 1000000, 0x08, 0x00, 0x01 },
                           { 0, 0x0, 0x00, 0x00 },
                           { 0, 0x0, 0x00, 0x00 },
                           { 0, 0x0, 0x00, 0x00 }
                         };
                          // Baud    CTL2  CTL1  OPT
BaudStruct SX2_16_Set[] = { { 9600,   0x0D, 0x06, 0x00 },
                           { 14400,  0x45, 0x03, 0x00 },
                           { 19200,  0x0D, 0x05, 0x00 },
                           { 28800,  0x8B, 0x01, 0x00 },
                           { 38400,  0x0D, 0x04, 0x00 },
                           { 57600,  0x45, 0x01, 0x00 },
                           { 115200, 0x45, 0x00, 0x00 },
                           { 230400, 0x23, 0x00, 0x00 },
                           { 460800, 0x11, 0x00, 0x00 },
                           { 615300, 0x0D, 0x00, 0x00 },
                           { 806400, 0x0A, 0x00, 0x00 },
                           { 1000000, 0x08, 0x00, 0x00 },
                           { 0, 0x0, 0x00, 0x00 },
                           { 0, 0x0, 0x00, 0x00 },
                           { 0, 0x0, 0x00, 0x00 }
                         };
                          // Baud    CTL2  CTL1  OPT
BaudStruct SX2_32_Set[]= { { 9600,   0xD0, 0x03, 0x00 },
                           { 14400,  0x8B, 0x03, 0x00 },
                           { 19200,  0xD0, 0x02, 0x00 },
                           { 28800,  0x8B, 0x02, 0x00 },
                           { 38400,  0xD0, 0x01, 0x00 },
                           { 57600,  0x8B, 0x01, 0x00 },
                           { 115200, 0x8B, 0x00, 0x00 },
                           { 230400, 0x45, 0x00, 0x00 },
                           { 460800, 0x23, 0x00, 0x00 },
                           { 615300, 0x1A, 0x00, 0x00 },
                           { 806400, 0x14, 0x00, 0x00 },
                           { 1000000, 0x10, 0x00, 0x00 },
                           { 0, 0x0, 0x00, 0x00 },
                           { 0, 0x0, 0x00, 0x00 },
                           { 0, 0x0, 0x00, 0x00 }
                         };
                          // Baud    CTL2  CTL1  OPT
BaudStruct FX4_160_Set[]= { { 9600,  0xFF, 0x05, 0x00 },
                           { 14400,  0xAC, 0x05, 0x00 },
                           { 19200,  0x82, 0x05, 0x00 },
                           { 28800,  0x57, 0x05, 0x00 },
                           { 38400,  0x82, 0x04, 0x00 },
                           { 57600,  0xAD, 0x03, 0x00 },
                           { 115200, 0x57, 0x03, 0x00 },
                           { 230400, 0x2B, 0x03, 0x00 },
                           { 460800, 0x57, 0x01, 0x00 },
                           { 615300, 0x41, 0x01, 0x00 },
                           { 806400, 0x63, 0x00, 0x00 },
                           { 1000000, 0x50, 0x00, 0x00 },
                           { 1500000, 0x35, 0x00, 0x00 },
                           { 2000000, 0x28, 0x00, 0x00 },
                           { 3000000, 0x1A, 0x00, 0x00 }
                         };
                          // Baud    CTL2  CTL1  OPT
BaudStruct FX4_80_Set[]= { { 9600,   0x82, 0x05, 0x00 },
                           { 14400,  0x56, 0x04, 0x00 },
                           { 19200,  0x82, 0x04, 0x00 },
                           { 28800,  0x56, 0x04, 0x00 },
                           { 38400,  0x41, 0x04, 0x00 },
                           { 57600,  0x57, 0x03, 0x00 },
                           { 115200, 0x57, 0x02, 0x00 },
                           { 230400, 0x2B, 0x02, 0x00 },
                           { 460800, 0x2B, 0x01, 0x00 },
                           { 615300, 0x21, 0x01, 0x00 },
                           { 806400, 0x32, 0x00, 0x00 },
                           { 1000000, 0x28, 0x00, 0x00 },
                           { 1500000, 0x20, 0x00, 0x00 },
                           { 2000000, 0x10, 0x00, 0x00 },
                           { 3000000, 0x08, 0x00, 0x00 }
                         };
                          // Baud    CTL2  CTL1  OPT
BaudStruct FX4_64_Set[]= { { 9600,   0xD0, 0x04, 0x00 },
                           { 14400,  0x8B, 0x04, 0x00 },
                           { 19200,  0x68, 0x04, 0x00 },
                           { 28800,  0x8B, 0x03, 0x00 },
                           { 38400,  0x68, 0x03, 0x00 },
                           { 57600,  0x8B, 0x02, 0x00 },
                           { 115200, 0x45, 0x02, 0x00 },
                           { 230400, 0x23, 0x02, 0x00 },
                           { 460800, 0x34, 0x02, 0x00 },
                           { 615300, 0x23, 0x01, 0x00 },
                           { 806400, 0x14, 0x01, 0x00 },
                           { 1000000, 0x10, 0x01, 0x00 },
                           { 1500000, 0x1A, 0x00, 0x00 },
                           { 2000000, 0x0D, 0x00, 0x00 },
                           { 3000000, 0x06, 0x00, 0x00 }
                         };
                          // Baud    CTL2  CTL1  OPT
BaudStruct FX4_48_Set[]= { { 9600,   0x9C, 0x04, 0x00 },
                           { 14400,  0x68, 0x04, 0x00 },
                           { 19200,  0x9C, 0x03, 0x00 },
                           { 28800,  0x68, 0x03, 0x00 },
                           { 38400,  0x9C, 0x02, 0x00 },
                           { 57600,  0x68, 0x02, 0x00 },
                           { 115200, 0x68, 0x01, 0x00 },
                           { 230400, 0x34, 0x01, 0x00 },
                           { 460800, 0x34, 0x00, 0x00 },
                           { 615300, 0x27, 0x00, 0x00 },
                           { 806400, 0x1D, 0x00, 0x00 },
                           { 1000000, 0x18, 0x00, 0x00 },
                           { 1500000, 0x14, 0x00, 0x00 },
                           { 2000000, 0x0A, 0x00, 0x00 },
                           { 3000000, 0x05, 0x00, 0x00 }
                         };
BaudStruct *BaudRegSet;
#define MAX_BAUD_IDX           14

int ChkValidBaudrate(long lBaud)           // Prüfung auf unterstützte Baudrate
{
  if (ubDevice == DEVICE_V850E)            // V850E kennt nur 16 und 32 MHz SysClock
     BaudRegSet = (SysClock == 16) ? &SX2_16_Set[MAX_BAUD_IDX] : (SysClock == 32) ? &SX2_32_Set[MAX_BAUD_IDX] : NULL;
  else if (ubDevice == DEVICE_V850E2M)     // V850E2M kennt nur 80 und 64 MHz SysClock
     BaudRegSet = (SysClock == 160) ? &FX4_160_Set[MAX_BAUD_IDX] : (SysClock == 80) ? &FX4_80_Set[MAX_BAUD_IDX] : (SysClock == 64) ? &FX4_64_Set[MAX_BAUD_IDX] : (SysClock == 48) ? &FX4_48_Set[MAX_BAUD_IDX] : NULL;
  else BaudRegSet = &V850_Set[MAX_BAUD_IDX];

  //BaudRegSet = (ubDevice == DEVICE_V850E) ? ((SysClock == 16) ? &V850E_Set[MAX_BAUD_IDX] : &V850Ex_Set[MAX_BAUD_IDX]) : &V850_Set[MAX_BAUD_IDX];

  if (BaudRegSet == NULL)
  {  ProgressUpdate("Baudrate calculation is not yet supported for that device/SysClock");
     return -1;
  }

  int i;

  for (i = MAX_BAUD_IDX; i>=0; i--)
      if ((ulword)lBaud == BaudRegSet->baud) break;
      else BaudRegSet--;

  return i;
}



/****************************************************************************/

/*DLL .. */



/*************************************************************************@FA*
 * @Function          : Flash_vSetProgressBC, Flash_vSetDebugBC ()
 *----------------------------------------------------------------------------
 * @Description       : These Function can be used to enable or disable the
 *                      status output for Progress and Debug.
 *----------------------------------------------------------------------------
 * @Returnvalue       : none
 *----------------------------------------------------------------------------
 * @Parameters        : vect: pointer to backcall function of Application
 *----------------------------------------------------------------------------
 * @Functioncalls     : none
 *----------------------------------------------------------------------------
 * @Global_Vars_Used  : IntParms
 *----------------------------------------------------------------------------
 * @Copyright         : (c) 2001 Robert Bosch GmbH, Hildesheim
 * @Author            : CM-CR / ESD4  Peter Tuschik
 *----------------------------------------------------------------------------
 *
 * @History           : Initial Version
 *
 ************************************************************************@FE*/
DLLEXPORT void __stdcall Flash_vSetProgressBC(vect func)
{
  IntParms.pProgressInfo = func;            /* save ptr to backcall function*/
}
DLLEXPORT void __stdcall Flash_vSetDebugBC(vect func)
{
  IntParms.pComDebug = func;                /* save ptr to backcall function*/
}
DLLEXPORT void __stdcall Flash_vSetErrMsgBC(vect func)
{
  IntParms.pErrMsg = func;                  /* save ptr to backcall function*/
}
DLLEXPORT void __stdcall Flash_vSetMKeyBC(vect func)
{
  IntParms.pGetMKey = func;                 /* save ptr to backcall function*/
}
DLLEXPORT void __stdcall Flash_vSetCDMBXchgBC(vect func)
{
  IntParms.pCDMBXchg = func;                /* save ptr to backcall function*/
}
DLLEXPORT void __stdcall Flash_vSetKDSXchgBC(vect func)
{
  IntParms.pKDSXchg = func;                /* save ptr to backcall function*/
}

/*************************************************************************@FA*
 * @Function          : Flash_vSetFrameSize ()
 *----------------------------------------------------------------------------
 * @Description       : These Function can be used to modify the default frame
 *                      length of a message to device > V850E.
 *----------------------------------------------------------------------------
 * @Returnvalue       : none
 *----------------------------------------------------------------------------
 * @Parameters        : ulword: max length of a frame
 *----------------------------------------------------------------------------
 * @Functioncalls     : none
 *----------------------------------------------------------------------------
 * @Global_Vars_Used  : IntParms
 *----------------------------------------------------------------------------
 * @Copyright         : (c) 2013 Robert Bosch GmbH, Hildesheim
 * @Author            : CM-AI / PJ-CC11  Peter Tuschik
 *----------------------------------------------------------------------------
 *
 * @History           : Initial Version
 *
 ************************************************************************@FE*/
DLLEXPORT void __stdcall Flash_vSetFrameSize(ulword FrameSize)
{
  if (FrameSize <= 1024)                /* more that 0x400 is not supported */
     IntParms.ulwMaxFrameSize = FrameSize;   /* save to internal parameters */
}


/*************************************************************************@FA*
 * @Function          : Flash_vSetTestManager ()
 *----------------------------------------------------------------------------
 * @Description       : These Function can be used to modify the default Test-
 *                      Manager to be downloaded to device > V850E.
 *----------------------------------------------------------------------------
 * @Returnvalue       : none
 *----------------------------------------------------------------------------
 * @Parameters        : ulword: BlockNumber
 *----------------------------------------------------------------------------
 * @Functioncalls     : none
 *----------------------------------------------------------------------------
 * @Global_Vars_Used  : IntParms
 *----------------------------------------------------------------------------
 * @Copyright         : (c) 2013 Robert Bosch GmbH, Hildesheim
 * @Author            : CM-AI / PJ-CC11  Peter Tuschik
 *----------------------------------------------------------------------------
 *
 * @History           : Initial Version
 *
 ************************************************************************@FE*/
DLLEXPORT void __stdcall Flash_vSetTestManager(uword BlockNumber)
{
  if ((BlockNumber & 0xFF00) == 0x8400)        /* Check for valid Blocktype */
     IntParms.uwTestManager = BlockNumber;   /* save to internal parameters */
}


/*************************************************************************@FA*
 * @Function          : Flash_ulwInit ()
 *----------------------------------------------------------------------------
 * @Description       : This routine opens the COM Port given by the parameter
 *                      pubCom and sets the parameter, passed by the structure
 *                      "pstFlashParms".
 *                      The memory of the structure will be allocated and must
 *                      be released by the Flash_ShutDown().
 *----------------------------------------------------------------------------
 * @Returnvalue       : none
 *----------------------------------------------------------------------------
 * @Parameters        : ubyte* pubCom: string to COM-Port
 *                      Flash_TYParms: ptr to parameter values
 *----------------------------------------------------------------------------
 * @Functioncalls     : strchr(), memcpy(), PortInit(), PortOpen()
 *----------------------------------------------------------------------------
 * @Global_Vars_Used  : pFlashPort, FlashParms
 *----------------------------------------------------------------------------
 * @Copyright         : (c) 2001 Robert Bosch GmbH, Hildesheim
 * @Author            : CM-CR / ESD4  Peter Tuschik
 *----------------------------------------------------------------------------
 *
 * @History           : Initial Version
 *
 ************************************************************************@FE*/
DLLEXPORT ulword __stdcall Flash_ulwInit(ubyte  *pubCom, Flash_TYParms *pstFlashParms)
{ char *cPtr;
  blFlashActive = false;                         /* default, dll = inactive */
  blAbort = false;                           /* initdefault, dll = inactive */
  aubDevInfo[0] = 0;                                  /* enable Device-Info */


                        /* get Log-enable first to be able to write LogFile */
  FlashParms.ubLogEnable = pstFlashParms->ubLogEnable;

  if ((NULL == pstFlashParms) || (NULL == pubCom))
  {  LogFileWrite("ERROR: Wrong parameter");
     return FLASH_PARM_ERROR;
  }

  char help[1024];                    /* copy to help-Buffer, not to modify */
  strcpy(help,pstFlashParms->LogPath);            /* the original file name */

  if (strstr(toUpper(help),".TXT") == 0)
     sprintf(LogFileName,"%sLogFile.txt",pstFlashParms->LogPath);
  else sprintf(LogFileName,"%s",pstFlashParms->LogPath);

  Flash_ulwShutDown();                      /* if already initialised       */

  LogFileWrite("Entered Flash_ulwInit()");

  IntParms.pIntroLoader = NULL;             /* initial values for data ptr  */
  IntParms.pFlashWriter = NULL;
  IntParms.ulwMaxFrameSize = 512;
  IntParms.uwTestManager = 0x8400;
  DFInfoBuffer = NULL;
  BlockList = NULL;
  ApplMemory = NULL;

  cPtr = (char*)strchr((const char*)pubCom,',');
  if (cPtr) *cPtr = 0;                          /* extract only Port Number */

  if (pubCom == (unsigned char*)"")
  {  LogFileWrite("ERROR: Missing parameter for port configuration");
     return (FLASH_PARM_ERROR);
  }

  /* Initialisation of Com-Timers and GPIO-Pins */
  if (!PortInit(pstFlashParms->GPIO_Reset, pstFlashParms->GPIO_VPP))
  {  LogFileWrite("ERROR: Windows Performance Timer not available or GPIO-Error");
     return FLASH_PERFORM_TMR_NAV;
  }

  DevParms.ulwBlindBuf = 9;         /* Initial Value to avoid error message */
  Flash_ulwSetParms(pstFlashParms);                /* Init Flash parameters */

  pFlashPort = PortNew((char*)pubCom, FlashParms.uwTelegramTimeOut,
                       kIntroBaud, k8Bit, kNone, k1Stop);
  pConnectPort = PortNew((char*)pubCom, FlashParms.uwTelegramTimeOut,
                         kFlashBaud, k8Bit, kEven, k1Stop);

  #if 0
  // Test for Raja, to prevent from Reset at FlashGUI Start/Stop
  if (!PortOpen(pFlashPort))                               /* Open Com-Port */
  {  LogFileWrite("ERROR: Cannot open com Port");
     return FLASH_COM_SET_FAIL;
  }
  PortSwitch(pFlashPort,P_OFF,false);        /* Default: Interface disablen */
  #endif

  LogFileWrite("Exit Flash_ulwInit() with success");
  return FLASH_SUCCESS;

} /* end of Flash_ulwnit() */


/*************************************************************************@FA*
 * @Function          : Flash_ulwShutDown ()
 *----------------------------------------------------------------------------
 * @Description       : This function has to be called before closing the
 *                      application to release all use Resource
 *
 *----------------------------------------------------------------------------
 * @Returnvalue       : ulword ( success or error status )
 *----------------------------------------------------------------------------
 * @Parameters        : none
 *----------------------------------------------------------------------------
 * @Functioncalls     : PortClose(), PortDelete(), delete()
 *----------------------------------------------------------------------------
 * @Global_Vars_Used  :
 *----------------------------------------------------------------------------
 * @Copyright         : (c) 2001 Robert Bosch GmbH, Hildesheim
 * @Author            : CM-CR / ESD4  Peter Tuschik
 *----------------------------------------------------------------------------
 *
 * @History           : Initial Version
 ************************************************************************@FE*/
DLLEXPORT ulword __stdcall Flash_ulwShutDown (void)
{
  if (blFlashActive) return (FLASH_ACCESS_DENIED);/* ignore if dll = active */
  LogFileWrite("Entered Flash_ulwShutDown()");

  IntParms.pProgressInfo = NULL;
  IntParms.pComDebug = NULL;
  IntParms.pErrMsg = NULL;
  IntParms.pGetMKey = NULL;
  Flash_ulwFreeMemory(&ApplMemory);             /* evtl. Speicher freigeben */

  if (pFlashPort != NULL)                          /* free allocated memory */
  {  if (pFlashPort->hID)
        PortClose (pFlashPort);                           /* close COM-Port */
     PortDelete(pFlashPort);
  }
  pFlashPort = NULL;

  if (pConnectPort != NULL)
     PortDelete(pConnectPort);
  pConnectPort = NULL;

  if (IntParms.pIntroLoader != NULL)
     delete [] IntParms.pIntroLoader;
  IntParms.pIntroLoader = NULL;

  if (IntParms.pFlashWriter != NULL)
     delete [] IntParms.pFlashWriter;
  IntParms.pFlashWriter = NULL;

  if (KDS_Buf != NULL)
     delete [] KDS_Buf;
  KDS_Buf = NULL;

  if (IntParms.f_DFile) fclose(IntParms.f_DFile);
  IntParms.f_DFile = NULL;

  if (DFInfoBuffer != NULL)
     delete [] DFInfoBuffer;
  DFInfoBuffer = NULL;

  PortExit();

  LogFileWrite("Exit Flash_ulwShutDown() with success");
  return (FLASH_SUCCESS);

} /* end of Flash_ulwShutDown() */

/*************************************************************************@FA*
 * @Function          : Flash_ulwDoConnect ()
 *----------------------------------------------------------------------------
 * @Description       : This function handles the connection set up to the
 *                      NEC V850 SF1 / SC3  with respect to the document
 *                      "Flash Concept Virgin". and enters MAUS Level 0 for
 *                      Flash Writer on NEC V850. The baudrate is passed by
 *                      the parameter pubCom and will patch the Intro-Loader
 *                      accordingly
 *
 *----------------------------------------------------------------------------
 * @Returnvalue       : ulword ( success or error status )
 *----------------------------------------------------------------------------
 * @Parameters        : ubyte* pubCom         : string of COM-Port
 *                      ubyte* pubDFF_Filename: string of filename
 *                      ulword ulwMode        : starting point where con-
 *                                              nection set up is entered
 *                      ubyte ubToggleCnt     : # of counts to toggle the Vpp
 *                                            : Line to get com-access
 *----------------------------------------------------------------------------
 * @Functioncalls     : Flash_uiInitBootstrap(), PortSend(), TimeWait(),
 *                      PortReadOK()
 *----------------------------------------------------------------------------
 * @Global_Vars_Used  :
 *----------------------------------------------------------------------------
 * @Copyright         : (c) 2001 Robert Bosch GmbH, Hildesheim
 * @Author            : CM-CR / ESD4  Peter Tuschik
 *----------------------------------------------------------------------------
 *
 * @History           : Initial Version
 *
 *           ver  2.2 - FlashPro split into FlashDLL and Flashexe
 *                    - EEPDownload option - "Startup & Delivery"
 *                    - Patching of BaudRate to Bootstrap file
 ************************************************************************@FE*/
DLLEXPORT ulword __stdcall Flash_ulwDoConnect (ubyte  *pubCom,
                           ubyte *pubDF_Filename, ulword ulwMode,
                           ubyte ubToggleCnt )
{ const ulword caulwSizeOffset[] = { 0x3ff9000, 0x3ff7000, 0x3ff5000, 0x3ff3000, 0x3ff0000, 0x3ff0000, 0x3ff0000 };
  ulword ulwiMode = ulwMode & 0xff;               /* strip off unused flags */
  ulword err;
  blAbort = false;
  blDebug = (ulwiMode == 50);                   /* Roli's Debugmode aktiv ? */
  bool blChipErase = ((ulwMode & FLASH_MODE_CHIP_ERASE) == 0);
  bool blClearDataFlash = ((ulwMode & FLASH_MODE_DATAFLASH_ERASE) == 0) && !blChipErase;
  ulword ulwLoadAddr = INTROL_START_V850;
  uword FlType = 0x8200;                        /* Default: FlashWriter     */
  if ((ulwMode & FLASH_MODE_TESTMODE) == 0)     /* if Testmode, then set to */
     FlType = IntParms.uwTestManager;           /* Testmanager              */

  LogFileWrite("Entered Flash_ulwDoConnect()");
  LogFileWrite("Parameters are: %s, %s, 0x%x, %d",pubCom,pubDF_Filename,ulwMode,ubToggleCnt);
  memset(OptLog,0,sizeof(OptLog));       /* Delete data for Optionbyte info */

  if (ulwiMode == 0)                          /* default programming method */
     ulwiMode = IntParms.ulwMode;                    /* for detected device */

  ComSettings((char*)pubCom,&pConnectPort);      /* get Com-Set from string */
                                                  /* exit if error detected */
                                       /* Mode for ignore Echo on MAUS comm */
  EchoCom = (ulwiMode == 30) ? kEcho : 0;

  if ((ulwiMode < 4) || ((ulwiMode % 10) == 0) || (ulwiMode == 14))
  {  if (blFlashActive)
     {  LogFileWrite("ERROR: Flash850.dll is still active - Command ignored");
        blAbort = true;
        return (FLASH_ACCESS_DENIED);             /* ignore if dll = active */
     }
     if (IntParms.f_DFile) fclose(IntParms.f_DFile);
     IntParms.f_DFile = fopen((const char*)pubDF_Filename,"rb");
     if (IntParms.f_DFile == NULL)
     {  LogFileWrite("ERROR: DF-File cannot be opened");
        return (FLASH_DF_FILE_IO_ERROR);
     }

     if (pFlashPort)
     {  if (pFlashPort->hID)
           PortClose(pFlashPort);
        PortDelete(pFlashPort);
     }                                      /* and set COM-Port, as defined */
     pFlashPort = PortNew(pConnectPort->pcName, FlashParms.uwTelegramTimeOut,
                          pConnectPort->ulBaudRate,
                          pConnectPort->ucBit,
                          pConnectPort->ucParity,
                          pConnectPort->ucStop);

     if ((pConnectPort->ucBit != 8) || (pConnectPort->ucStop != 1))
        LogFileWrite("ERROR: Es gibt nur 8 Databits und 1 StopBit. Wer braucht denn da was anderes ??");

     if (!PortOpen(pFlashPort))                           /* exit, if error */
     {  LogFileWrite("ERROR: Cannot set COM-Port parameters (1)");
        return FLASH_BAUDRATE_SET_FAILED;
     }

     if ((err = DFInfo_ulwInit(IntParms.f_DFile,&DFInfo)) != FLASH_SUCCESS)
        return err;                               /* exit if error detected */
                                                   /* read IntroLoader data */
     if (!DFInfo_blReadData(IntParms.f_DFile,&DFInfo,&IntParms.pIntroLoader,0x8100))
     {  LogFileWrite("ERROR: Introloader cannot be read from DNL-File");
        return (FLASH_DF_FILE_CONTENT_ERROR);
     }

     if ( ubToggleCnt != 0xff)                 /* falls gültiger Wert, dann */
        DevParms.ubToggleCounts = ubToggleCnt;        /* init mit Parameter */
                                     /* if DF-Info Data enabled, then check */
     if ((ulwMode & FLASH_MODE_DFINFO) == 0)
     {  err = DFInfo_blReadDevParms(IntParms.f_DFile,&DFInfo,&DevParms);
        if (err == (ulword)FLASH_NO_SUCCESS)
           LogFileWrite("No device parameter found in DNL-File. DLL uses default values !");
        else if (err != FLASH_SUCCESS)
             {  LogFileWrite("Error during Reading of the device parameter found in DNL-File !");
                return err;
             }
     }

     /* Fix or variable load address for intro loader */

     if ((ulwMode & FLASH_MODE_FIX_LOAD_ADDR) && SX3)
     {  ulword idx = 0;
        for (ulword size = 0x3ffff; size < DevParms.ulwRomSize; size += 0x20000)
        {
            idx++;
        }
        if (idx < 7) ulwLoadAddr = caulwSizeOffset[idx];
     }

     if (ulwiMode < 4)                      /* Achtung: Nur V850 SF1 und SC3 */
        if (DFInfo_ulwGetLen(&DFInfo,0x8100) > 256)
        {  LogFileWrite("ERROR: Size for Introloader is too big ( > 256)");
           return (FLASH_INTROLOADER_SIZE_TOO_BIG);
        }

     if (!DFInfo_blReadData(IntParms.f_DFile,&DFInfo,&IntParms.pFlashWriter,FlType))
     {  if (FlType == 0x8200) LogFileWrite("ERROR: Flashwriter cannot be read from DNL-File");
        else LogFileWrite("ERROR: TestManager cannot be read from DNL-File");
        return (FLASH_DF_FILE_CONTENT_ERROR);
     }
  }
  else
  {
    pConnectPort->ulBaudRate = kConnectBaud;     // This is fixed for Maus cmds
    pConnectPort->ucParity = kEven;
    pConnectPort->ucBit = 8;
    pConnectPort->ucStop = 1;
    if ( pFlashPort && pConnectPort &&              // COM-Parameter geändert ?
         ((strcmp(pConnectPort->pcName,pFlashPort->pcName) != 0) ||
          (pConnectPort->ulBaudRate != pFlashPort->ulBaudRate) ||
          (pConnectPort->ucBit != pFlashPort->ucBit) ||
          (pConnectPort->ucParity != pFlashPort->ucParity) ||
          (pConnectPort->ucStop != pFlashPort->ucStop)
         )
       )
    {
        if (pFlashPort->hID)
           PortClose(pFlashPort);
        PortDelete(pFlashPort);
                                                // and set COM-Port, as defined
        pFlashPort = PortNew(pConnectPort->pcName, FlashParms.uwTelegramTimeOut,
                             pConnectPort->ulBaudRate,
                             pConnectPort->ucBit,
                             pConnectPort->ucParity,
                             pConnectPort->ucStop);

        if (!PortOpen(pFlashPort))                            // exit, if error
        {  LogFileWrite("ERROR: Cannot set COM-Port parameters (2)");
           return FLASH_BAUDRATE_SET_FAILED;
        }
    }
  }

  blFlashActive = true;                                 /* now dll = active */
  blErased = false;

  ubDevice = DEVICE_V850;                                 /* default = V850 */
  ASCI = (ulwMode & FLASH_MODE_NON_CROSS_SCI) != 0L;

  PortSwitch(pFlashPort,P_NORM,ASCI);  /* Default: RX-/ TX nicht invertiert */
                                                /* sonst ASCI-Konfiguration */
  switch (ulwiMode)
  {                                            /* Einsprung: Flash für V850 */
                                               /* ========================= */
    case 0:                           /* start at beginning of the sequence */
    {
            ulword i;
            pFlashPort->ulBaudRate = kIntroBaud;     /* init default values */
            pFlashPort->ucParity = kNone;
            if (!PortSetup(pFlashPort))
            {  LogFileWrite("ERROR: Baudrate for Intro-loader cannot be set");
               err = FLASH_BAUDRATE_SET_FAILED;
               goto ErrExit0;                                 /* switch off */
            }
                                                /* Now enter Bootstrap mode */
            pFlashPort->ulTimeOut = FlashParms.uwFLWTimeOut;

            for (i = 0; i < DevParms.ulwPM_F9; i++)
            {   if (Flash_blInitBootstrap())
                {  ProgressUpdate("Init Bootstrap, Attempt: %d",i+1);

                                      /* Send Interface Configuration Bytes */
                   PortSend(pFlashPort,(unsigned char*)"\x00",1);
                   PortSend(pFlashPort,(unsigned char*)"\x00",1);
                   TimeWait (1000);                        /* 1000 = 1 mSec */
                                                      /* Send Reset Command */

                   PortSend(pFlashPort,(unsigned char*)"\x00",1);
                   if (PortReadOK (pFlashPort, 0x3c, (char*)"\0") == true)
                       break;
                }
                if (blAbort) break;                        /* evtl. Abbruch */
            }
            if ((i >= DevParms.ulwPM_F9) || blAbort)
            {  LogFileWrite("ERROR: No response on entering the Bootstrap-mode");
               err = FLASH_CONNECT_FAILED;
               goto ErrExit;                /* set VPP and Reset to default */
                                                       /* and exit with err */
            }
    }
    case 1:                         /* start beginning at Request Bootstrap */
            aubDevInfo[0] = 0;                        /* enable Device-Info */
            if (FlashParms.ubReadSignature)
            {  LogFileWrite("Start reading the Signature from Device");
               if (Flash_ulwReadDeviceInfo(&aubDevInfo[1]) != FLASH_SUCCESS)
               {  ProgressUpdate("ERROR: Read Signature failed");
                  err = FLASH_DEVINFO_FAILED;
                  goto ErrExit;             /* set VPP and Reset to default */
                                                       /* and exit with err */
               }
            }

            aubDevInfo[0] = 0xff;                    /* disable Device-Info */
            ProgressUpdate("Request Bootstrap");
            PortSend(pFlashPort,(unsigned char*) "\xd0", 1);
            if (PortReadOK(pFlashPort, 0x3c, (char*)"\0") == false)
            {  LogFileWrite("ERROR: No response for the Bootstrap-request");
               err = FLASH_BOOTSTRAP_FAILED;
               goto ErrExit;                /* set VPP and Reset to default */
                                                       /* and exit with err */
            }
                                                    /* transmit IntroLoader */
            ProgressUpdate("Download Introloader");
                                          /* Patch Baudrate for DEVICE_V850 */
            if (!Flash_vIntroLoaderPatchBaud(pConnectPort))
            {  LogFileWrite("ERROR: Baudrate not supported to patch Introloader");
               err = FLASH_BAUDRATE_SET_FAILED;
               goto ErrExit;                /* set VPP and Reset to default */
                                                       /* and exit with err */
            }

            /* ------------------------------------------------------------ */
            /* es ist sicher egal, ob man im Falle eines kleineren Introloaders
               als 256 Byte einfach den Rest aus irgend einem Speichermüll
               überträgt, aber der CodeGuard meckert dann, weil es ihm nicht
               gefällt, wenn man anderer Leute Speicher mißbraucht.
               Deshalb gibt es hier ne kleine Codeverschwendung !!!         */

            char cBuffer[256];   /* IntroLoader braucht leider 256 Byte fix */
            memset(cBuffer, 0xff, 256);
            memcpy(cBuffer, IntParms.pIntroLoader, DFInfo_ulwGetLen(&DFInfo,0x8100) );
            PortSend(pFlashPort, (unsigned char*)cBuffer, 256);
            /* so gings auch:
            PortSend(pFlashPort,(unsigned char*)IntParms.pIntroLoader,256); */
            /* ------------------------------------------------------------ */

    waitILxmFW:                                      /* Einsprung für V850E */
                          /* Wait for 0x3C after Introloader starts working */
            if ((err = ReadPortSendResult()) != FLASH_SUCCESS)
               goto ErrExit;

    case 2:                           /* wait for Flw_Writer Download ready */
    {                                             /* switch to new baudrate */
            if ((pFlashPort->ulBaudRate != pConnectPort->ulBaudRate) ||
                (pFlashPort->ucParity != pConnectPort->ucParity))
            {  LogFileWrite("Switch to new Baudrate after Introloader");
               pFlashPort->ulBaudRate = pConnectPort->ulBaudRate;
               pFlashPort->ucParity = pConnectPort->ucParity;
               if (!PortSetup(pFlashPort))
               {  LogFileWrite("ERROR: switch to Baudrate for Flashwriter was not successful");
                  err = FLASH_BAUDRATE_SET_FAILED;         /* exit with err */
                  goto ErrExit;             /* set VPP and Reset to default */
               }
            }

            /* Warten auf 0x3c in neuer Baudrate gibt es nicht mehr ! Ultragefährlich !!*/
                                   /* Now xmit len and data for FlashWriter */
                                                      /* inclusive CheckSum */

            if (FlType == 0x8200) ProgressUpdate("Download FlashWriter");
            else ProgressUpdate("Download Testmanager Block 0x%04x",FlType);

            unsigned int flwlen = DFInfo_ulwGetLen(&DFInfo,FlType) + 1;
            unsigned char ucByte = (unsigned char)((flwlen >> 8) & 0xff);

            PortSend(pFlashPort,&ucByte, 1);
            TimeWait (50);           /* Roland muß hier irgendwas ausrechen */
            ucByte = (unsigned char)(flwlen & 0xff);
            PortSend(pFlashPort,&ucByte, 1);
            TimeWait (50);                          /* und wieder berechnen */

            /* The read of the following 0x3C after sending the FLW-length  */
            /* was ignored for older version but should still be valid.     */
            /* Let's think to jump if there will come up errors with other  */
            /* projects, but it normally must work. (!! Tut's aber nicht)   */
            if (ubDevice == DEVICE_V850E2M) {
            if (PortReadOK(pFlashPort, 0x3c, (char*)"\0",true) == false)
            {  pFlashPort->ulTimeOut = FlashParms.uwTelegramTimeOut;
               if (FlType == 0x8200)
               {  LogFileWrite("ERROR: No response from the Flashwriter after sending length info");
                  err = FLASH_FLASHWRITER_FAILED;          /* exit with err */
               }
               else
               {  LogFileWrite("ERROR: No response from the TestManager after sending length info");
                  err = FLASH_TESTMANAGER_FAILED;
               }
               goto ErrExit;                /* set VPP and Reset to default */
            }}

            PortSend(pFlashPort,IntParms.pFlashWriter, flwlen);

            if (ubDevice == DEVICE_V850E2M)  /* FX4 hat 1 wire com-line und */
            {  unsigned long ulLength = flwlen;      /* muß erst Gesendetes */
               unsigned char buf[64*1024];                   /* zurücklesen */
               if (flwlen < (64*1024))
                  PortReceive(pFlashPort, buf, &ulLength, kDebugOut | kEmpty);
               else LogFileWrite("ERROR: Size of Block 0x%04x is too big (> 64 kb)", FlType);
            }


            if (PortReadOK(pFlashPort, 0x3c, (char*)"\0",true) == false)
            {  pFlashPort->ulTimeOut = FlashParms.uwTelegramTimeOut;
               if (FlType == 0x8200)
               {  LogFileWrite("ERROR: No response from the Flashwriter");
                  err = FLASH_FLASHWRITER_FAILED;          /* exit with err */
               }
               else
               {  LogFileWrite("ERROR: No response from the TestManager");
                  err = FLASH_TESTMANAGER_FAILED;
               }
               goto ErrExit;                /* set VPP and Reset to default */
            }
            if (FlType == IntParms.uwTestManager)
            {  LogFileWrite("Exit Flash_ulwDoConnect() with success and Testmode active (VPP is now off)");
               blConnected = true;
               blFlashActive = false;                     /* dll = inactive */
               PortSetRTS(pFlashPort, !FLASH_VPP);        /* switch off VPP */
               return err;
            }

            TimeWait (FlashParms.uwPreDnlDelay);/* wait before MAUS request */
    }
    case 3:                  /* start at request MAUS for Flash Programming */
    {       ubyte MausLevel = FLASH_MAUS_LVL_0;

            #ifdef DEBUG_STOP
            IntParms.pErrMsg("Warte vor Request MausLevel0",true);
            #endif

            err = ReqMausLevel(ulwMode & (FLASH_DROP_OFF<<8),0,MausLevel);
            if (err == (ulword)FLASH_SUCCESS)
            {  blConnected = true;
               blFlashActive = false;                     /* dll = inactive */

               if ((ubDevice == DEVICE_V850E2M) && (FX4Virgin != 7) && (FX4Default != 7))
               {  // Connected but error in case of invalid options
                  err = FLASH_INVALID_OPTIONSETTINGS;
                  LogFileWrite("Exit Flash_ulwDoConnect() with ivalid Option settings");
               }
               else LogFileWrite("Exit Flash_ulwDoConnect() with success");
               return err;
            }
    }
ErrExit:
#ifndef NO_RESET_ON_ERROR
                           /* This can be removed in case of debug purposis */
            PortSetRTS(pFlashPort, !FLASH_VPP);           /* switch off VPP */
            TimeWait(DevParms.ulwPM_F11);                    /* wait 1 mSec */
            PortSetDTR(pFlashPort, !FLASH_RESET);              /* Reset off */
ErrExit0:
            PortSwitch(pFlashPort,P_OFF,false);           /* Off:  disablen */
#else
ErrExit0:
#endif
            blFlashActive = false;                        /* dll = inactive */
            LogFileWrite("Exit Flash_ulwDoConnect() with no success");
            return err;
    case 4:
    {                                                       /* Maus Reizung */
            LogFileWrite("Switch to Maus-Default Baudrate");
            pFlashPort->ulBaudRate = kConnectBaud;
            pFlashPort->ucParity = kEven;
            if (!PortSetup(pFlashPort))
            {  LogFileWrite("ERROR: switch to Maus-Default Baudrate was not successful");
               err = FLASH_BAUDRATE_SET_FAILED;
               goto ErrExit;                /* set VPP and Reset to default */
                                                       /* and exit with err */
            }

                                                /* Now enter Bootstrap mode */
            PortSetRTS(pFlashPort, !FLASH_VPP);                  /* Vpp off */
            PortSetDTR(pFlashPort, FLASH_RESET);          /* reset aktivate */
            TimeWait(kT1);                                   /* wait 1 mSec */
            PortSetDTR(pFlashPort, !FLASH_RESET);       /* deaktivate reset */
            pFlashPort->ulTimeOut = FlashParms.uwTelegramTimeOut;
            TimeWait(kT1);                                   /* wait 1 mSec */
            TimeWait(kT1);                                   /* wait 1 mSec */

            blFlashActive = false;                        /* dll = inactive */
            int retry = 3;
            LogFileWrite("Try to Connect Maus-Mode");
            do
            {
              MausSend(pFlashPort, 0x52, NULL, 0);
              ucBuffer[0] = 0;
              unsigned char ucLength = 255;

              if (MausReceive(pFlashPort, 0x05, ucBuffer, &ucLength, 5, kDebug) == true)
              {  LogFileWrite("Connection Mode: %x",ucBuffer[4]);
                 LogFileWrite("Exit Flash_ulwDoConnect() with success");
                 return (FLASH_SUCCESS);
              }
            }
            while (retry--);
            err = Flash_ulwMausResult(0,0,0);
            goto ErrExit;                   /* set VPP and Reset to default */
                                                       /* and exit with err */
    }
    case 5:                                      /* Maus Level 0 einstellen */
    {       blFlashActive = false;                        /* dll = inactive */
            ubyte MausLevel = FLASH_MAUS_LVL_0;
            ulword RetVal = ReqMausLevel(0,0,MausLevel);
            LogFileWrite("Exit Flash_ulwDoConnect() with Connection Mode: %x",MausLevel);
            return (RetVal);
    }
    case 6:                            /* Maus Level 0 Protected einstellen */
    {       blFlashActive = false;                        /* dll = inactive */
            ubyte MausLevel = FLASH_MAUS_LVL_0P;
            ulword RetVal = ReqMausLevel(0,0,MausLevel);
            LogFileWrite("Exit Flash_ulwDoConnect() with Connection Mode: %x",MausLevel);
            return (RetVal);
    }
    case 7:
    {                                                // Maus Level 1 einstellen
            blFlashActive = false;                        /* dll = inactive */
            ubyte MausLevel = FLASH_MAUS_LVL_1;
            ulword RetVal = ReqMausLevel(0,0,MausLevel);
            LogFileWrite("Exit Flash_ulwDoConnect() with Connection Mode: %x",MausLevel);
            return (RetVal);
    }

    case 9:
    {                                                // Open Maus command
            blFlashActive = false;                        /* dll = inactive */
            ulword RetVal = (MausSend(pFlashPort, 0x19, (unsigned char*)"\xAB\x31\x49\x17\x8\x27\x5\x77", 8) == true) ? FLASH_SUCCESS : FLASH_NO_SUCCESS;
            LogFileWrite("Exit Flash_ulwDoConnect() after OpenMaus: %x",RetVal);
            return (RetVal);
    }
                                             /* Einsprung: Flash für TUAREG */
    case 10:                                 /* =========================== */
    {
            ulword j;
            ubDevice = DEVICE_TUAREG;               /* z. Zt reine Formsache */
            pFlashPort->ulBaudRate = kTuaregIntroBaud;/* init default values */
            pFlashPort->ucParity = kNone;
            if (!PortSetup(pFlashPort))
            {  LogFileWrite("ERROR: Baudrate for Intro-loader cannot be set");
               err = FLASH_BAUDRATE_SET_FAILED;
               goto ErrExit0;                                 /* switch off */
            }
                                                /* Now enter Bootstrap mode */
            pFlashPort->ulTimeOut = FlashParms.uwFLWTimeOut;

            for (j = 0; j < DevParms.ulwPM_F9; j++)
            {
                PortSetDTR(pFlashPort, FLASH_RESET);      /* reset aktivate */
                PortSetRTS(pFlashPort, !FLASH_VPP);              /* Vpp off */
                TimeWait(kT1);                               /* wait 1 mSec */
                PortSetDTR(pFlashPort, !FLASH_RESET);   /* deaktivate reset */
                ProgressUpdate("Init Boot Loader Request, Attempt: %d",j+1);

                                                       /* Send Boot-Request */
                PortSend(pFlashPort,(unsigned char*)"\xE5\x00",2);
                if (PortReadOK (pFlashPort, 0x06, (char*)"\0") == true)
                   if (PortSend(pFlashPort,(unsigned char*)"\x06",1) != 0)
                      break;                     /* raus aus for, wenn okay */
                if (blAbort) break;                        /* evtl. Abbruch */
            }

            if ((j >= DevParms.ulwPM_F9) || blAbort)
            {  LogFileWrite("ERROR: No response on starting the Boot Loader");
               err = FLASH_CONNECT_FAILED;
               goto ErrExit;                /* set VPP and Reset to default */
                                                       /* and exit with err */
            }
    }
    case 11:
    {                                               /* transmit IntroLoader */
            ProgressUpdate("Download IntroLoader");
            ProgressUpdate("Download: 0%%");

            ulword CHK, CS = 0;
            ulword ulwLen = DFInfo_ulwGetLen(&DFInfo,0x8100);
            unsigned char *cBuffer;
            if ((cBuffer = new unsigned char [ulwLen+2]) == NULL)
            {  LogFileWrite("ERROR: No systemspace to alloc the  DFInfo-Contentbuffer");
               err = FLASH_SYSTEM_ERROR;
               goto ErrExit;                /* set VPP and Reset to default */
                                                       /* and exit with err */
            }
            unsigned char *Ptr = cBuffer;
            *cBuffer     = (char)(ulwLen & 0xff);
            *(cBuffer+1) = (char)((ulwLen >> 8) & 0xff);
            memcpy (cBuffer+2,IntParms.pIntroLoader,ulwLen);
            ulword *p = (ulword*)IntParms.pIntroLoader;
            if (ulwLen % sizeof(ulword) == 0 )
            {  for (int l = ulwLen/sizeof(ulword); l > 0; l-- )
                   CS += *p++;
            } else CS = 0xffffffff;

            int prozent = 0;

            for (ulword i=0; i < ulwLen + 2; i++)
            {   if (PortSend(pFlashPort,Ptr,1) == 0)
                   LogFileWrite("Error during transmit");
                unsigned char proz = (unsigned char)(100-((ulwLen+2-i)*100/ulwLen));
                if (prozent != proz)
                   ProgressRefresh("Download:  %d%%",(prozent = proz));
                if (PortReadOK(pFlashPort, *Ptr, (char*)"\0") == false)
                {  LogFileWrite("ERROR: No response after transmit the Intro-loader");
                   delete []cBuffer;
                   err = FLASH_INTROLOADER_FAILED;
                   goto ErrExit;            /* set VPP and Reset to default */
                                                       /* and exit with err */
                }
                Ptr++;
            }
            unsigned long ulLength = 4;
            memset(cBuffer,0,10);
            int result = PortReceive(pFlashPort, cBuffer, &ulLength, kDebug);

            CHK = ((ulword)cBuffer[3]<<24) + ((ulword)cBuffer[2]<<16) + ((ulword)cBuffer[1]<<8) + (ulword)cBuffer[0];

            if ((result != kOK) || (CHK != CS))
            {  LogFileWrite("ERROR: Error in CS after transmit the Intro-loader");
               delete []cBuffer;
               err = FLASH_INTROLOADER_CS_ERROR;
               goto ErrExit;                /* set VPP and Reset to default */
                                                       /* and exit with err */
            }

            PortSend(pFlashPort,(unsigned char*)"\x06",1);
            delete []cBuffer;

    }
    case 12:
    {                                                /* Request Maus Level 0 */
            ubyte MausLevel = FLASH_MAUS_LVL_0;      /* No Drop check but Maus default */
            err = ReqMausLevel(0,1,MausLevel);
            if (err != FLASH_SUCCESS)
               goto ErrExit;                /* set VPP and Reset to default */
                                                       /* and exit with err */

                                                  /* switch to new baudrate */

            LogFileWrite("Try to switch to Baudrate: %s",pubCom);
            blFlashActive = false;      /* sonst geht die Umschaltung nicht */
            err = Flash_ulwMausChangeParms(pubCom);

            if (err != FLASH_SUCCESS)
            {  LogFileWrite("ERROR: Error during switch to new Baudrate by Maus");
               goto ErrExit;                /* set VPP and Reset to default */
            }                                          /* and exit with err */
            blFlashActive = true;
    }
    case 13:
    {                            /* transmit TUAREG Testmanager and execute */
                                     /* get Data from Blocktyp: Flashwriter */
            ulword Len = DFInfo_ulwGetLen(&DFInfo,0x8200);
            ulword Dest = DFInfo_ulwGetStart(&DFInfo,0x8200);

            ProgressUpdate("Download Testmanager");
            err = Flash_ulwWrite(0x10,IntParms.pFlashWriter,Dest,Len);

            if (err == (ulword)FLASH_SUCCESS)                  /* check CRC */
            {  ulword CRC;
               ProgressUpdate("Start reading CRC from Testmanager");
               if ((err = Flash_uiGetCRC(0x10,Dest,Len,(unsigned int*)&CRC)) == FLASH_SUCCESS)
                  if (CRC != DFInfo_ulwGetCRC(&DFInfo,0x8200))
                     err = FLASH_DF_FILE_CRC_ERROR;
            }
            if (err != FLASH_SUCCESS)
            {  LogFileWrite("ERROR: Error during download of Testmanager");
               goto ErrExit;                /* set VPP and Reset to default */
            }                                          /* and exit with err */

                                                     /* Exceute Testmanager */
            ProgressUpdate("Execute the Testmanager");
            sprintf((char*)ucBuffer,"\x10\x01");
            CopyU32ToBuf(&ucBuffer[2],Dest);

            if (ChkMausSend(pFlashPort, 0x85,ucBuffer, 6) == true)
            {  unsigned char ucLength = 255;

               ucBuffer[0] = 0;
               if (MausReceive (pFlashPort, 0x84, ucBuffer, &ucLength, 3, kGuard | kDebug) == false)
                  err = Flash_ulwMausResult(0,0,0);
            } else err = FLASH_NO_SUCCESS;      /* Error bei Senden des cmd */

            if (err != FLASH_SUCCESS)
            {  LogFileWrite("ERROR: Timeout on execute the Testmanager");
               goto ErrExit;                /* set VPP and Reset to default */
            }                                          /* and exit with err */
    }
    case 14:
    {                                                /* Request Maus Level 0 */
            ubyte MausLevel = FLASH_MAUS_LVL_0;      /* No Drop check but Maus default */
            err = ReqMausLevel(0,1,MausLevel);

            if (err != FLASH_SUCCESS)
               goto ErrExit;                /* set VPP and Reset to default */
                                                       /* and exit with err */

                                                  /* switch to new baudrate */

            LogFileWrite("Try to switch to Baudrate: %s",pubCom);
            blFlashActive = false;      /* sonst geht die Umschaltung nicht */
            err = Flash_ulwMausChangeParms(pubCom);

            if (err != FLASH_SUCCESS)
            {  LogFileWrite("ERROR: Error during switch to new Baudrate by Maus");
               goto ErrExit;                /* set VPP and Reset to default */
            }                                          /* and exit with err */
            blConnected = true;
            break;
    }
                                              /* Einsprung: Flash für V850E */
    case 20:                                  /* ========================== */
            ubDevice = DEVICE_V850E;    /* V850E benötigt anderen Baudrate- */
            goto V850EContinue;                                    /* Patch */
    case 30:                                /* Einsprung: Flash für V850E2M */
            ubDevice = DEVICE_V850E2M;      /* ============================ */
            ulwLoadAddr = INTROL_START_V850E2M;

V850EContinue:

            pFlashPort->ulBaudRate = kIntroBaud;     /* init default values */
            pFlashPort->ucParity = kNone;
            if (!PortSetup(pFlashPort))
            {  LogFileWrite("ERROR: Default Baudrate for Intro-loader cannot be set");
               err = FLASH_BAUDRATE_SET_FAILED;
               goto ErrExit0;                                 /* switch off */
            }
                                                /* Now enter Bootstrap mode */
            {
            ulword i;
            for (i = 0; i < DevParms.ulwPM_F9; i++)
            {   if (Flash_blInitBootstrap())
                {  ProgressUpdate("Init Bootstrap, Attempt: %d",i+1);

                                      /* Send Interface Configuration Bytes */
                   PortSend(pFlashPort,(unsigned char*)"\x00",1);
                   TimeWait(DevParms.ulwPM_F10);
                   PortSend(pFlashPort,(unsigned char*)"\x00",1);
                   TimeWait(DevParms.ulwPM_F10);

                   if (FX4)                /* Read back and garbage as well */
                   {  TimeWait(DevParms.ulwPM_F10);  // Delay to ensure
                      PortClear(pFlashPort);
                   }
                                             /* Send Reset Frame depending  */
                                             /* on device type.             */
                   /* V859E2M changed type for length info from byte to word */
                   if (SendFrameMsg(0x1,0x1,(unsigned char *)"\x00"))
                   {                          /* Send Get Silicon Signature */

                      // Do Chip erase if requested
                      if (blChipErase)
                      {  if ((err = SendChipErase()) != FLASH_SUCCESS)
                            goto ErrExit;
                      }

                      aubDevInfo[0] = 0;              /* enable Device-Info */
                      if (FlashParms.ubReadSignature)
                      {  LogFileWrite("Start reading the Signature from Device");

                         if (Flash_ulwReadDeviceInfo(&aubDevInfo[1]) != FLASH_SUCCESS)
                         {  ProgressUpdate("ERROR: Read Signature failed");
                            err = FLASH_DEVINFO_FAILED;
                            goto ErrExit;   /* set VPP and Reset to default */
                         }                             /* and exit with err */
                         else if (blClearDataFlash)
                         { sprintf((char*)ucBuffer,"\x22");
                           CopyU32ToBuf(&ucBuffer[1],ulwDataFlash-0x7fff);
                           CopyU32ToBuf(&ucBuffer[5],ulwDataFlash);
                           ProgressUpdate("Erase DataFlash");
                           if (!SendFrameMsg(0x1,0x9,ucBuffer,3,1,5))
                           {
                              ProgressUpdate("ERROR: Erase Dataflash failed");
                              err = FLASH_NO_SUCCESS;
                              goto ErrExit; /* set VPP and Reset to default */
                           }
                         }
                         break;
                      }
                      else
                      {             /* Read Signature is always required !! */
                         ProgressUpdate("ERROR: Read Signature is required");
                         err = FLASH_NO_SUCCESS;
                         goto ErrExit;   /* set VPP and Reset to default */
                      }
                   }
                }
                if (blAbort) break;                        /* evtl. Abbruch */
            }
            if ((i >= DevParms.ulwPM_F9) || blAbort)
            {  LogFileWrite("ERROR: No response on entering the Bootstrap-mode");
               err = FLASH_CONNECT_FAILED;
               goto ErrExit;                /* set VPP and Reset to default */
            }                                          /* and exit with err */
            }
    case 21:                        /* start beginning at Request Bootstrap */
    case 31:
            {
            ulword Addr = ulwLoadAddr;
            unsigned char ucBuffer[10];

            aubDevInfo[0] = 0xff;                    /* disable Device-Info */

            if (ubDevice == DEVICE_V850E2M)
            {  /* Get device info only if ReadSignature is enabled */
               if (FlashParms.ubIgnoreOptionFlags == 0)
               {  ProgressUpdate("Get Device information of FX4 !");

                  FX4Virgin = FX4Default = 0;             /* Init Option flags */
                  if (((err = GetV850Info((char*)"Get Option Bytes",0xAA)) != FLASH_SUCCESS) ||
                      ((err = GetV850Info((char*)"Get Security flags",0xA1)) != FLASH_SUCCESS) ||
                      ((err = GetV850Info((char*)"Get OCD ID",0xA7)) != FLASH_SUCCESS) )
                       goto ErrExit;

                  if (FX4Virgin == 7) ProgressUpdate("Sys-Option flags are virgin");
                  else if (FX4Default == 7) ProgressUpdate("Sys-Option flags unchanged");
               }
         else LogFileWrite("Ignore Read of Optionbytes is active. Don't read Device Info !");

               ubyte BaudRate = (pConnectPort->ulBaudRate == pFlashPort->ulBaudRate) ? 0x0 :
                                (pConnectPort->ulBaudRate == 115200) ? 0x1 :
                                (pConnectPort->ulBaudRate == 500000) ? 0x2 :
                                (pConnectPort->ulBaudRate == 1000000) ? 0x3 :
                                (pConnectPort->ulBaudRate == 2000000) ? 0x4 : 0xFF;

               if (BaudRate == 0xFF)
               {  ProgressUpdate("ERROR: Baud rate (%d) not supported by device",pConnectPort->ulBaudRate);
                  err = FLASH_BAUDRATE_SET_FAILED;
                  goto ErrExit;                /* set VPP and Reset to default */
               }

               if (BaudRate != 0)
               {
                  ProgressUpdate("Send Baud rate setting processing before Introloader (0x%x)", BaudRate);
                  TimeWait(DevParms.ulwPM_01);

                  /* Only 4 Mhz and 16 Mhz is supported */
                  if ((DevParms.ulwOscClock != 4000000) && (DevParms.ulwOscClock != 16000000))
                  {  err = FLASH_BAUDRATE_SET_FAILED;
                     goto ErrExit;                /* set VPP and Reset to default */
                  }

                  unsigned char tbFreq[2][5] = { {0x90,4,0,0,4 },{0x90,1,6,0,5} };
                  int idx = (DevParms.ulwOscClock == 4000000) ? 0 : 1;
                  // Frequency set processing
                  if (!SendFrameMsg(0x1,0x5,tbFreq[idx],3,3))
                  {  LogFileWrite("ERROR: No response for Frequency rate setting processing");
                     err = FLASH_BAUDRATE_SET_FAILED;
                     goto ErrExit;                /* set VPP and Reset to default */
                  }

                  // Baud rate set processing
                  //sprintf((char*)ucBuffer,"\x9A\x0");
                  ucBuffer[0] = 0x9A;
                  ucBuffer[1] = BaudRate;

                  if (!SendFrameMsg(0x1,0x2,ucBuffer))
                  {  LogFileWrite("ERROR: No response for Baud rate setting processing");
                     err = FLASH_BAUDRATE_SET_FAILED;
                     goto ErrExit;                /* set VPP and Reset to default */
                  }
                  TimeWait(DevParms.ulwPM_01);
                  LogFileWrite("Switch to new Baudrate before Introloader");
                  pFlashPort->ulBaudRate = pConnectPort->ulBaudRate;
                  //pFlashPort->ucParity = pConnectPort->ucParity;  Muß erst mal noch NonParity bleiben

                  if (!PortSetup(pFlashPort))
                  {  LogFileWrite("ERROR: switch to Baudrate for Introloader was not successful");
                     err = FLASH_BAUDRATE_SET_FAILED;         /* exit with err */
                     goto ErrExit;             /* set VPP and Reset to default */
                  }
                  // Reset Commmand
                  if (!SendFrameMsg(0x1,0x1,(unsigned char*)"\x00"))
                  {  err = FLASH_INTROLOADER_FAILED;
                     goto ErrExit;          /* set VPP and Reset to default */
                  }
               }
            }

            ProgressUpdate("Request Bootstrap");

            sprintf((char*)ucBuffer,"\xD0");
            CopyU32ToBuf(&ucBuffer[1],Addr);

            if (!SendFrameMsg(0x1,0x5,ucBuffer))
            {  LogFileWrite("ERROR: No response for the Bootstrap-request");
               err = FLASH_BOOTSTRAP_FAILED;
               goto ErrExit;                /* set VPP and Reset to default */
            }

                                                    /* transmit IntroLoader */
            ProgressUpdate("Download Introloader");
                                         /* Patch Baudrate for DEVICE_V850E */
            if ((ubDevice != DEVICE_V850E2M) && !Flash_vIntroLoaderPatchBaud(pConnectPort))
            {  LogFileWrite("ERROR: Baudrate not supported to patch Introloader");
               err = FLASH_BAUDRATE_SET_FAILED;
               goto ErrExit;                /* set VPP and Reset to default */
            }                                          /* and exit with err */

            int iLen = (int)DFInfo_ulwGetLen(&DFInfo,0x8100);
            if ((ubDevice == DEVICE_V850E2M && iLen > 1024) || (ubDevice != DEVICE_V850E2M && iLen >= 258))
            {  err = FLASH_INTROLOADER_SIZE_TOO_BIG;
               goto ErrExit;                /* set VPP and Reset to default */
            }                                          /* and exit with err */

            ubyte *ptr = IntParms.pIntroLoader;
            unsigned char DataCmd = (ubDevice == DEVICE_V850E2M) ? 0x11 : 0x2;  // SOD or STX command
            while (iLen > 0)
            {     int len = (iLen > (int)IntParms.ulwMaxFrameSize) ? (int)IntParms.ulwMaxFrameSize : iLen;
                  if (!SendFrameMsg(DataCmd, len, ptr, (len == iLen) ? 0x3 : 0x17))
                  {  err = FLASH_INTROLOADER_FAILED;
                     goto ErrExit;          /* set VPP and Reset to default */
                  }
                  ptr += len;                          /* and exit with err */
                  iLen -=len;
             }

            // send reverse Acknowledge if device == V850E2M
            if (ubDevice == DEVICE_V850E2M)
            {  TimeWait(DevParms.ulwPM_01);    /* Recv Ack -> Send Data Wait */
               if (!SendFrameMsg(0x11,0x1,(unsigned char *)"\x06",0x3,0))
               {  err = FLASH_INTROLOADER_FAILED;
                  goto ErrExit;          /* set VPP and Reset to default */
               }
            }

            goto waitILxmFW;  /* Hier kann der Rest vom V850 benutzt werden */
            }

                                          /* Einsprung: Flash via Connector */
    case 100:                             /* ============================== */
    {       int Repeat = 0;
            ubDevice = 0xff;    /* clear Device for Flash_ulwReadDeviceInfo */

            do                            /* Reset und Maus synchronisieren */
            {
              if ((ulwMode & FLASH_MODE_RESET_CONN_CB) == 0)
              {
                 LogFileWrite("Switch off/on VPP");
                 PortSetDTR(pFlashPort, !FLASH_RESET);   /* deaktivate reset */
                 PortSetRTS(pFlashPort, !FLASH_VPP);              /* Vpp off */
                 int del = Repeat ? Repeat * 20 : 5;
                 ProgressUpdate("Switch off VPP for %d seconds",del);
                 ProgressUpdate("Second: 0");
                 for (int i=0;i<del && !blAbort;i++)
                 {   ProgressRefresh("Second: %d",i+1);
                     TimeWait(1000000);               /* delay Repeat * 15 s */
                 }

                 PortSetRTS(pFlashPort, FLASH_VPP);                /* Vpp on */

                 if (blAbort)
                 {  LogFileWrite("Abort Connection by User");
                    err = FLASH_ABORTED;
                    goto ErrExit;           /* set VPP and Reset to default */
                 }                                 /* and exit with Aborted */

                 TimeSetStart();
                 TimeSetStop2(2500);

                 LogFileWrite("Wait 1.5 s for ASCI-Com");
                 TimeWait(1500);

                 pFlashPort->ulBaudRate = 4800;
                 pFlashPort->ucParity = kEven;
                 if (!PortSetup(pFlashPort))
                 {  LogFileWrite("ERROR: Switch to CDC-Default Baudrate was not successful");
                    err = FLASH_BAUDRATE_SET_FAILED;
                    goto ErrExit;           /* set VPP and Reset to default */
                 }                                     /* and exit with err */
                 do
                 {  unsigned long ulLength = 5;
                    memset(ucBuffer,0,10);                  /* get ACK-Frame */
                    if ((PortReceive(pFlashPort, ucBuffer, &ulLength, kDebug | kDelay) == kOK) &&
                       (ulLength > 1113) )
                    {  ProgressUpdate("ERROR: Peripherals connected to SCI-Port. Remove first");
                       err = FLASH_NO_SUCCESS;
                       goto ErrExit;        /* set VPP and Reset to default */
                    }                                  /* and exit with err */
                 } while (TimeIsStop2() == false);
              } /* reset and check for ASCI-Comm in Automatic Programming-Mode */
              /* Open Maus Request */

              pFlashPort->ulBaudRate = 19200;
              pFlashPort->ucParity = kEven;
              if (!PortSetup(pFlashPort))
              {  LogFileWrite("ERROR: Switch to Open Maus Baudrate was not successful");
                 err = FLASH_BAUDRATE_SET_FAILED;
                 goto ErrExit;              /* set VPP and Reset to default */
              }                                        /* and exit with err */
                                                    /* send Maus Listen Mode */
              MausSend(pFlashPort, 0x19, (unsigned char*)"\xAB\x31\x49\x17\x8\x27\x5\x77", 8);
              TimeWait(3000);                                  /* Maus-Delay */

                                           /* No Drop check, no Maus default */
              ubyte MausLevel = FLASH_MAUS_LVL_0P;
              err = ReqMausLevel(ulwMode & (FLASH_DROP_OFF<<8),0,MausLevel);
            } while ((err != FLASH_SUCCESS) && (++Repeat < 4));

            if (err != FLASH_SUCCESS)
            {  LogFileWrite("ERROR: Exit Flash_ulwDoConnect() without success");
               blConnected = false;
               goto ErrExit;                 /* set VPP and Reset to default */
            }                                        /* and exit with err */
            else ProgressUpdate("Level 0 - Protected is activated");
    }
    case 101:
    {
            aubDevInfo[0] = 0;                        /* enable Device-Info */
            if (FlashParms.ubReadSignature)
            {  LogFileWrite("Start reading the Signature from Device");

               if (Flash_ulwReadDeviceInfo(&aubDevInfo[1]) != FLASH_SUCCESS)
               {  ProgressUpdate("ERROR: Read Signature failed");
                  err = FLASH_DEVINFO_FAILED;
                  goto ErrExit;             /* set VPP and Reset to default */
               }                                       /* and exit with err */
            }
            aubDevInfo[0] = 0xff;                    /* disable Device-Info */

            #ifdef DEBUG_STOP
            IntParms.pErrMsg("Warte vor DownLoad Introloader",true);
            #endif

                                                    /* transmit IntroLoader */
            ProgressUpdate("Download IntroLoader");
            if (DevParms.ulwBlindBuf < 9)
            {  LogFileWrite("ERROR: Exit Flash_ulwDoConnect() because Mausbuf is too small (%d)",DevParms.ulwBlindBuf);
               blConnected = false;
               err = FLASH_NO_SUCCESS;
               goto ErrExit;                /* set VPP and Reset to default */
            }                                          /* and exit with err */

            if (!Flash_vIntroLoaderPatchBaud(pConnectPort))
            {  LogFileWrite("ERROR: Baudrate not supported to patch Introloader");
               err = FLASH_BAUDRATE_SET_FAILED;
               goto ErrExit;                /* set VPP and Reset to default */
            }                                          /* and exit with err */
            char cBuffer[1024];                 /* max, size of Introloader */
            memset(cBuffer, 0xff, 1024);
            int Len = DFInfo_ulwGetLen(&DFInfo,0x8100);

            if (Len > 1024)
            {  err = FLASH_INTROLOADER_SIZE_TOO_BIG;
               goto ErrExit;
            }

            memcpy(cBuffer, IntParms.pIntroLoader, Len );

            ulword Dest = DFInfo_ulwGetStart(&DFInfo,0x8100) + ulwLoadAddr;

            ulword l = DevParms.ulwBlindBuf;

            /***********Debug Test ********/
            if (DevParms.ulwBlindBuf < 9)
               LogFileWrite("Uups: 0 - Blindbuf is now too small %d" , l);
            if (DevParms.ulwMAUS_Buf < 9)
               LogFileWrite("Uups: 0 - MAUS_Buf is now too small %d" , l);
            /***********Debug Test ********/

            DevParms.ulwBlindBuf = (DevParms.ulwMAUS_Buf < 9) ? 9 : (DevParms.ulwMAUS_Buf-8);

            /* use MemWrite to transfer the intro loader */
            err = Flash_ulwWrite(0x10,IntParms.pIntroLoader,Dest,Len);
            DevParms.ulwBlindBuf = l;


            /***********Debug Test ********/
            if (DevParms.ulwBlindBuf < 9)
               LogFileWrite("1 - Blindbuf is now too small %d" , l);
            /***********Debug Test ********/


            if (err != FLASH_SUCCESS)
            {  LogFileWrite("ERROR: No response after transmit the Intro-loader");
               err = FLASH_INTROLOADER_FAILED;
               goto ErrExit;                /* set VPP and Reset to default */
            }                                          /* and exit with err */

    }
    case 102:
    {
            #ifdef DEBUG_STOP
            IntParms.pErrMsg("Warte vor Execute Introloader",true);
            #endif

            ProgressUpdate("Execute the IntroLoader");
            ucBuffer[0] = 0x10;
            ucBuffer[1] = 0x0;
            CopyU32ToBuf(&ucBuffer[2],ulwLoadAddr&0xFFFFFF00);

            if (ChkMausSend(pFlashPort, 0x85,ucBuffer, 6) == true)
            {  unsigned char ucLength = 255;

               ucBuffer[0] = 0;
               if (MausReceive (pFlashPort, 0x84, ucBuffer, &ucLength, 3, kGuard | kDebug | kDelay) == false)
                  err = Flash_ulwMausResult(0,0,0);
            } else err = FLASH_NO_SUCCESS;      /* Error bei Senden des cmd */

            if (err != FLASH_SUCCESS)
            {  LogFileWrite("ERROR: Timeout on execute the Introloader");
               goto ErrExit;                /* set VPP and Reset to default */
            }                                          /* and exit with err */
            goto waitILxmFW;
    }
                          /* undokumentierter Einsprung für Connection-Test */
                          /* Gerät On/Off mit Delay. !!Togglecount wird als
                             Anzahl der Sekunden vergewaltigt!!             */
    case 110:
    {         LogFileWrite("Switch off/on VPP with delay of %d sec",ubToggleCnt);
              PortSetDTR(pFlashPort, !FLASH_RESET);     /* deaktivate reset */
              PortSetRTS(pFlashPort, !FLASH_VPP);                /* Vpp off */
              ProgressUpdate("Switch off VPP for %d seconds",ubToggleCnt);
              ProgressUpdate("Second: 0");
              for (int i=0;i<ubToggleCnt && !blAbort;i++)
              {   ProgressRefresh("Second: %d",i+1);
                  TimeWait(1000000);                 /* delay ubToggleCnt s */
              }

              if (blAbort)
              {  LogFileWrite("Abort Connection by User");
                 err = FLASH_ABORTED;
                 goto ErrExit;              /* set VPP and Reset to default */
              }                                    /* and exit with Aborted */
              else goto SuccessExit;              /* else exit with success */
    }
    #ifdef SOFTWARETEST

                                            /* Einsprung: Roli's Debug-Test */
    case 50:                                /* ============================ */
    case 51:
    {
            ulword i;
            pFlashPort->ulBaudRate = kIntroBaud;     /* init default values */
            pFlashPort->ucParity = kNone;
            if (!PortSetup(pFlashPort))
            {  LogFileWrite("ERROR: Baudrate for Intro-loader cannot be set");
               err = FLASH_BAUDRATE_SET_FAILED;
               goto ErrExit0;                                 /* switch off */
            }
                                                /* Now enter Bootstrap mode */
            pFlashPort->ulTimeOut = FlashParms.uwFLWTimeOut;

            for (i = 0; i < DevParms.ulwPM_F9; i++)
            {   if (Flash_blInitBootstrap())
                {  ProgressUpdate("Init Bootstrap, Attempt: %d",i+1);

                                      /* Send Interface Configuration Bytes */
                   PortSend(pFlashPort,(unsigned char*)"\x00",1);
                   PortSend(pFlashPort,(unsigned char*)"\x00",1);
                   TimeWait (1000);                        /* 1000 = 1 mSec */
                                                      /* Send Reset Command */

                   PortSend(pFlashPort,(unsigned char*)"\x00",1);
                   if (PortReadOK (pFlashPort, 0x3c, (char*)"\0") == true)
                       break;
                }
                if (blAbort) break;                        /* evtl. Abbruch */
            }
            if ((i >= DevParms.ulwPM_F9) || blAbort)
            {  LogFileWrite("ERROR: No response on entering the Bootstrap-mode");
               err = FLASH_CONNECT_FAILED;
               goto ErrExit;                /* set VPP and Reset to default */
                                                       /* and exit with err */
            }

            aubDevInfo[0] = 0;                        /* enable Device-Info */
            if (FlashParms.ubReadSignature)
            {  LogFileWrite("Start reading the Signature from Device");
               if (Flash_ulwReadDeviceInfo(&aubDevInfo[1]) != FLASH_SUCCESS)
               {  ProgressUpdate("ERROR: Read Signature failed");
                  err = FLASH_DEVINFO_FAILED;
                  goto ErrExit;             /* set VPP and Reset to default */
                                                       /* and exit with err */
               }
            }

            if (ulwiMode == 50)
               break;



            aubDevInfo[0] = 0xff;                    /* disable Device-Info */
            ProgressUpdate("Request Bootstrap");
            PortSend(pFlashPort,(unsigned char*) "\xd0", 1);
            if (PortReadOK(pFlashPort, 0x3c, (char*)"\0") == false)
            {  LogFileWrite("ERROR: No response for the Bootstrap-request");
               err = FLASH_BOOTSTRAP_FAILED;
               goto ErrExit;                /* set VPP and Reset to default */
                                                       /* and exit with err */
            }
                                                    /* transmit IntroLoader */
            ProgressUpdate("Download Introloader");
                                          /* Patch Baudrate for DEVICE_V850 */
            if (!Flash_vIntroLoaderPatchBaud(pConnectPort))
            {  LogFileWrite("ERROR: Baudrate not supported to patch Introloader");
               err = FLASH_BAUDRATE_SET_FAILED;
               goto ErrExit;                /* set VPP and Reset to default */
                                                       /* and exit with err */
            }

            /* ------------------------------------------------------------ */
            /* es ist sicher egal, ob man im Falle eines kleineren Introloaders
               als 256 Byte einfach den Rest aus irgend einem Speichermüll
               überträgt, aber der CodeGuard meckert dann, weil es ihm nicht
               gefällt, wenn man anderer Leute Speicher mißbraucht.
               Deshalb gibt es hier ne kleine Codeverschwendung !!! */

            char cBuffer[256];   /* IntroLoader braucht leider 256 Byte fix */
            memset(cBuffer, 0xff, 256);
            memcpy(cBuffer, IntParms.pIntroLoader, DFInfo_ulwGetLen(&DFInfo,0x8100) );
            PortSend(pFlashPort, (unsigned char*)cBuffer, 256);
            /* so gings auch:
            PortSend(pFlashPort,(unsigned char*)IntParms.pIntroLoader, 256); */
             /* ------------------------------------------------------------ */

            if ((err = ReadPortSendResult()) != FLASH_SUCCESS)
               goto ErrExit;
            }
            break;
    }
    #endif

    default:
            LogFileWrite("unknown parameter \"Mode\" for Flash_ulwDoConnect()");
            err = FLASH_NO_SUCCESS;
            goto ErrExit;                   /* set VPP and Reset to default */
                                                       /* and exit with err */

  } /* end switch (ulwiMode) */

SuccessExit:

  blFlashActive = false;                                  /* dll = inactive */
  LogFileWrite("Exit Flash_ulwDoConnect() with success");
  return (FLASH_SUCCESS);

} /* end of Flash_ulwDoConnect() */



/*************************************************************************@FA*
 * @Function          : Flash_ulwDoDisconnect ()
 *----------------------------------------------------------------------------
 * @Description       : This function Quits MAUS Level 0 of FLW_Writer and
 *                      does a Vpp off
 *
 *----------------------------------------------------------------------------
 * @Returnvalue       : ulword ( success or error status )
 *----------------------------------------------------------------------------
 * @Parameters        : none
 *----------------------------------------------------------------------------
 * @Functioncalls     :
 *----------------------------------------------------------------------------
 * @Global_Vars_Used  :
 *----------------------------------------------------------------------------
 * @Copyright         : (c) 2001 Robert Bosch GmbH, Hildesheim
 * @Author            : CM-CR / ESD4  Peter Tuschik
 *----------------------------------------------------------------------------
 *
 * @History           : Initial Version
 *
 ************************************************************************@FE*/
DLLEXPORT ulword __stdcall Flash_ulwDoDisconnect (void)
{ ulword retval = FLASH_FLASHWRITER_FAILED;
  ubyte i;
                                                /* try to quit MAUS Level 0 */
  LogFileWrite("Entered Flash_ulwDoDisconnect()");
  blAbort = false;

  if (blFlashActive)
  {  LogFileWrite("ERROR: Flash850.dll is still active - Command ignored");
     blAbort = true;
     return (FLASH_ACCESS_DENIED);                /* ignore if dll = active */
  }

  blFlashActive = true;

  if (!blConnected)                     /* no Maus-Msg, if not Maus Level 0 */
     retval = FLASH_SUCCESS;
  else
  {
     for (i = 0;i <= FlashParms.ubTelegramRetries;i++)
     {   if (MausSend(pFlashPort,0x0d,NULL,0|EchoCom) == true)/* Quit Maus */
         {
            if (IntParms.f_DFile) fclose(IntParms.f_DFile);
            IntParms.f_DFile = NULL;
            retval = FLASH_SUCCESS;
            blConnected = false;
            break;
         }
         if (blAbort) break;                               /* evtl. Abbruch */
     }
  }
  PortSetRTS(pFlashPort, !FLASH_VPP);                     /* switch off VPP */
  TimeWait(DevParms.ulwPM_F11);                              /* wait 1 mSec */
  PortSetDTR(pFlashPort, !FLASH_RESET);                        /* Reset off */

  if (retval != FLASH_SUCCESS)
      LogFileWrite("ERROR: No response for Quit-Maus command");
  else LogFileWrite("Flashwriter successfully disconnected");

  PortSwitch(pFlashPort,P_OFF,false);        /* Default: Interface disablen */
  blConnected = false;         /* damit auch bei Fehler beendet werden kann */
  blFlashActive = false;
  return retval;                     /* error in com during communication ? */

} /* end of Flash_ulwDoDisconnect() */


/*************************************************************************@FA*
 * @Function          : Flash_ubDoReset ()
 *----------------------------------------------------------------------------
 * @Description       : This function resets the processor by reseting the
 *                      VPP line and bringing the reset line low for 1ms
 *----------------------------------------------------------------------------
 * @Returnvalue       : ubyte - FMG_SUCCESS / FMG_NOSUCCESS
 *----------------------------------------------------------------------------
 * @Parameters        : None
 *----------------------------------------------------------------------------
 * @Functioncalls     : PortSetDTR()
 *----------------------------------------------------------------------------
 * @Global_Vars_Used  : None
 *----------------------------------------------------------------------------
 * @Copyright         : (c) 2001 Robert Bosch GmbH, Hildesheim
 * @Author            : CM-CR / ESD4  Peter Tuschik
 *----------------------------------------------------------------------------
 *
 * @History           : Initial version
 *
 ************************************************************************@FE*/
DLLEXPORT ulword __stdcall Flash_ulwDoReset (void)
{ ulword retval;

  blAbort = true;

  LogFileWrite("Entered Flash_ulwDoReset()");
  if (blFlashActive)
  {  LogFileWrite("ERROR: Flash850.dll is still active - Command ignored");
     return (FLASH_ACCESS_DENIED);                /* ignore if dll = active */
  }

  blAbort = false;
  retval = Flash_ulwDoDisconnect();/* quits MAUS level 0 and switch off VPP */
  PortSetDTR(pFlashPort, FLASH_RESET);                    /* reset aktivate */
  TimeWait (1000);                                              /* for 1 ms */
  PortSetDTR(pFlashPort, !FLASH_RESET);                 /* deaktivate reset */
  return retval;
}


/*************************************************************************@FA*
 * @Function          : Flash_ulwGetDeviceInfo ()
 *----------------------------------------------------------------------------
 * @Description       : This function returns the Flash device Code of the
 *                      connected NEC V850 Device
 *----------------------------------------------------------------------------
 * @Returnvalue       : ulword - FMG_SUCCESS / FMG_NOSUCCESS
 *----------------------------------------------------------------------------
 * @Parameters        : ubyte * DeviceInfo : ptr to data
 *----------------------------------------------------------------------------
 * @Functioncalls     : PortSetDTR()
 *----------------------------------------------------------------------------
 * @Global_Vars_Used  : None
 *----------------------------------------------------------------------------
 * @Copyright         : (c) 2001 Robert Bosch GmbH, Hildesheim
 * @Author            : CM-CR / ESD4  Peter Tuschik
 *----------------------------------------------------------------------------
 *
 * @History           : Initial version
 *
 ************************************************************************@FE*/
DLLEXPORT ulword __stdcall Flash_ulwGetDeviceInfo (ubyte * DeviceInfo)
{
  LogFileWrite("Entered Flash_ulwGetDeviceInfo()");
  if (aubDevInfo[0] != 0xff)                // no Device-Info available
  {  LogFileWrite("ERROR: Deviceinfo rejected, because DeviceInfo not read");
     return FLASH_DEVINFO_REJECT;           // already entered
  }
  else strcpy((char*)DeviceInfo,(char*)&aubDevInfo[1]);
  LogFileWrite("DeviceInfo succesfully read");
  return (FLASH_SUCCESS);
}

/*************************************************************************@FA*
 * @Function          : Flash_ulwReadDeviceInfo ()
 *----------------------------------------------------------------------------
 * @Description       : This function returns the Flash device Code of the
 *                      connected NEC V850 Device
 *----------------------------------------------------------------------------
 * @Returnvalue       : ulword - FMG_SUCCESS / FMG_NOSUCCESS
 *----------------------------------------------------------------------------
 * @Parameters        : ubyte * DeviceInfo : ptr to data
 *----------------------------------------------------------------------------
 * @Functioncalls     : PortSetDTR()
 *----------------------------------------------------------------------------
 * @Global_Vars_Used  : None
 *----------------------------------------------------------------------------
 * @Copyright         : (c) 2001 Robert Bosch GmbH, Hildesheim
 * @Author            : CM-CR / ESD4  Peter Tuschik
 *----------------------------------------------------------------------------
 *
 * @History           : Initial version
 *
 ************************************************************************@FE*/
ulword Flash_ulwReadDeviceInfo (ubyte * DeviceInfo)
{ ubyte *pucSignature = ucBuffer;
  unsigned char *DFSignature = DevParms.uDevSign.Parts.aubSignature;

  unsigned long ulLength = 255;
  unsigned char ucLength = 255;
  ulwDataFlash = 0;

  LogFileWrite("Entered Flash_ulwReadDeviceInfo()");

  switch (ubDevice)
  {
    case DEVICE_V850:

         if (PortSend(pFlashPort,(unsigned char*) "\xc0", 1) == 0)
         {  LogFileWrite("ERROR: No response for DeviceInfo-Request");
            return (FLASH_NO_SUCCESS);
         }


         do
         { if (PortReceive(pFlashPort, pucSignature, &ulLength, kDebug) != kOK)
           {  LogFileWrite("ERROR: Reading of DeviceInfo not successful");
              return (FLASH_NO_SUCCESS);
           }
           else
           {  if (ulLength == 1)
                 *pucSignature = 0x20;             // if 1st char is 0x3c, dont stop
              pucSignature += ulLength;
           }

         } while ((*(pucSignature-1) != 0x3c) && (pucSignature < (ucBuffer+255)));

         ConvertSignature(DeviceInfo,ucBuffer+7);

         while (*(pucSignature-1) != 0x3c)
         { pucSignature = ucBuffer;
           ulLength = 255;
           if (PortReceive(pFlashPort, pucSignature, &ulLength, kDebug)  != kOK)
           {  LogFileWrite("ERROR: Reading of Rest of DeviceInfo not successful");
              return (FLASH_NO_SUCCESS);
           }
           else if (ulLength == 0) return FLASH_NO_SUCCESS;
                else pucSignature += ulLength;
         }
         if (ucBuffer[0] != 0x3c)
            LogFileWrite("ERROR: WARNING ! DeviceInfo seems to be invalid !");

         if (ubDevice != DecodeDevice((char*)DeviceInfo))
         {  LogFileWrite("ERROR: Read Devicetype is different from selection");
            return (FLASH_NO_SUCCESS);
         }
         break;

    case DEVICE_V850E:
    case DEVICE_V850E2M:

         if  (SendFrameMsg(0x1,0x1,(unsigned char *)"\xC0"))
         {  ulLength = 260;
            memset(ucBuffer,0,260);

            if (ubDevice == DEVICE_V850E2M) SendFrameMsg(0x11,0x1,(unsigned char *)"\x06",0x3,0);
            if (PortReceive(pFlashPort, ucBuffer, &ulLength, kDebug)  != kOK)
            {  LogFileWrite("ERROR: Reading of DeviceInfo not successful");
               return (FLASH_NO_SUCCESS);
            }
            PortClear(pFlashPort); /* read rest, if size has changed for newer devices */

            ubyte *ptr = (ucBuffer[4] == 0x4) ? ucBuffer+19 : ucBuffer+8;
            if (ubDevice == DEVICE_V850E2M)
            {  ptr = ucBuffer+6;
               ulwDataFlash = ((ulword)*(ptr+17)<<24) + ((ulword)*(ptr+16)<<16) + ((ulword)*(ptr+15)<<8) + (ulword)*(ptr+14);
            }

            ConvertSignature(DeviceInfo,ptr);

            if (ubDevice != DecodeDevice((char*)DeviceInfo))
            {  LogFileWrite("ERROR: Read Devicetype is different from selection");
               return (FLASH_NO_SUCCESS);
            }
         }
         else return (FLASH_NO_SUCCESS);
         break;

    case 0xFF:                           /* Dummy device für Anschlußkasten */

         if (MausSend(pFlashPort, 0x94, NULL, 0))
         {  ucLength = 255;
            memset(ucBuffer,0,255);
            if (MausReceive(pFlashPort, 0x95, ucBuffer, &ucLength, 10, kDebug|kDelay) == false)
            {  LogFileWrite("ERROR: Reading of DeviceInfo not successful");
               return (FLASH_NO_SUCCESS);
            }
                                               /* Def.: Signature aus Device */
            ubyte *ptr = (ucBuffer[5] == 0x4) ? ucBuffer+20 : ucBuffer+9;
            pucSignature = ConvertSignature(DeviceInfo,ptr);

            if (DFSignature[0] != 0)
            { /* falls Deviceinfo vorhanden, diesen zur Typbestimmung nutzen */
               if ((strstr((char*)DFSignature,(char*)DeviceInfo) == NULL) || !DeviceInfo[0])
               {  LogFileWrite("Warning: Signature is different from DeviceInfo");
                  pucSignature = DFSignature;
               }
            }

            ubDevice = DecodeDevice((char*)pucSignature);

         }
         else return (FLASH_NO_SUCCESS);
         break;
    default: return (FLASH_NO_SUCCESS);
  }

  LogFileWrite("Expected SysClock = %d MHz, OscFreq = %d Hz",SysClock,DevParms.ulwOscClock);
  LogFileWrite("DeviceInfo succesfully read");
  ProgressUpdate("Signature = %s", DeviceInfo);

  return (FLASH_SUCCESS);
}


/*************************************************************************@FA*
 * @Function          : Flash_ulwGetParms
 *----------------------------------------------------------------------------
 * @Description       : This function return the FlashParms of the current
 *                      settings
 *----------------------------------------------------------------------------
 * @Returnvalue       : ulword 0: okay, else not okay
 *----------------------------------------------------------------------------
 * @Parameters        : Flash_TYParms * pstFlashParms
 *----------------------------------------------------------------------------
 * @Functioncalls     : memcpy
 *----------------------------------------------------------------------------
 * @Global_Vars_Used  : None
 *----------------------------------------------------------------------------
 * @Copyright         : (c) 2001 Robert Bosch GmbH, Hildesheim
 * @Author            : CM-CR / ESD4  Peter Tuschik
 *----------------------------------------------------------------------------
 *
 * @History           : Initial version
 *
 ************************************************************************@FE*/
DLLEXPORT ulword __stdcall  Flash_ulwGetParms (Flash_TYParms * pstFlashParms)
{
  if (pstFlashParms->ubVerify == 0x80)
     pstFlashParms->ubVerify = FLASH_VERSION;
  else memcpy((void*)pstFlashParms,(void*)&FlashParms,sizeof(FlashParms));
  pstFlashParms->pRS232 = pFlashPort;
  return FLASH_SUCCESS;
}


/*************************************************************************@FA*
 * @Function          : Flash_ulwSetParms
 *----------------------------------------------------------------------------
 * @Description       : With this function new FlashParms can be stored
 *----------------------------------------------------------------------------
 * @Returnvalue       : ulword 0: okay, else not okay
 *----------------------------------------------------------------------------
 * @Parameters        : Flash_TYParms * pstFlashParms
 *----------------------------------------------------------------------------
 * @Functioncalls     : memcpy
 *----------------------------------------------------------------------------
 * @Global_Vars_Used  : None
 *----------------------------------------------------------------------------
 * @Copyright         : (c) 2001 Robert Bosch GmbH, Hildesheim
 * @Author            : CM-CR / ESD4  Peter Tuschik
 *----------------------------------------------------------------------------
 *
 * @History           : Initial version
 *
 ************************************************************************@FE*/
DLLEXPORT ulword __stdcall  Flash_ulwSetParms (Flash_TYParms * pstFlashParms)
{
  if (blFlashActive)                    /* not valid during active transfer */
     return FLASH_ACCESS_DENIED;
                                        /* else copy parameter to structure */
  memcpy((void*)&FlashParms,(void*)pstFlashParms,sizeof(FlashParms));
  char help[1024];                    /* copy to help-Buffer, not to modify */
  strcpy(help,pstFlashParms->LogPath);            /* the original file name */

  if (strstr(toUpper(help),".TXT") == 0)
     sprintf(LogFileName,"%sLogFile.txt",FlashParms.LogPath);
  else sprintf(LogFileName,"%s",FlashParms.LogPath);


  LogFileWrite("Entered FLASH_ulwSetParms");
  LogFileWrite("New values are: %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d",
                FlashParms.uwTelegramTimeOut,FlashParms.ubTelegramLength,
                FlashParms.ubVerify,FlashParms.ubReadSignature,
                FlashParms.ubTelegramRetries,FlashParms.ubProgrammingRetries,
                FlashParms.ubEraseTimeOut,FlashParms.ubWriteTimeOut,
                FlashParms.uwPreDnlDelay,FlashParms.uwFLWTimeOut,
                FlashParms.ubWriteRandomEnable);

  DebOS_Delay = ((FlashParms.ubVerify & 0x7E) >> 1) * 200;
  FlashParms.ubVerify &= 0x1;

  if (DevParms.ulwBlindBuf > (ulword)FlashParms.ubTelegramLength)
     DevParms.ulwBlindBuf = SetBlindBuf(FlashParms.ubTelegramLength);

  LogFileWrite("Exit FLASH_ulwSetParms");
  return FLASH_SUCCESS;
}


/*************************************************************************@FA*
 * @Function          : Flash_ulwDoDownloadFileTransfer
 *----------------------------------------------------------------------------
 * @Description       : This function manages the flash programming task.
 *                      It calls the download for the non disabled devices
 *----------------------------------------------------------------------------
 * @Returnvalue       : ulword indicating Success/Error
 *----------------------------------------------------------------------------
 * @Parameters        : pubValidBlocks with info of memtypes to download
 *                      ulwMode with bitmask control informations
 *                      ulwRNDNum with Random number to programme
 *----------------------------------------------------------------------------
 * @Functioncalls     : Flash_ulwDownload
 *----------------------------------------------------------------------------
 * @Global_Vars_Used  :
 *----------------------------------------------------------------------------
 * @Copyright         : (c) 2001 Robert Bosch GmbH, Hildesheim
 * @Author            : CM-CR / ESD4  Peter Tuschik
 *----------------------------------------------------------------------------
 *
 * @History           : Initial Version
 ************************************************************************@FE*/
DLLEXPORT ulword __stdcall Flash_ulwDoDownloadFileTransfer (uword  *puwValidBlocks, ulword ulwMode,
                                                 __int64 ulwRNDNum)
{ ulword RetVal = FLASH_NO_SUCCESS, ExitVal = FLASH_SUCCESS;

  LogFileWrite("Entered Flash_ulwDoDownloadFileTransfer with: puwBlkListe, 0x%08X, 0x%08X%08X", ulwMode, (unsigned int)(ulwRNDNum>>32),(unsigned int)ulwRNDNum);

  if (blFlashActive)
     return (FLASH_ACCESS_DENIED);          // ignore if dll = active
                                            // suppress programming erased data ?

  BlockList = puwValidBlocks;               // Liste der gültigen zu Blöcke zuweisen
  blProgSuppress = !(ulwMode & FLASH_MODE_FAST_PROG);
  blUpdate       = !(ulwMode & FLASH_MODE_UPDATE);
  blProgInvalid  = false;                   // Default: Int. Flash ist programmierbar
                                            // disable erase for all devices ?
  ubyte ubEraIni = (ulwMode & FLASH_MODE_GLOB_ERASE ) ? 0xff : 0;
  for (int j = 0; j < FLASH_MAX_DEV; j++)
      aubEraseDev[j] = ubEraIni;            // init erase-state


  if (KDS_Buf) free(KDS_Buf);               // Read KDS-content to buffer
  KDS_Buf = NULL;                           // if required
  unsigned char *ptrKDS = NULL;

  if (!blUpdate && !(ulwMode & FLASH_MODE_KDS_SAVE) && DFInfo_blReadData(IntParms.f_DFile,&DFInfo,&ptrKDS, 0x8000))
  { ptrKDS = ptrKDS + KDS_Offset + 1 + 2;

    unsigned long StartAddr = (*(ptrKDS) << 24) + (*(ptrKDS+1) << 16) +
                              (*(ptrKDS+2) << 8) + *(ptrKDS+3);
    KDS_Size                = (*(ptrKDS+4) << 24) + (*(ptrKDS+5) << 16) +
                              (*(ptrKDS+6) << 8) + *(ptrKDS+7);

    KDS_Buf = (unsigned char*)malloc((KDS_Size * sizeof(unsigned char)));
    if (KDS_Buf)
       ExitVal = Flash_ulwReadMemory (0x40,KDS_Buf,StartAddr,KDS_Size);
    else ExitVal = FLASH_NO_SUCCESS;
    if (ExitVal == (ulword)FLASH_SUCCESS) IntParms.pKDSXchg = GetMyKDSData;
  }


  if (!(ulwMode & FLASH_MODE_CHIP_ERASE))
  {  if (!blUpdate) ExitVal = SendChipErase();
     else ProgressUpdate("Chiperase is not possible on update. Virgin flash required.");
  }

  blFlashActive = true;                               // now dll = active
  bool blCDMB_Used = false, blKDS_Used = false, blFGS_Used = false;
  uword Typ,MemType;

  if (blUpdate && aubComNr[0] != 0)
  {
    if (!bCheckCompatibility())
       ExitVal = FLASH_COMP_NUM_INVALID;
  }

  if (ExitVal == (ulword)FLASH_SUCCESS)
  {
    for (int i = 0; i < DFInfo.Count; i++)
    {   bool found = false;
        RetVal = FLASH_SUCCESS;                       // default no error

        Typ = DFInfo.Header[i].Typ;
        MemType = (Typ >> 8);

        switch (MemType)
        { case 0x40:                                  // write int. Flash
          case 0x42:                                  // write ext. par. Flash
          case 0x43:                                  // write secondary Flash
          case 0x44:                                  // write ext. ser. Flash #1
          case 0x45:                                  // write ext. ser. Flash #2
          case 0x53:                                  // write ADR Trojan
          case 0x54:                                  // write ADR
          case 0x30:                                  // write int. EEP
          case 0x38:                                  // write MemoryCard
          case 0x95:                                  // (DAB Trojan version)
          case 0xC4:                                  // write DAB D-Fire 2 (Flash)
          case 0x94:                                  // write DAB D-Fire 2 (RAM)
          case 0x97:                                  // (DSA Trojan version)
          case 0xC6:                                  // write DSA (Flash)
          case 0x96:                                  // write DSA (RAM)
          case 0x4C:                                  // write FGS configuration
          case 0x4D:                                  // write FGS 2nd parallel Flash
          case 0x4E:                                  // FGS Parallel Flash
          case 0x4F:                                  // Internal Data Flash
          case 0x34:                                  // write ext. ser. EEP
                     found = bCheckValidBlock(Typ);
                     break;
          case 0x4B:                                  // FGS Parallel Flash
                     if (bCheckValidBlock(Typ))       // Sonderprogrammierung
                         blFGS_Used = true;           // veranlassen
                     break;
        }


        if (found ||
            ((Typ == 0x40EF) && blCDMB_Used && CDMB_Offset && !blDebug) ||
            ((Typ == 0x40ED) && blKDS_Used && KDS_Offset && !blDebug))
        {  SYSTEMTIME t0;

           // - Switch VPP-/VCC Drop only for internal Flash - Rest only VCC-Drop
           RetVal = SetDropDetection(ulwMode | ((MemType == 0x40) ? 0x0 : 0x1)); //ulwMode & 0x3);

           if (RetVal == (ulword)FLASH_SUCCESS)
           {
              GetSystemTime(&t0);

              if (Typ == 0x40EF)                      // CDMB-Daten schreiben
              {  RetVal = CreateConfigDataMemBlock(blUpdate);
                 blCDMB_Used = false;
              }
              else if (Typ == 0x40ED)                 // KDS-Daten schreiben
              {  RetVal = CreateKDSDataMemBlock();
                 blKDS_Used = false;
              }
              else                                    // normale Daten schreiben
              {  RetVal = Flash_ulwDownload(Typ,ulwRNDNum);
                 if ((MemType == 0x40) && !blUpdate && !blProgInvalid)
                 {  blCDMB_Used = true;               // evtl. CDMB-Daten
                    blKDS_Used = true;                // und KDS-Daten schreiben
                 }
              }

              if (RetVal == (ulword)FLASH_SUCCESS)
              {
                 ProgressUpdate("Download of Block %04X successfull in %s",Typ,GetTimeDifference(t0));
              }
              else
              {
                 if ( (RetVal != FLASH_VPP_DROP) && (RetVal != FLASH_VCC_DROP) &&
                      (!(ulwMode & FLASH_MODE_HISTORY)) &&
                      (!((MemType == 0x34) && (RetVal == FLASH_REJECT))) )
                    Flash_vWriteErrHistory(Typ,RetVal); // write error history
                 LogFileWrite("ERROR: %x in Typ : %x during Flash_ulwDownload",RetVal,Typ);
                 /*Langer 13.01.2003 begin*/
                 ExitVal = RetVal;
                 break; // for
                 /*Langer 13.01.2003 end*/
              } /* end write Error History */
           } /* if (RetVal == FLASH_SUCCESS) */
        } /* if (found) */

        if (blAbort)                                  // abort by user
        {  ExitVal = FLASH_ABORTED;
           break;
        }
        if (RetVal != FLASH_SUCCESS)
           ExitVal = RetVal;

    } /* end for (;;;) */
  }

  if ( (ExitVal == FLASH_SUCCESS) && blFGS_Used )
     ExitVal = FGS_ulwDownload(ulwMode);              // Programm handling for FGS

  if ( (ExitVal == FLASH_SUCCESS) &&                  // if success, then
       (!(ulwMode & FLASH_MODE_HISTORY)) )            // write new Version
       Flash_vWriteErrHistory(0,RetVal);

  if ( (ExitVal == FLASH_SUCCESS) && !blUpdate &&    // if success, then
       (ubDevice == DEVICE_V850E2M) && FX4)          // write option Flags
  {
    if (FX4Virgin == 7)                              // write only in case of
    {                                                // virgin state
       ProgressUpdate("Write Security option flags");
       ucBuffer[0] = 0x40;
       ucBuffer[1] = 0xA9;
       memcpy(&ucBuffer[2],DevParms.aubSysOpt,sizeof(DevParms.aubSysOpt));
       if ((ExitVal = MausSendWaitAck(pFlashPort, 0x8F, 6, 3, kDebug|EchoCom)) == FLASH_SUCCESS)
       {  ucBuffer[0] = 0x40;
          ucBuffer[1] = 0xA0;
          memcpy(&ucBuffer[2],DevParms.aubSysSec,sizeof(DevParms.aubSysSec));
          if ((ExitVal = MausSendWaitAck(pFlashPort, 0x8F, 8, 3, kDebug|EchoCom)) == FLASH_SUCCESS)
          {  ucBuffer[0] = 0x40;
             ucBuffer[1] = 0xA6;
             memcpy(&ucBuffer[2],DevParms.aubSysOCD,sizeof(DevParms.aubSysOCD));
             ExitVal = MausSendWaitAck(pFlashPort, 0x8F, 14, 3, kDebug|EchoCom);
          }
       }
    }
    else if (FX4Default != 7) ProgressUpdate("Skip write Security option flags, because not virgin");
    else ProgressUpdate("Skip write Security option flags, because unchanged");
  }


  blAbort       = false;
  blUpdate      = false;
  blFlashActive = false;                              // dll = inactive
  blErased      = false;

  LogFileWrite("Exit Flash_ulwDoDownloadFileTransfer");
  return (ExitVal);
}


/*************************************************************************@FA*
 * @Function          : Flash_ulwGetDFDeviceContent
 *----------------------------------------------------------------------------
 * @Description       : This function read the specified content of a device
 *                      into a buffer and pass it to the Application
 *----------------------------------------------------------------------------
 * @Returnvalue       : ulword Result
 *----------------------------------------------------------------------------
 * @Parameters        : - pubDF_Filename -> Filename of DF-File
 *                      - puwValidBlocks -> info of memtypes to download
 *                      - ulwMode -> informations about type of mem allocation
 *                        ( 1 = allocate actual used memory,
 *                          2 = allocate memory from 0x00 to end of last block
 *                          3 = allocate memory in size of the device)
 *                      - ubInit -> default value to init the free spaces
 *                      - **Memory -> pointer to data block
 *----------------------------------------------------------------------------
 * @Functioncalls     :
 *----------------------------------------------------------------------------
 * @Global_Vars_Used  :
 *----------------------------------------------------------------------------
 * @Copyright         : (c) 2005 Robert Bosch GmbH, Hildesheim
 * @Author            : CM-CR / ESD4  Peter Tuschik
 *----------------------------------------------------------------------------
 *
 * @History           : Initial Version
 ************************************************************************@FE*/
DLLEXPORT ulword __stdcall Flash_ulwGetDFDeviceContent (ubyte *pubDF_Filename, uword  * puwValidBlocks,
                                                        ulword ulwMode, ubyte ubInit, ubyte **Memory,
                                                        ulword *ulwStart, ulword *ulwSize)
{ ulword RetVal;

  LogFileWrite("Entered Flash_ulwGetDFDeviceContent with: %s, puwBlkListe, 0x%08X, 0x%02X **Memory", pubDF_Filename, ulwMode, ubInit);
  FILE * f_DFile;
  Flash_strTYInfo TempInfo;

  if (*Memory) free(*Memory);        /* evtl. vorherigen Speicher freigeben */

  f_DFile = fopen((const char*)pubDF_Filename,"rb");
  if (f_DFile == NULL)
  {  LogFileWrite("ERROR: DNL-File cannot be opened");
     return (FLASH_DF_FILE_IO_ERROR);
  }

  BlockList = puwValidBlocks;               /* Liste der gültigen zu Blöcke */
                                                                /* zuweisen */
  if (BlockList != NULL)                         /* raus, falls keine Liste */
  {  uword *Liste = BlockList;                               /* vorhanden ! */
     uword Device = *Liste & 0xFF00;
     ulword Start = 0xffffffff, MinStart = 0xffffffff, End = 0, MaxEnd = 0;

     RetVal = DFInfo_ulwInit(f_DFile,&TempInfo);       /* DNL-File einlesen */

     while (*Liste && (RetVal == FLASH_SUCCESS))
     {                                      // ungültiger Parameter prüfen
       if ((*Liste == 0xffff) || (Device != (*Liste & 0xFF00)))
          RetVal = FLASH_PARM_ERROR;
       Liste++;
     }

     if (RetVal == FLASH_SUCCESS)
     {
        blUpdate = false;               /* Keine Einschränkung durch Update */
                                                   /* Start und Ende suchen */
        for (int i = 0; i < TempInfo.Count; i++)
        {   ulword BlkEnd = (TempInfo.Header[i].Start+TempInfo.Header[i].Length);

                                        /* Maximale Range des Device merken */
            if ((TempInfo.Header[i].Typ & 0xFF00) == Device)
            {  if (MinStart > TempInfo.Header[i].Start) MinStart = TempInfo.Header[i].Start;
               if (MaxEnd < BlkEnd) MaxEnd = BlkEnd;
            }
                              /* Reale Range der ausgewählten Blöcke merken */
            if ( bCheckValidBlock(TempInfo.Header[i].Typ) )
            {  if (Start > TempInfo.Header[i].Start) Start = TempInfo.Header[i].Start;
               if (End < BlkEnd) End = BlkEnd;
            }
        }
        if (ulwMode == 2) Start = 0;
        if (ulwMode == 3) {Start = MinStart; End = MaxEnd;}

        *Memory = (ubyte*)malloc((End-Start)*sizeof(char));
        ApplMemory = *Memory;            /* merken, falls Appl free vergißt */

        memset((void*)*Memory,ubInit,(End-Start)*sizeof(char));
        *ulwStart = Start;
        *ulwSize = End-Start;

        for (int j = 0; j < TempInfo.Count; j++)
        {
            if ( bCheckValidBlock(TempInfo.Header[j].Typ) )
            {
               if (fseek(f_DFile,TempInfo.Header[j].Pos,SEEK_SET) == 0)
                  fread((void*)((*Memory)+(TempInfo.Header[j].Start-Start)),TempInfo.Header[j].Length,1,f_DFile);
            }
        }
     }
  } else RetVal = FLASH_PARM_ERROR;
  fclose(f_DFile);
  LogFileWrite("Exit Flash_ulwGetDFDeviceContent");
  return (RetVal);
}

/*************************************************************************@FA*
 * @Function          : Flash_ulwFreeMemory
 *----------------------------------------------------------------------------
 * @Description       : This function releases the previous accesses memory
 *----------------------------------------------------------------------------
 * @Returnvalue       : ulword Result
 *----------------------------------------------------------------------------
 * @Parameters        : **Memory with pointer to data block
 *----------------------------------------------------------------------------
 * @Functioncalls     :
 *----------------------------------------------------------------------------
 * @Global_Vars_Used  :
 *----------------------------------------------------------------------------
 * @Copyright         : (c) 2005 Robert Bosch GmbH, Hildesheim
 * @Author            : CM-CR / ESD4  Peter Tuschik
 *----------------------------------------------------------------------------
 *
 * @History           : Initial Version
 ************************************************************************@FE*/
DLLEXPORT ulword __stdcall Flash_ulwFreeMemory (ubyte **Memory)
{
 if (*Memory) free(*Memory);
 *Memory = ApplMemory = NULL;
 return FLASH_SUCCESS;
}

/*************************************************************************@FA*
 * @Function          : Flash_ulwGetDFContents
 *----------------------------------------------------------------------------
 * @Description       : This function reads the content of the DNL-File and
 *                      the headerinfo as a comma separated string
 *----------------------------------------------------------------------------
 * @Returnvalue       : ulword indicating Success/Error
 *----------------------------------------------------------------------------
 * @Parameters        : ubyte *pubFile: Name of File
 *                      ubyte *pubText: Ausgabestring
 *----------------------------------------------------------------------------
 * @Functioncalls     : Flash_ulwDownload
 *----------------------------------------------------------------------------
 * @Global_Vars_Used  :
 *----------------------------------------------------------------------------
 * @Copyright         : (c) 2001 Robert Bosch GmbH, Hildesheim
 * @Author            : CM-CR / ESD4  Peter Tuschik
 *----------------------------------------------------------------------------
 *
 * @History           : Initial Version
 ************************************************************************@FE*/
DLLEXPORT ulword __stdcall Flash_ulwGetDFContents (ubyte *pubFile, ubyte **pubText)
{
  Flash_strTYInfo Info;                            /* array for header data */
  unsigned char *ptr = NULL;
  ulword retval;

  LogFileWrite("Entered Flash_ulwGetDFContents");

  FILE *InStream = fopen((const char*)pubFile,"rb");

  if (!InStream) return FLASH_DF_FILE_IO_ERROR;  /* IO-Open error detetcted */

  if ((retval = DFInfo_ulwInit(InStream,&Info)) != FLASH_SUCCESS)
  {  fclose(InStream);
     LogFileWrite("Exit Flash_ulwGetDFContents");
     return retval;                    /* return error, if wrong dataformat */
  }
  if (DFInfo_blReadData(InStream,&Info,&ptr,0x8000))
  {  char help[64];                             /* first read Version - data */
     if (DFInfoBuffer != NULL) delete [] DFInfoBuffer;
     if ((DFInfoBuffer = new unsigned char [(11*2) + Info.Count*32 + 1 + 31]) == NULL)
     {  LogFileWrite("ERROR: No systemspace to alloc for DFInfo-Contentbuffer");
        fclose(InStream);
        if (ptr != NULL) delete [] ptr;
        return FLASH_SYSTEM_ERROR;
     }

     *DFInfoBuffer = 0;                                /* default: Info = "" */

     for (int j = 0; j < 11;j++)            /* Länge ist fix! Keine Dev-Info */
     {   sprintf(help,"%02X",*(ptr+j));         /* als Versionsdaten liefern */
         strcat((char*)DFInfoBuffer,help);
     }
     LogFileWrite("Versiondata: %s",DFInfoBuffer);

     for (int i=0; i< Info.Count; i++)           /* then read all blocktypes */
     {
     sprintf(help,",%04X,%08X,%08X,%08X", Info.Header[i].Typ,
                                             (unsigned int)Info.Header[i].Start,
                                             (unsigned int)Info.Header[i].Length,
                                             (unsigned int)Info.Header[i].CS);
         strcat((char*)DFInfoBuffer,help);
     }
     sprintf(help,",");
     for (int i=0;i<10;i++)
     {   strncat(help,(const char*)(aubComNr+i),1);
     }
     for (int i=10;i<15;i++)
     {   char H[5];
         sprintf(H,"%02x",aubComNr[i]);
         strcat(help,H);
     }
     strcat((char*)DFInfoBuffer,help);

     retval = FLASH_SUCCESS;
  } else retval = FLASH_DF_FILE_CONTENT_ERROR;
  fclose(InStream);
  if (ptr) delete [] ptr;                           /* free allocated memory */
  *pubText = DFInfoBuffer;
  LogFileWrite("Exit Flash_ulwGetDFContents");
  return retval;
}


/*************************************************************************@FA*
 * @Function          : Flash_ulwGetDFParms
 *----------------------------------------------------------------------------
 * @Description       : This function reads the Device info block from the
 *                      Software information header.
 *----------------------------------------------------------------------------
 * @Returnvalue       : ulword indicating Success if available or Error if no
 *                      data
 *----------------------------------------------------------------------------
 * @Parameters        : ubyte *pubFile,
 *                      Flash_TYParms * pstFlashParms
 *----------------------------------------------------------------------------
 * @Functioncalls     : Flash_ulwDownload
 *----------------------------------------------------------------------------
 * @Global_Vars_Used  :
 *----------------------------------------------------------------------------
 * @Copyright         : (c) 2003 Robert Bosch GmbH, Hildesheim
 * @Author            : CM-CR / ESD4  Peter Tuschik
 *----------------------------------------------------------------------------
 *
 * @History           : Initial Version
 ************************************************************************@FE*/
DLLEXPORT ulword __stdcall Flash_ulwGetDFParms (ubyte *pubFile, Flash_TYDevParms * pstDevParms)
{
  Flash_strTYInfo Info;                            /* array for header data */
  ulword retval = FLASH_NO_SUCCESS;

  LogFileWrite("Entered Flash_ulwGetDFParms");

  FILE *InStream = fopen((const char*)pubFile,"rb");

  if (InStream)
  {                                         /* Erst mal Headerdaten einlesen */
    if ((retval = DFInfo_ulwInit(InStream,&Info)) == FLASH_SUCCESS)
    {                     /* Blockdaten für Software Information Block lesen */
      retval = DFInfo_blReadDevParms(InStream,&Info,pstDevParms);
    }
    fclose(InStream);
  } else retval = FLASH_DF_FILE_IO_ERROR;         /* IO-Open error detetcted */

  if ((retval != FLASH_SUCCESS) && (retval != (ulword)FLASH_NO_SUCCESS))
     LogFileWrite("ERROR: Wrong data in read the device parameters from DNL-File");

  LogFileWrite("Exit Flash_ulwGetDFParms");
  return retval;
}

/*************************************************************************@FA*
 * @Function          : Flash_ulwSetDFParms
 *----------------------------------------------------------------------------
 * @Description       : With this function new DevParms can be stored
 *----------------------------------------------------------------------------
 * @Returnvalue       : ulword 0: okay, else not okay
 *----------------------------------------------------------------------------
 * @Parameters        : Flash_TYDevParms * pstDevParms
 *----------------------------------------------------------------------------
 * @Functioncalls     : memcpy
 *----------------------------------------------------------------------------
 * @Global_Vars_Used  : None
 *----------------------------------------------------------------------------
 * @Copyright         : (c) 2001 Robert Bosch GmbH, Hildesheim
 * @Author            : CM-CR / ESD4  Peter Tuschik
 *----------------------------------------------------------------------------
 *
 * @History           : Initial version
 *
 ************************************************************************@FE*/
DLLEXPORT ulword __stdcall  Flash_ulwSetDFParms (Flash_TYDevParms * pstDevParms)
{
  if (blFlashActive)                    /* not valid during active transfer */
     return FLASH_ACCESS_DENIED;
                                        /* else copy parameter to structure */
  LogFileWrite("Entered Flash_ulwSetDFParms");
  memcpy((void*)&DevParms,(void*)pstDevParms,sizeof(DevParms));
  SysClock = pstDevParms->ulwSysClock / 1000000;

  LogFileWrite("New values are: %d, %d, %d, %s, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d",
           pstDevParms->ubToggleCounts,pstDevParms->ulwRomSize,pstDevParms->ulwSysClock,
           pstDevParms->uDevSign.Parts.aubSignature,pstDevParms->ulwPM_F0,pstDevParms->ulwPM_F1,
           pstDevParms->ulwPM_F2,pstDevParms->ulwPM_F3,pstDevParms->ulwPM_F4,
           pstDevParms->ulwPM_F5,pstDevParms->ulwPM_F6,pstDevParms->ulwPM_F7,
           pstDevParms->ulwPM_F8,pstDevParms->ulwPM_F9,pstDevParms->ulwPM_F10,
           pstDevParms->ulwPM_F11,pstDevParms->ulwPM_01,pstDevParms->ulwPM_08,
           pstDevParms->ulwBlindBuf,pstDevParms->ulwPM_14,pstDevParms->ulwPM_18,
           pstDevParms->ulwMAUS_Buf,pstDevParms->ulwPM_M4,pstDevParms->ulwPM_M8);

  if (DevParms.ulwBlindBuf > (ulword)FlashParms.ubTelegramLength)
     DevParms.ulwBlindBuf = SetBlindBuf(FlashParms.ubTelegramLength);

  LogFileWrite("Exit Flash_ulwSetDFParms");
  return FLASH_SUCCESS;
}


/*************************************************************************@FA*
 * @Function          : Flash_ulwWriteMemory
 *----------------------------------------------------------------------------
 * @Description       : This function download the data from pubSrc to the
 *                      defined device and address
 *----------------------------------------------------------------------------
 * @Returnvalue       : ubyte indicating Success/Error
 *----------------------------------------------------------------------------
 * @Parameters        : ubMemtype: Device to write to,
 *                      pubSrc:    pointer to Data to write into device
 *                      ulwDest:   destination address in device
 *                      ulwLength: length of data to write into device
 *----------------------------------------------------------------------------
 * @Functioncalls     : Flash_ulwWrite
 *----------------------------------------------------------------------------
 * @Global_Vars_Used  :
 *----------------------------------------------------------------------------
 * @Copyright         : (c) 2001 Robert Bosch GmbH, Hildesheim
 * @Author            : CM-CR / ESD4  Peter Tuschik
 *----------------------------------------------------------------------------
 *
 * @History           : Initial Version
 ************************************************************************@FE*/
DLLEXPORT ulword __stdcall Flash_ulwWriteMemory (ubyte ubMemType,
                           ubyte *pubSrc,ulword ulwDest,ulword ulwLength)
{
  if (blFlashActive)
     return (FLASH_ACCESS_DENIED);          // ignore if dll = active

  blFlashActive = true;                     // now dll = active
  ProgressUpdate("Start programming Type: %X into Memory",ubMemType);
  ulword RetVal = Flash_ulwWrite(ubMemType,pubSrc,ulwDest,ulwLength);
  blFlashActive = false;                    // dll is inactive
  blAbort = false;
  return RetVal;
}


/*************************************************************************@FA*
 * @Function          : Flash_ulwReadMemory
 *----------------------------------------------------------------------------
 * @Description       : This function read the data from a device to the
 *                      defined device and address
 *----------------------------------------------------------------------------
 * @Returnvalue       : ubyte indicating Success/Error
 *----------------------------------------------------------------------------
 * @Parameters        : ubMemtype: Device to write to,
 *                      pubDest:   pointer to Data to read from device
 *                      ulwSrc:   destination address to store the data
 *                      ulwLength: length of data to read from device
 *----------------------------------------------------------------------------
 * @Functioncalls     : Flash_uiRead
 *----------------------------------------------------------------------------
 * @Global_Vars_Used  :
 *----------------------------------------------------------------------------
 * @Copyright         : (c) 2001 Robert Bosch GmbH, Hildesheim
 * @Author            : CM-CR / ESD4  Peter Tuschik
 *----------------------------------------------------------------------------
 *
 * @History           : Initial Version
 ************************************************************************@FE*/
DLLEXPORT ulword __stdcall Flash_ulwReadMemory (ubyte ubMemType,
                           ubyte *pubDest,ulword ulwSrc,ulword ulwLength)
{
  if (blFlashActive)
     return (FLASH_ACCESS_DENIED);          // ignore if dll = active

  blFlashActive = true;                     // now dll = active
  ulword RetVal = FLASH_SUCCESS;
  ProgressUpdate("Start reading Type: %X from Memory at address 0x%x for 0x%x bytes",ubMemType, ulwSrc, ulwLength);

  unsigned char prozent = 0;
  ulword max = ulwLength;

  ProgressUpdate("Reading: 0%%");
  while (ulwLength && (RetVal == FLASH_SUCCESS))
  { ubyte len = (DevParms.ulwBlindBuf > 240) ? 240 : (ubyte)DevParms.ulwBlindBuf;
    if ((ulwLength < (ulword)len)) len = (ubyte)ulwLength;
    RetVal = Flash_uiRead(ubMemType, ulwSrc, len, pubDest);
    if (RetVal == FLASH_SUCCESS)
    {  pubDest += len;
       ulwSrc += len;
       ulwLength -= len;
       unsigned char proz = 100-(unsigned char)((ulwLength*100/max));
       if (prozent != proz)
          ProgressRefresh("Reading:  %d%%",(prozent = proz));
    }
    else LogFileWrite("ERROR: cannot read from device");
    if (blAbort) RetVal = FLASH_ABORTED;
  }
  blFlashActive = false;                    // dll is inactive
  blAbort = false;
  return RetVal;
}


/*************************************************************************@FA*
 * @Function          : Flash_ulwEraseMemory
 *----------------------------------------------------------------------------
 * @Description       : This function erase the data from pubSrc to the
 *                      defined device and address
 *----------------------------------------------------------------------------
 * @Returnvalue       : ubyte indicating Success/Error
 *----------------------------------------------------------------------------
 * @Parameters        : ubMemtype: Device to write to,
 *                      pubSrc:    pointer to Data to write into device
 *                      ulwDest:   destination address in device
 *                      ulwLength: length of data to write into device
 *----------------------------------------------------------------------------
 * @Functioncalls     : Flash_ulwDownload
 *----------------------------------------------------------------------------
 * @Global_Vars_Used  :
 *----------------------------------------------------------------------------
 * @Copyright         : (c) 2001 Robert Bosch GmbH, Hildesheim
 * @Author            : CM-CR / ESD4  Peter Tuschik
 *----------------------------------------------------------------------------
 *
 * @History           : Initial Version
 ************************************************************************@FE*/
DLLEXPORT ulword __stdcall Flash_ulwEraseMemory (ubyte ubMemType, ulword ulwStartAdr,
                                                 ulword ulwLength)
{
  if (blFlashActive)
     return (FLASH_ACCESS_DENIED);          // ignore if dll = active

  blFlashActive = true;                     // now dll = active
  if ((ulwStartAdr+ulwLength) == 0)
     ProgressUpdate("Start erasing Type: %02X",ubMemType);
  else ProgressUpdate("Start block erase Type: %02X from 0x%08x to 0x%08x",ubMemType,ulwStartAdr,ulwStartAdr+ulwLength-1);
  ulword RetVal = Flash_uiErase(ubMemType,ulwStartAdr,ulwLength,0xffffffffffffffff);
  blFlashActive = false;                    // dll is inactive
  blAbort = false;
  return RetVal;
}

/*************************************************************************@FA*
 * @Function          : Flash_ulwBlankCheck
 *----------------------------------------------------------------------------
 * @Description       : This function checks the device for blank state of
 *                      memory
 *----------------------------------------------------------------------------
 * @Returnvalue       : ubyte indicating Success/Error
 *----------------------------------------------------------------------------
 * @Parameters        : ubMemtype: Device to write to,
 *                      pubSrc:    pointer to Data to write into device
 *                      ulwDest:   destination address in device
 *                      ulwLength: length of data to write into device
 *----------------------------------------------------------------------------
 * @Functioncalls     : Flash_uiGetBlank
 *----------------------------------------------------------------------------
 * @Global_Vars_Used  :
 *----------------------------------------------------------------------------
 * @Copyright         : (c) 2001 Robert Bosch GmbH, Hildesheim
 * @Author            : CM-CR / ESD4  Peter Tuschik
 *----------------------------------------------------------------------------
 *
 * @History           : Initial Version
 ************************************************************************@FE*/
DLLEXPORT ulword __stdcall Flash_ulwBlankCheck (ubyte ubMemType)
{
  if (blFlashActive)
     return (FLASH_ACCESS_DENIED);          // ignore if dll = active

  blFlashActive = true;                     // now dll = active
  LogFileWrite("Entered Flash_ulwBlankCheck()");
  ProgressUpdate("Start blank checking Type: %02X",ubMemType);
  ulword RetVal = Flash_uiGetBlank(ubMemType);
  blFlashActive = false;                    // dll is inactive
  blAbort = false;
  LogFileWrite("Exit Flash_ulwBlankCheck()");
  return RetVal;
}


/*************************************************************************@FA*
 * @Function          : Flash_ulwGetCRCValue
 *----------------------------------------------------------------------------
 * @Description       : This function read the CRC and return it as parameter
 *----------------------------------------------------------------------------
 * @Returnvalue       : ulword indicating Success/Error
 *----------------------------------------------------------------------------
 * @Parameters        : ubMemtype:   Device to read CRC from,
 *                      ulwStartAdr: start address in device,
 *                      ulwLength:   length of data to check CRC
 *                      ulwCRC:      CRC-returnvalue to calling programme
 *----------------------------------------------------------------------------
 * @Functioncalls     : Flash_uiGetCRC
 *----------------------------------------------------------------------------
 * @Global_Vars_Used  :
 *----------------------------------------------------------------------------
 * @Copyright         : (c) 2001 Robert Bosch GmbH, Hildesheim
 * @Author            : CM-CR / ESD4  Peter Tuschik
 *----------------------------------------------------------------------------
 *
 * @History           : Initial Version
 ************************************************************************@FE*/
DLLEXPORT ulword __stdcall Flash_ulwGetCRCValue (ubyte ubMemType,
                           ulword ulwStartAdr,ulword ulwLength,ulword *CRC)

{
  if (blFlashActive)
     return (FLASH_ACCESS_DENIED);          // ignore if dll = active

  blFlashActive = true;                     // now dll = active
  ProgressUpdate("Start reading CRC from device %02X, Adr: 0x%08X-0x%08X",ubMemType,ulwStartAdr,ulwStartAdr+ulwLength);
  ulword RetVal = Flash_uiGetCRC(ubMemType,ulwStartAdr,ulwLength,(unsigned int*)CRC);
  blFlashActive = false;                    // dll is inactive
  blAbort = false;
  return RetVal;
}
/*************************************************************************@FA*
 * @Function          : Flash_ulwDoVerify
 *----------------------------------------------------------------------------
 * @Description       : This function verifies all the selected blocks, passed
 *                      as parameter puwValidBlocks
 *----------------------------------------------------------------------------
 * @Returnvalue       : ulword indicating Success/Error
 *----------------------------------------------------------------------------
 * @Parameters        : uMode * with list of blocks and uword error block #.
 *----------------------------------------------------------------------------
 * @Functioncalls     : Flash_uiGetCRC()
 *----------------------------------------------------------------------------
 * @Global_Vars_Used  :
 *----------------------------------------------------------------------------
 * @Copyright         : (c) 2005 Robert Bosch GmbH, Hildesheim
 * @Author            : CM-CR / ESD4  Peter Tuschik
 *----------------------------------------------------------------------------
 *
 * @History           : Initial Version
 ************************************************************************@FE*/
DLLEXPORT ulword __stdcall Flash_ulwDoVerify (uword  *puwValidBlocks, uword *uwError)
{ ulword RetVal = FLASH_SUCCESS;

  LogFileWrite("Entered Flash_ulwDoVerify()");

  if (blFlashActive)
     return (FLASH_ACCESS_DENIED);          // ignore if dll = active
                                            // suppress programming erased data ?

  BlockList = puwValidBlocks;               // Liste der gültigen zu Blöcke zuweisen
  blFlashActive = true;                     // now dll = active

  uword Typ,MemType;

  for (int i = 0; i < DFInfo.Count; i++)
  {   bool found = false;
      RetVal = FLASH_SUCCESS;                         // default no error

      Typ = DFInfo.Header[i].Typ;
      MemType = (Typ >> 8);

      switch (MemType)
      { case 0x40:                                    // write int. Flash
        case 0x42:                                    // write ext. par. Flash
        case 0x43:                                    // write secondary Flash
        case 0x44:                                    // write ext. ser. Flash #1
        case 0x54:                                    // write ADR
        case 0x30:                                    // write int. EEP
        case 0x38:                                    // write MemoryCard
        case 0xC4:                                    // write DAB D-Fire 2 (Flash)
        case 0x94:                                    // write DAB D-Fire 2 (RAM)
        case 0xC6:                                    // write DSA (Flash)
        case 0x96:                                    // write DSA (RAM)
        case 0x4C:                                    // write FGS configuration
        case 0x4B:                                    // FGS Parallel Flash
        case 0x4D:                                    // write FGS configuration
        case 0x4E:                                    // write FGS 2nd parallel Flash
        case 0x4F:                                    // Internal Data Flash
        case 0x34:                                    // write FGS serial EEProm
                   found = bCheckValidBlock(Typ);
                   break;
      }

      if (found)
      {  unsigned int CRC;
         *uwError = Typ;                              // default = aktueller Block

         RetVal = Flash_uiGetCRC((ubyte)MemType,DFInfo.Header[i].Start,DFInfo.Header[i].Length,&CRC);
         if (RetVal == FLASH_SUCCESS)
         {  if (CRC != DFInfo.Header[i].CS)
            {  RetVal = FLASH_NO_SUCCESS;
               break;
            }
         }
         else break;

      } /* if (found) */

      if (blAbort)                                    // abort by user
      {  RetVal = FLASH_ABORTED;
         break;
      }
  } /* end for (;;;) */

  blAbort       = false;
  blFlashActive = false;                              // dll = inactive

  LogFileWrite("Exit Flash_ulwDoVerify()");
  return (RetVal);
}


/*************************************************************************@FA*
 * @Function          : Flash_ulwMausSend ()
 *----------------------------------------------------------------------------
 * @Description       : This function is used to xmit a msg via MAUS
 *----------------------------------------------------------------------------
 * @Returnvalue       : ulword indicating Success/Error
 *----------------------------------------------------------------------------
 * @Parameters        : ubyte ubCmd:   Command of msg to xmit,
 *                      ubyte *pubBuf: ptr to databuf with data to xmit,
 *                      ubyte ubLen:   length of data to send
 *----------------------------------------------------------------------------
 * @Functioncalls     : MausSend()
 *----------------------------------------------------------------------------
 * @Global_Vars_Used  :
 *----------------------------------------------------------------------------
 * @Copyright         : (c) 2001 Robert Bosch GmbH, Hildesheim
 * @Author            : CM-CR / ESD4  Peter Tuschik
 *----------------------------------------------------------------------------
 *
 * @History           : Initial Version
 ************************************************************************@FE*/
DLLEXPORT ulword __stdcall Flash_ulwMausSend(ubyte ubCmd, ubyte *pubBuf, ubyte ubLen)

{
  if (blFlashActive)
     return (FLASH_ACCESS_DENIED);          // ignore if dll = active

  ProgressUpdate("Start Sendig data via Maus");
  if (pFlashPort->hID && (MausSend(pFlashPort, ubCmd, pubBuf, ubLen) == true))
     return FLASH_SUCCESS;
  else return FLASH_NO_SUCCESS;
}


/*************************************************************************@FA*
 * @Function          : Flash_ulwMausReceive ()
 *----------------------------------------------------------------------------
 * @Description       : This function is used to xmit a msg via MAUS
 *----------------------------------------------------------------------------
 * @Returnvalue       : ulword indicating Success/Error
 *----------------------------------------------------------------------------
 * @Parameters        : ubyte ubCmd:   Command of msg to xmit,
 *                      ubyte *pubBuf: ptr to databuf with data to xmit,
 *                      ubyte ubLen:   length of data to send
 *----------------------------------------------------------------------------
 * @Functioncalls     : MausReceive()
 *----------------------------------------------------------------------------
 * @Global_Vars_Used  :
 *----------------------------------------------------------------------------
 * @Copyright         : (c) 2001 Robert Bosch GmbH, Hildesheim
 * @Author            : CM-CR / ESD4  Peter Tuschik
 *----------------------------------------------------------------------------
 *
 * @History           : Initial Version
 ************************************************************************@FE*/
DLLEXPORT ulword __stdcall Flash_ulwMausReceive(ubyte ubCmd, ubyte *pubRcvBuf,
                                                ubyte *pubLength, ulword ulwRetrys)
{
  if (blFlashActive)
     return (FLASH_ACCESS_DENIED);          // ignore if dll = active

  ProgressUpdate("Start Receiving data via Maus");

  if (pFlashPort->hID && (MausReceive (pFlashPort, ubCmd, pubRcvBuf, pubLength, ulwRetrys, kDebug|kDelay|EchoCom) == true))
     return FLASH_SUCCESS;
  else return FLASH_NO_SUCCESS;
}


/*************************************************************************@FA*
 * @Function          : Flash_ulwMausChangeParms  ()
 *----------------------------------------------------------------------------
 * @Description       : This function is used to change the Port configuration
 *----------------------------------------------------------------------------
 * @Returnvalue       : ulword indicating Success/Error
 *----------------------------------------------------------------------------
 * @Parameters        : ubyte * pubCom:   Initstring for Com-Port
 *----------------------------------------------------------------------------
 * @Functioncalls     : MausSend(), MausReceive(), PortSetup()
 *----------------------------------------------------------------------------
 * @Global_Vars_Used  :
 *----------------------------------------------------------------------------
 * @Copyright         : (c) 2001 Robert Bosch GmbH, Hildesheim
 * @Author            : CM-CR / ESD4  Peter Tuschik
 *----------------------------------------------------------------------------
 *
 * @History           : Initial Version
 ************************************************************************@FE*/

DLLEXPORT ulword __stdcall Flash_ulwMausChangeParms(ubyte *pubCom)
{ RS232Typ *pPort = NULL;

  if (blFlashActive)
  {  blAbort = true;
     return (FLASH_ACCESS_DENIED);          // ignore if dll = active
  }
  else blAbort = false;

  blFlashActive = true;
  LogFileWrite("Entered Flash_ulwMausChangeParms()");

  ComSettings((char*)pubCom,&pPort);        /* get Com-Settings from string */

  int BaudIdx = ChkValidBaudrate(pPort->ulBaudRate);

  if ((BaudIdx < 0) || (BaudIdx >= 10))
  {  LogFileWrite("ERROR: Baudrate is not supported by MAUS.");
     blFlashActive = false;
     return (FLASH_BAUDRATE_SET_FAILED);
  }

  for (int i = 0; i < FlashParms.ubTelegramRetries; i++)
  {   ProgressUpdate("Baudrate change to %d Baud, Attempt: %d",pPort->ulBaudRate,i+1);
      if (MausSend(pFlashPort, 0x04, (ubyte*)&BaudIdx, 1))
      {  ucBuffer[0] = 0;
         unsigned char ucLength = 255;
         if (MausReceive(pFlashPort, 0x05, ucBuffer, &ucLength, 10, kDebug|EchoCom) == true)
            if ((ucBuffer[1] == 0x01) && (ucBuffer[2]== 0x55))
            {  LogFileWrite("Maus Baudrate successfully changed in Device");

               pFlashPort->ulBaudRate = pPort->ulBaudRate;
               pFlashPort->ucBit = pPort->ucBit;
               pFlashPort->ucStop = pPort->ucStop;
               pFlashPort->ucBitCount = pPort->ucBitCount;
               pFlashPort->ucParity = pPort->ucParity;
               strcpy(pFlashPort->pcName,pPort->pcName);

               if (!PortSetup(pFlashPort))
               {  LogFileWrite("ERROR: Cannot set Baudrate on UART");
                  blFlashActive = false;                  /* dll = inactive */
                  return (FLASH_BAUDRATE_SET_FAILED);
               }
               LogFileWrite("Exit Flash_ulwMausChangeParms() with success");
               blFlashActive = false;
               return FLASH_SUCCESS;   // device is successfully erased
            }
      }
      if (blAbort) break;                                  /* evtl. Abbruch */
  }
                                            // common err in Maus communication
  if (blAbort)
     ProgressUpdate("Abort Changing Mausparams");
  else ProgressUpdate("No response for change Maus BaudrateLevel");
  blFlashActive = false;
  return (FLASH_BAUDRATE_SET_FAILED);
}
//---------------------------------------------------------------------------

DLLEXPORT ulword __stdcall FLASH_ulwGetRSASignature(uword Block,ubyte **RetBuf)
{ ulword RetVal = FLASH_NO_SUCCESS;
  *RetBuf = NULL;

  LogFileWrite("Entered FLASH_ulwGetRSASignature()");

  if (RSA_Offset)
  {  if (IntParms.f_DFile)
     {  unsigned char *ptr = NULL;
        unsigned char *ptrRSA;
        int len = DFInfo_blReadData(IntParms.f_DFile,&DFInfo,&ptr, 0x8000);
        if (len)
        { ptrRSA = ptr + RSA_Offset + 1 + 2;
          unsigned long size = 2;

          uword anz = (*ptrRSA << 8) + *(ptrRSA+1);
          for (int i=0;(i<anz)&&(RetVal!=FLASH_SUCCESS);i++)
          {   uword B = (*(ptrRSA+size) << 8) + *(ptrRSA+size+1);
              ulword S = (*(ptrRSA+size+2) << 8) + *(ptrRSA+size+3);
              if (B == Block)
              {  *RetBuf = (ubyte*)malloc(S * sizeof(unsigned char));
                 memcpy((void*)*RetBuf,(void*)(ptrRSA+size+4),S);
                 LogFileWrite("RSA-Signature key copied to Return buffer");
                 RetVal = FLASH_SUCCESS;
              }
              else size = size + S + 4;
          }
        }
        else LogFileWrite("Error on reading Blok 0x8000 from DNL-file");
     }
     else LogFileWrite("Error: Not connected of no DNL-file available");
  } /* if (RSA_Offset) */
  else LogFileWrite("Error: No RDA-Signature data in DNL-file");

  LogFileWrite("Exit FLASH_ulwGetRSASignature()");
  return RetVal;
}
//---------------------------------------------------------------------------

DLLEXPORT char*  __stdcall FLASH_pcGetOptLogInfo(void)
{
  return (char*)OptLog;
}

/*****************************************************************************
 *  Maus-Routines for Flash-Processing                                       *
 *****************************************************************************/

unsigned int Flash_uiWriteRandom(unsigned char ucMemType,__int64 uint64RandomNo)
{
  ProgressUpdate("Write RandomNr: 0x%08X%08X into serial Flash",(unsigned int)(uint64RandomNo>>32),(unsigned int)uint64RandomNo);
  ucBuffer[0] = ucMemType;
  ucBuffer[1] = 1;
  ucBuffer[2] = (unsigned char)(uint64RandomNo >> 56);
  ucBuffer[3] = (unsigned char)(uint64RandomNo >> 48);
  ucBuffer[4] = (unsigned char)(uint64RandomNo >> 40);
  ucBuffer[5] = (unsigned char)(uint64RandomNo >> 32);
  ucBuffer[6] = (unsigned char)(uint64RandomNo >> 24);
  ucBuffer[7] = (unsigned char)(uint64RandomNo >> 16);
  ucBuffer[8] = (unsigned char)(uint64RandomNo >> 8);
  ucBuffer[9] = (unsigned char)(uint64RandomNo);

  if (ChkMausSend(pFlashPort, 0x8F, ucBuffer, 13) == true)
  {  unsigned char ucLength = 255;

     ucBuffer[0] = 0;
     if (MausReceive (pFlashPort, 0x71, ucBuffer, &ucLength, DevParms.ulwPM_F9, kDebug|EchoCom) == true)
     {  if ((ucBuffer[1] == 0x01) && (ucBuffer[2]== 0x55))
        {  ProgressUpdate("Randon Nr is written");
           return FLASH_SUCCESS;            // random Nr is written
        }
                                            // command in progress ?
        if ((ucBuffer[1] == 0x01) && (ucBuffer[2] == 0x00))
        {  if (WaitChkMausSend (pFlashPort, 0x8F, (unsigned char*)"\x00", 1) == true)
           {  ucBuffer[0] = 0;
              ucLength = 255;
              unsigned long ulTries = 1 + FlashParms.ubWriteTimeOut * 1000 / FlashParms.uwTelegramTimeOut;
              if (MausReceive (pFlashPort, 0x71, ucBuffer, &ucLength, ulTries, kDebug|EchoCom) == true)
                 if (ucBuffer[1] == 0x01)
                 {  if (ucBuffer[2]== 0x55)
                    {  ProgressUpdate("Randon Nr is written");
                       return FLASH_SUCCESS;   // random is written
                    }
                    if (ucBuffer[2]== 0xAA)
                    {  ProgressUpdate("Randon Nr is not written");
                       return FLASH_NO_SUCCESS; // random Nr cannot be written
                    }
                 }
           }
        }
     }
  } else ucBuffer[1] = 0;                   // destroy buffer for global error
  return Flash_ulwMausResult(ucMemType,0,8);
}



//-- Check for Blank Device ----------------------------------------------------

unsigned int Flash_uiGetBlank( unsigned char ucMemType)
{
  ucBuffer[0] = ucMemType;
  ucBuffer[1] = 0;


  if (ChkMausSend(pFlashPort, 0x8F, ucBuffer, 2) == true)
  {  unsigned char ucLength = 255;

     ucBuffer[0] = 0;
     if (MausReceive (pFlashPort, 0x71, ucBuffer, &ucLength, DevParms.ulwPM_F9, kDebug|EchoCom) == true)
     {  if ((ucBuffer[1] == 0x01) && (ucBuffer[2]== 0x55))
        {  ProgressUpdate("Device is blank");
           return FLASH_SUCCESS;            // device is already erased
        }
                                            // command in progress ?
        if ((ucBuffer[1] == 0x01) && (ucBuffer[2] == 0x00))
        {  if (WaitChkMausSend (pFlashPort, 0x8F, (unsigned char*)"\x00", 1) == true)
           {  ucBuffer[0] = 0;
              ucLength = 255;
              unsigned long ulTries = 1 + FlashParms.ubEraseTimeOut * 1000 / FlashParms.uwTelegramTimeOut;
              if (MausReceive (pFlashPort, 0x71, ucBuffer, &ucLength, ulTries, kDebug|EchoCom) == true)
                 if (ucBuffer[1] == 0x01)
                 {  if (ucBuffer[2]== 0x55)
                    {  ProgressUpdate("Device is blank");
                       return FLASH_SUCCESS;   // device is blank
                    }
                    if (ucBuffer[2]== 0xAA)
                    {  LogFileWrite("Device is not Blank");
                       return FLASH_DEVICE_NOT_BLANK;   // device is non blank
                    }
                 }
           }
        }
     }
  } else ucBuffer[1] = 0;                   // destroy buffer for global error
  return Flash_ulwMausResult(ucMemType,0,0);
}


//-- Erase Flash ----------------------------------------------------

unsigned int Flash_uiErase( unsigned char ucMemType,
                            unsigned int  uiStart,
                            unsigned int  uiLength,
                            __int64 uint64RandomNo)
{ unsigned char ucCmdLen=9;
  unsigned int uiSt = uiStart, uiLen = uiLength;
  unsigned long ulSize;
  SYSTEMTIME t0;

  GetSystemTime(&t0);

  if ((ucMemType == 0x44) && (FlashParms.ubWriteRandomEnable == 1))
  {  ProgressUpdate("Erase but Write RandomNr: 0x%08X%08X into serial Flash",(unsigned int)(uint64RandomNo>>32),(unsigned int)uint64RandomNo);
     ucBuffer[9]  = (unsigned char)(uint64RandomNo >> 56);
     ucBuffer[10] = (unsigned char)(uint64RandomNo >> 48);
     ucBuffer[11] = (unsigned char)(uint64RandomNo >> 40);
     ucBuffer[12] = (unsigned char)(uint64RandomNo >> 32);
     ucBuffer[13] = (unsigned char)(uint64RandomNo >> 24);
     ucBuffer[14] = (unsigned char)(uint64RandomNo >> 16);
     ucBuffer[15] = (unsigned char)(uint64RandomNo >> 8);
     ucBuffer[16] = (unsigned char)(uint64RandomNo);
     ucCmdLen =17;
     uiSt = uiLen = 0;                      // for random Nr -> erase all
  }

  ucBuffer[0] = ucMemType;
  ucBuffer[1] = (unsigned char)(uiSt >> 24);
  ucBuffer[2] = (unsigned char)(uiSt >> 16);
  ucBuffer[3] = (unsigned char)(uiSt >> 8);
  ucBuffer[4] = (unsigned char)(uiSt);

  ucBuffer[5] = (unsigned char)(uiLen >> 24);
  ucBuffer[6] = (unsigned char)(uiLen >> 16);
  ucBuffer[7] = (unsigned char)(uiLen >> 8);
  ucBuffer[8] = (unsigned char)(uiLen);

  if ((ulSize = uiLen) == 0)                // Bulk-Erase ?
     ulSize = ulwGetDeviceSize(ucMemType);  // dann Gesamtgröße holen

  if (ChkMausSend(pFlashPort, 0x86, ucBuffer, ucCmdLen) == true)
  {  unsigned char ucLength = 255;

     ucBuffer[0] = 0;
     if (MausReceive (pFlashPort, 0x71, ucBuffer, &ucLength, DevParms.ulwPM_F9, kGuard|kDebug|EchoCom) == true)
     {  if ((ucBuffer[1] == 0x01) && (ucBuffer[2]== 0x55))
        {  ProgressUpdate("Device was already erased");
           return FLASH_SUCCESS;            // device is already erased
        }
                                            // command in progress ?
        if ((ucBuffer[1] == 0x01) && (ucBuffer[2] == 0x00))
        {  if (WaitChkMausSend (pFlashPort, 0x86, (unsigned char*)"\x00", 1) == true)
           {  ucBuffer[0] = 0;
              ucLength = 255;
                                            // Vielfaches von 1 MB berechnen
              ulSize /= 1000000;            // um Erase-Timeout pro 1 MB zu
              if (ulSize<1.0) ulSize = 1;   // berücksichtigen

              unsigned long ulTries = 1 + (((FlashParms.ubEraseTimeOut * 1000) / FlashParms.uwTelegramTimeOut) * ulSize);
              if (MausReceive (pFlashPort, 0x71, ucBuffer, &ucLength, ulTries, kGuard|kDebug|EchoCom) == true)
                 if ((ucBuffer[1] == 0x01) && (ucBuffer[2]== 0x55))
                 {  ProgressUpdate("Device successful erased in %s",GetTimeDifference(t0));
                    return FLASH_SUCCESS;   // device is successfully erased
                 }
           }
        }
     }
  } else ucBuffer[1] = 0;                   // destroy buffer for global error
  return Flash_ulwMausResult(ucMemType,uiStart,uiLength);
}


//-- Get CRC Flash ----------------------------------------------------
unsigned int Flash_uiGetCRC (unsigned char ucMemType,
                             unsigned int  uiStartAddress,
                             unsigned int  uiLength,
                             unsigned int  *uiValue)
{
  unsigned long ulSize;
  ucBuffer[0] = ucMemType;
  ucBuffer[1] = (unsigned char)(uiStartAddress >> 24);
  ucBuffer[2] = (unsigned char)(uiStartAddress >> 16);
  ucBuffer[3] = (unsigned char)(uiStartAddress >> 8);
  ucBuffer[4] = (unsigned char)(uiStartAddress);

  ucBuffer[5] = (unsigned char)(uiLength >> 24);
  ucBuffer[6] = (unsigned char)(uiLength >> 16);
  ucBuffer[7] = (unsigned char)(uiLength >> 8);
  ucBuffer[8] = (unsigned char)(uiLength);

  if (ChkMausSend(pFlashPort, 0x8C, ucBuffer, 0x09) == true)
  {  unsigned char ucLength = 255;
     unsigned long ulTries;

     ucBuffer[0] = 0;
     if (MausReceive (pFlashPort, 0x8D, ucBuffer, &ucLength, DevParms.ulwPM_F9, kDebug|EchoCom) == true)
     {  if ((ucBuffer[1] == 0x05) && (ucBuffer[2]== 0x55))
           goto CRC_ok;
                                            // command in progress ?
        if ((ucBuffer[1] == 0x01) && (ucBuffer[2]== 0x00))
        {  if (WaitChkMausSend (pFlashPort, 0x8C, (unsigned char*)"\x00", 1) == true)
           {  ucBuffer[0] = 0;
              ucLength = 255;
                                               // Vielfaches von 1 MB berechnen
              ulSize = uiLength/1000000;       // um CS-Timeout pro 1 MB zu
              if (ulSize<1.0) ulSize = 1;      // berücksichtigen

              ulTries = 1 + (((FlashParms.ubEraseTimeOut * 1000) / FlashParms.uwTelegramTimeOut) * ulSize);
              if (MausReceive (pFlashPort, 0x8D, ucBuffer, &ucLength, ulTries, kDebug|EchoCom) == true)
                 if ((ucBuffer[1] == 0x05) && (ucBuffer[2]== 0x55))
                 {  CRC_ok:
                    *uiValue  = (unsigned int)(ucBuffer[3] <<24);
                    *uiValue |= (unsigned int)(ucBuffer[4] <<16);
                    *uiValue |= (unsigned int)(ucBuffer[5] <<8);
                    *uiValue |= (unsigned int)(ucBuffer[6]);

                    ProgressUpdate("Device %02X (0x%08x-0x%08x) CRC is %08X",ucMemType,uiStartAddress,uiStartAddress+uiLength,*uiValue);
                    return FLASH_SUCCESS;   // Verify okay, see CRC
                 }
           }
        }
     }
  } else ucBuffer[1] = 0;                   // destroy buffer for global error
  return Flash_ulwMausResult(ucMemType,uiStartAddress,uiLength);
}

//-- Fill Internal Flash ----------------------------------------------------
unsigned int Flash_uiFill (unsigned char ucMemType,
                           unsigned int  uiStartAddress,
                           unsigned int  uiLength,
                           unsigned char ucValue)
{
  ProgressUpdate("Start fill device %02X",ucMemType);

  ucBuffer[0] = ucMemType;
  ucBuffer[1] = (unsigned char)(uiStartAddress >> 24);
  ucBuffer[2] = (unsigned char)(uiStartAddress >> 16);
  ucBuffer[3] = (unsigned char)(uiStartAddress >> 8);
  ucBuffer[4] = (unsigned char)(uiStartAddress);

  ucBuffer[5] = (unsigned char)(uiLength >> 24);
  ucBuffer[6] = (unsigned char)(uiLength >> 16);
  ucBuffer[7] = (unsigned char)(uiLength >> 8);
  ucBuffer[8] = (unsigned char)(uiLength);

  ucBuffer[9] = ucValue;

  if (ChkMausSend (pFlashPort, 0x89, ucBuffer, 10) == true)
  {  unsigned char ucLength = 255;

     ucBuffer[0] = 0;
     if (MausReceive (pFlashPort, 0x71, ucBuffer, &ucLength, DevParms.ulwPM_F9, kGuard|kDebug|EchoCom) == true)
     {  if ((ucBuffer[1] == 0x01) && (ucBuffer[2]== 0x55))
           return FLASH_SUCCESS;            // Write Data OK
                                            // command in progress ?
        if ((ucBuffer[1] == 0x01) && (ucBuffer[2]== 0x00))
        {  if (WaitChkMausSend (pFlashPort, 0x89, (unsigned char*)"\x00", 1) == true)
           {  ucBuffer[0] = 0;
              ucLength = 255;
                                            // hier write Time Out einbauen !!!
              unsigned long ulTries =1+ FlashParms.ubEraseTimeOut*1000 / FlashParms.uwTelegramTimeOut;
              if (MausReceive (pFlashPort, 0x71, ucBuffer, &ucLength, ulTries, kGuard|kDebug|EchoCom) == true)
                 if ((ucBuffer[1] == 0x01) && (ucBuffer[2]== 0x55))
                    return FLASH_SUCCESS;   // Write Data OK
           }
        }
     }
  } else ucBuffer[1] = 0;                   // destroy buffer for global error
  return Flash_ulwMausResult(ucMemType,uiStartAddress,uiLength);
}

//-- Read Memory ----------------------------------------------------
unsigned int Flash_uiRead (unsigned char ucMemType,
                           unsigned int  uiStartAddress,
                           unsigned char ucLength,
                           unsigned char *ucValue)
{
  ucBuffer[0] = ucMemType;
  ucBuffer[1] = (unsigned char) (uiStartAddress >> 24);
  ucBuffer[2] = (unsigned char) (uiStartAddress >> 16);
  ucBuffer[3] = (unsigned char) (uiStartAddress >> 8);
  ucBuffer[4] = (unsigned char) (uiStartAddress);
  ucBuffer[5] = ucLength;

  if (ChkMausSend (pFlashPort, 0x80, ucBuffer,  6) == true)
  {  unsigned char ucTxLength = 255;

     ucBuffer[0] = 0;
     if (MausReceive (pFlashPort, 0x81,ucBuffer,&ucTxLength, 5, kDebug|EchoCom) == true)
     {  if (ucBuffer[2]== 0x55)
           if (ucValue != NULL)
           {  memcpy ( ucValue, ucBuffer+3, (unsigned int) ucLength);
              return FLASH_SUCCESS;
           }
                                            // command in progress ?
        if ((ucBuffer[1] == 0x01) && (ucBuffer[2]== 0x00))
        {  if (WaitChkMausSend (pFlashPort, 0x80, (unsigned char*)"\x00", 1) == true)
           {  ucBuffer[0] = 0;
              ucTxLength = 255;
              unsigned long ulTries = 1+ FlashParms.ubEraseTimeOut*1000/FlashParms.uwTelegramTimeOut;
              if (MausReceive (pFlashPort, 0x81, ucBuffer, &ucTxLength, ulTries, kGuard|kDebug|EchoCom) == true)
                 if (ucBuffer[2]== 0x55)
                    if (ucValue != NULL)
                    {  memcpy ( ucValue, ucBuffer+3, (unsigned int) ucLength);
                       return FLASH_SUCCESS;
                    }
           }
        }
     }
  } else ucBuffer[1] = 0;                   // destroy buffer for global error
  return Flash_ulwMausResult(ucMemType,uiStartAddress,(unsigned int)ucLength);
}

//-- Write Memory ----------------------------------------------------
unsigned int Flash_uiWrite (unsigned char ucMemType,
                            unsigned int  uiStartAddress,
                            unsigned char ucLength,
                            unsigned char *pucValue)
{ ucBuffer[0] = ucMemType;
  ucBuffer[1] = (unsigned char) (uiStartAddress >> 24);
  ucBuffer[2] = (unsigned char) (uiStartAddress >> 16);
  ucBuffer[3] = (unsigned char) (uiStartAddress >> 8);
  ucBuffer[4] = (unsigned char) (uiStartAddress);
  memcpy (ucBuffer+5, pucValue, (unsigned int) ucLength);

  if (ChkMausSend (pFlashPort, 0x83, ucBuffer,  (5+ucLength)) == true)
  {  unsigned char ucLength = 255;
     ucBuffer[0] = 0;
     if (MausReceive (pFlashPort, 0x71, ucBuffer, &ucLength, DevParms.ulwPM_F9, kGuard|kDebug|EchoCom) == true)
     {  if ((ucBuffer[1] == 0x01) && (ucBuffer[2]== 0x55))
           return FLASH_SUCCESS;            // Read Data OK
                                            // command in progress ?
        if ((ucBuffer[1] == 0x01) && (ucBuffer[2]== 0x00))
        {  if (WaitChkMausSend (pFlashPort, 0x83, (unsigned char*)"\x00", 1) == true)
           {  ucBuffer[0] = 0;
              ucLength = 255;               // hier write Time Out einbauen !!!
              unsigned long ulTries = 1+ FlashParms.ubEraseTimeOut*1000/FlashParms.uwTelegramTimeOut;
              if (MausReceive (pFlashPort, 0x71, ucBuffer, &ucLength, ulTries, kGuard|kDebug|EchoCom) == true)
                 if ((ucBuffer[1] == 0x01) && (ucBuffer[2]== 0x55))
                    return FLASH_SUCCESS;   // Read Data OK
           }
        }
     }
  } else ucBuffer[1] = 0;                   // destroy buffer for global error
  return Flash_ulwMausResult(ucMemType,uiStartAddress,(unsigned int)ucLength);
}
//-- Write Memory ----------------------------------------------------

/*************************************************************************@FA*
 * @Function          : Flash_ulwMausResult(ubyte MemType, unsigned int Start,End)
 *----------------------------------------------------------------------------
 * @Description       : This function returns the errorcode from the received
 *                      Maus-Telegramme
 *----------------------------------------------------------------------------
 * @Returnvalue       : ulword errorcode
 *----------------------------------------------------------------------------
 * @Parameters        : ubyte ubMemtype: Type of last device
 *                      unsigned int Start: Start Address
 *                      unsigned int End: End Address
 *----------------------------------------------------------------------------
 * @Functioncalls     : ProgressUpdate()
 *----------------------------------------------------------------------------
 * @Global_Vars_Used  : ucBuffer
 *----------------------------------------------------------------------------
 * @Copyright         : (c) 2001 Robert Bosch GmbH, Hildesheim
 * @Author            : CM-CR / ESD4  Peter Tuschik
 *----------------------------------------------------------------------------
 *
 * @History           : Initial version
 *
 ************************************************************************@FE*/
ulword Flash_ulwMausResult(ubyte ucMemType, unsigned int Start, unsigned int Len)
{
  if (PortReadCTS(pFlashPort))              // Check VPP-/ and VCC-Drop
     return FLASH_VPP_DROP;

  if (PortReadDSR(pFlashPort))
     return FLASH_VCC_DROP;

  if (ucBuffer[1] == 0x01)
  {
     if (ucBuffer[2]== 0x09)                // Reject command
     {  ProgressUpdate("Command for Device %02X rejected",ucMemType);
        return FLASH_REJECT;
     }
     if (ucBuffer[2]== 0xAA)                // No success for actual command
     {  ProgressUpdate("No Sucess for actual command");
        return FLASH_NO_SUCCESS;
     }
  }

  if ((ucBuffer[1] == 0x02) && (ucBuffer[2]== 0xAA))
  {
     if (ucBuffer[3] == 0x01)               // Vpp Error
     {  ProgressUpdate("No Sucess: Vpp Error");
        return FLASH_VPP_ERROR;
     }
     if (ucBuffer[3] == 0x02)               // General Power failure
     {  ProgressUpdate("No Sucess: Power failure");
        return FLASH_POWER_FAIL;
     }
     if (ucBuffer[3] == 0x03)               // Erase failure
     {  ProgressUpdate("Device cannot be erased [Device %02X  failure]",ucMemType);
        return FLASH_ERASE_FAIL;
     }
     if (ucBuffer[3] == 0x04)               // programming failure
     {  ProgressUpdate("Internal programming failure [Device %02X  failure] at Adr: %x", ucMemType, Start);
        return FLASH_WRITE_FAIL;
     }
     if (ucBuffer[3] == 0x05)               // Address out of range
     {  ProgressUpdate("No Sucess: Address Range failure: %08X-%08X",Start,Start+Len);
        return FLASH_ADDRESS_ERROR;
     }
     if (ucBuffer[3] == 0x06)               // Invalid Address access
     {  ProgressUpdate("No Sucess: Address alignment failure: %08X", Start);
        return FLASH_ACCESS_ERROR;
     }
     if (ucBuffer[3] == 0x07)               // Device not present
     {  ProgressUpdate("No Sucess: Device %2X not present", ucMemType);
        return FLASH_DEVICE_NOT_PRESENT;
     }
     if (ucBuffer[3] == 0x08)               // Device is protected by Hardware
     {  ProgressUpdate("No Sucess: Device %2X is protected by Hardware", ucMemType);
        return FLASH_DEVICE_PROTECTED;
     }
     else                                   // other error
     {  ProgressUpdate("No Sucess: [ohter Error %X]",ucBuffer[3]);
        return FLASH_OTHER_ERROR;
     }
  }
  ProgressUpdate("Error during Maus communication");
  LogFileWrite("Last Receive: %02x %02x %02x",ucBuffer[0],ucBuffer[1],ucBuffer[2]);
  return FLASH_NO_SUCCESS;                  // global no success
}


/*****************************************************************************
 *    Internal routines for Connection and Download                          *
 *****************************************************************************/

/*************************************************************************@FA*
 * @Function          : Flash_uiInitBootstrap()
 *----------------------------------------------------------------------------
 * @Description       : This function is used to init the Bootstrap mode
 *----------------------------------------------------------------------------
 * @Returnvalue       : ubyte - FLASH_OK or FLASH_NOK
 *----------------------------------------------------------------------------
 * @Parameters        : None
 *----------------------------------------------------------------------------
 * @Functioncalls     : PortSetDTR, PortSetRTS, TimeWait, TimeSetStart,
 *                      TimeWaitTill,
 *----------------------------------------------------------------------------
 * @Global_Vars_Used  :
 *----------------------------------------------------------------------------
 * @Copyright         : (c) 2001 Robert Bosch GmbH, Hildesheim
 * @Author            : CM-CR / ESD4  Peter Tuschik
 *----------------------------------------------------------------------------
 *
 * @History           : Initial version
 *
 ************************************************************************@FE*/
bool Flash_blInitBootstrap(void)
{ ulword i = 0;
  bool retval = false;

  try
  {
    SetPriorityClass(GetCurrentProcess(),REALTIME_PRIORITY_CLASS);
    do
    {
      #ifndef FTDI_SUPPORT
      PortSetDTR(pFlashPort, FLASH_RESET);                  /* reset aktivate */
      PortSetRTS(pFlashPort, !FLASH_VPP);                          /* Vpp off */
      TimeWait(DevParms.ulwPM_F0);                             /* wait 1 mSec */

      TimeSetStart();
      TimeWaitTill(DevParms.ulwPM_F1);                         /* wait 1 mSec */
      PortSetRTS(pFlashPort, FLASH_VPP);                            /* Vpp on */

      TimeWaitTill(DevParms.ulwPM_F1 + DevParms.ulwPM_F2);       /* T2 > 10ms */
      PortSetDTR(pFlashPort, !FLASH_RESET);               /* deaktivate reset */
      TimeSetStart();
      TimeWaitTill(DevParms.ulwPM_F3);                          /* T3 ; > 1ms */

      for (int j = DevParms.ubToggleCounts; j > 0; j--)
      {
          PortSetRTS(pFlashPort, !FLASH_VPP);
          TimeWait(DevParms.ulwPM_F4/2);
          PortSetRTS(pFlashPort,  FLASH_VPP);
          TimeWait(DevParms.ulwPM_F4/2);
      }
      #else
      PortRstBoot(pFlashPort,DevParms.ubToggleCounts,DevParms.ulwPM_F0,DevParms.ulwPM_F1,DevParms.ulwPM_F2,DevParms.ulwPM_F3,DevParms.ulwPM_F4);
      #endif

      /* T4; intro OK if < 15(10) ms (wegen Verzögerung durch Betriebssystem) */
      if (TimeWaitTill(OS_Delay+DevParms.ulwPM_F3+(long)((double)((DevParms.ubToggleCounts+1)*DevParms.ulwPM_F4) * 1.2)) != 0UL)
      {
         TimeWaitTill(DevParms.ulwPM_F5);  /* T5; wait till contoller started */
         retval = true;                                      /* and return OKAY */
         break;
      }
      ProgressUpdate("Timeout for Toggle to Init Bootstrap !");
    } while (i++ < DevParms.ulwPM_F9);
  }
  catch( ... )
  {
  }

  SetPriorityClass(GetCurrentProcess(),NORMAL_PRIORITY_CLASS);

  return retval;                     /* return Not OKAY after Repeat-Retries */

} /* end of Flash_blInitBootstrap() */



/*************************************************************************@FA*
 * @Function          : IntroLoaderPatchBaud()
 *----------------------------------------------------------------------------
 * @Description       : This function patch the baudrate in the Intro loader
 *                      data buffer to the value passed as parameter
 *----------------------------------------------------------------------------
 * @Returnvalue       : void
 *----------------------------------------------------------------------------
 * @Parameters        : baud rate
 *----------------------------------------------------------------------------
 * @Functioncalls     : none
 *----------------------------------------------------------------------------
 * @Global_Vars_Used  :
 *----------------------------------------------------------------------------
 * @Copyright         : (c) 2001 Robert Bosch GmbH, Hildesheim
 * @Author            : CM-CR / ESD4  Peter Tuschik
 *----------------------------------------------------------------------------
 *
 * @History           : Initial version
 *
 ************************************************************************@FE*/
bool Flash_vIntroLoaderPatchBaud(RS232Typ *pPort)
{
  if (ChkValidBaudrate(pPort->ulBaudRate) < 0)
     return false;

  unsigned char ucP = (pPort->ucParity == kNone) ? 0 : (pPort->ucParity == kOdd) ? 2 : 3;

                                      /* gewünschte Baudrate im Introloader */
                                                                 /* patchen */
  if (DevParms.ulwRomSize != 0)                  /* Device-Info vorhanden ? */
  {
    IntParms.pIntroLoader[2]  = ((ubDevice == DEVICE_V850E) || (ubDevice == DEVICE_V850E2M)) ? (0xF2+(ucP<<2)) : (0xC8+(ucP<<4));
    IntParms.pIntroLoader[6]  = BaudRegSet->val3;
    IntParms.pIntroLoader[10] = BaudRegSet->val2;
    IntParms.pIntroLoader[14] = BaudRegSet->val1;
  }
  else                    /* keine Device-Info vorhanden: alter Introloader */
  {
    IntParms.pIntroLoader[26] = BaudRegSet->val1;
    IntParms.pIntroLoader[34] = BaudRegSet->val3;
    IntParms.pIntroLoader[42] = BaudRegSet->val2;
    IntParms.pIntroLoader[50] = (0xC8+(ucP<<4));
  }
  return true;

} /* end of IntroLoaderPatchBaud() */


/*************************************************************************@FA*
 * @Function          : ReqMausLevel()
 *----------------------------------------------------------------------------
 * @Description       : This function checks if Drop detection is possible
 *                      and tries to request the Maus level passed as parameter
 *----------------------------------------------------------------------------
 * @Returnvalue       : void
 *----------------------------------------------------------------------------
 * @Parameters        : Dropcheck != 0, if drop-Detect checking is necessary
                        InitPort != 0, if Port should be set to Maus default
 *----------------------------------------------------------------------------
 * @Functioncalls     : none
 *----------------------------------------------------------------------------
 * @Global_Vars_Used  :
 *----------------------------------------------------------------------------
 * @Copyright         : (c) 2001 Robert Bosch GmbH, Hildesheim
 * @Author            : CM-CR / ESD4  Peter Tuschik
 *----------------------------------------------------------------------------
 *
 * @History           : Initial version
 *
 ************************************************************************@FE*/

struct ReqData
{
  ubyte Level;
  ubyte Cmd;
  char strLevel[5];
  unsigned int delay;
};
struct ReqData ReqData[] = { { 0x0,  0x1C, "0",  0 }
                            ,{ 0x1,  0x8,  "1",  4000 }
                            ,{ 0x10, 0xB,  "0P", 0 } };
ulword ReqMausLevel(ulword DropCheck, ulword InitPort, ubyte &MausLevel)
{
  if (InitPort)
  {  pFlashPort->ulBaudRate = kConnectBaud;   /* def. Maus-Parameter */
     pFlashPort->ucParity = kEven;
     LogFileWrite("Set default Maus-Parameter");

     if (!PortSetup(pFlashPort))
     {  LogFileWrite("ERROR: Default Maus parameter cannot be set");
        return (FLASH_BAUDRATE_SET_FAILED);
     }
  }

  // -------- Check Drop Detection -------------------------

  if (DropCheck)
  {  ulword err = SetDropDetection(DropCheck >> 8);
     SetDropDetection(FLASH_DROP_OFF);            /* switch off after check */
     if (err != FLASH_SUCCESS)
     {  LogFileWrite("ERROR: VCC-/VPP-Drop detected immediately after set");
        return err;
     }
     LogFileWrite("Drop Detection sucessfully tested");
  }

  ProgressUpdate("Request Maus Level %s for Flash",ReqData[MausLevel].strLevel);
  pFlashPort->ulTimeOut = FlashParms.uwTelegramTimeOut;

  TimeWait (10000);                /* Roland braucht ein bißchen Bedenkzeit */

  for (int i = 0; i < FlashParms.ubTelegramRetries; i++)
  {
      ProgressUpdate("Request Maus, Attempt: %d",i+1);
      MausSend(pFlashPort, 0x52, NULL, 0);         /* erstmal Mode abfragen: */
      ucBuffer[0] = 0;
      unsigned char ucLength = 255;
      if (MausReceive(pFlashPort, 0x05, ucBuffer, &ucLength, 20, kDebug|EchoCom ) == true)
      {  if (ucBuffer[2] == 0x55)
         {                          /* Prüfung auf unterstützte Maus-Version */
            if ((ucBuffer[3] != 0x41) && (ucBuffer[3] != 0x40))
            {  LogFileWrite("Wrong Maus-Version: %x",ucBuffer[3]);
               return FLASH_NO_SUCCESS;
            }
                                      /* Ist der Level bereits eingestellt ? */
            if ((   ucBuffer[3] == 0x41
                  && ucBuffer[4] == ReqData[MausLevel].Level && ucBuffer[5] == 0x00
                  && ((ucBuffer[6] == 0x03) || ((ucBuffer[6] == 0x00) && (ucBuffer[7] == 0x04))))
                  ||
                 (ucBuffer[3] == 0x40 && ucBuffer[4] == ReqData[MausLevel].Level)// && ucBuffer[7] == 0x01) 1=temp,2=perm,3=locked
                )
            {  LogFileWrite("Maus Level %s successfully set",ReqData[MausLevel].strLevel);
               return (FLASH_SUCCESS);    /* Raus, wenn bereits eingestellt, */
            }                                                       /* sonst */
            else goto LevelChg;                           /* Level wechseln. */
         } /* Acknowledge */
         if (blAbort) break;                                /* evtl. Abbruch */
      } /* if (MausReceive() */
  } /* for (;;;) */

  LogFileWrite("ERROR: No response for the Maus Level request");
  return (FLASH_REQ_MAUS_FAILED);

  LevelChg:                                            /* Wechsel des Levels */
  ulword RetVal = FLASH_NO_SUCCESS;
  unsigned char ucLength = 255;

                                  /* 0-Protected benötigt vorherigen Level 0 */
  if ((MausLevel == FLASH_MAUS_LVL_0P) && (ucBuffer[4] != ReqData[FLASH_MAUS_LVL_0].Level))
  {  ubyte level0 = FLASH_MAUS_LVL_0;
     if ((RetVal = ReqMausLevel(0,0,level0)) != FLASH_SUCCESS)
        return RetVal;
  }
  TimeWait(ReqData[MausLevel].delay);
  pFlashPort->ulTimeOut = 1000;
  LogFileWrite("Send Level %s Cmd",ReqData[MausLevel].strLevel);
                                                           /* Level wechseln */
 if (MausSend(pFlashPort, ReqData[MausLevel].Cmd, NULL,0))
 {
   if (MausLevel == FLASH_MAUS_LVL_0P)
   {
     if (!MausReceive(pFlashPort, 0x03, ucBuffer, &ucLength, 5, kDebug|EchoCom) == true)
        return FLASH_NO_SUCCESS;
     else if (ucBuffer[0] != 0x3)
     {  LogFileWrite("Expected Value (0x3) not received ! Got: 0x%x",ucBuffer[0]);
        return FLASH_NO_SUCCESS;
     }

     if (IntParms.pGetMKey != NULL)
     {  IntParms.pGetMKey((char*)ucBuffer,false);
        if (ucBuffer[0] != 0)                 // alles fehlerfrei über MKey
        {  ucLength = 255;                    // erhalten

           if (MausSend(pFlashPort, 0x2, ucBuffer+1, ucBuffer[0]) == true)
           {  if (MausReceive(pFlashPort, 0x05, ucBuffer, &ucLength, 5, kDebug|EchoCom) == true)
              {  if (!((ucBuffer[2] != 0x55) || (ucBuffer[4] != 0x10)))
                 {  RetVal = FLASH_SUCCESS;   // --> Prima !!
                    MausLevel = ucBuffer[4];
                 }
                 else LogFileWrite("No success. Got: 0x%x, 0x%x, 0x%x, 0x%x, 0x%x", ucBuffer[0],ucBuffer[1],ucBuffer[2],ucBuffer[3],ucBuffer[4]);
              }
           }
        }                                     // oder Fehler ? dann übernehmen !!
        else
        {  RetVal = (ucBuffer[1]<<24) + (ucBuffer[2]<<24) + (ucBuffer[3]<<24) + ucBuffer[4];
           LogFileWrite((char *)(ucBuffer+5));
        }
     } // if (IntParms.pGetMKey != NULL)
     else LogFileWrite("No function pointer for MKey.exe available");
   } /* if (MausLevel == FLASH_MAUS_LVL_0P) */
   else
   {
     if (!MausReceive(pFlashPort, 0x05, ucBuffer, &ucLength, 1, kDebug|EchoCom))
        return Flash_ulwMausResult(0,0,0);

     if (MausLevel == ucBuffer[4]) RetVal = FLASH_SUCCESS;
        MausLevel = ucBuffer[4];
   }
 } // if (MausSend( ..... )
 return RetVal;

}


/*************************************************************************@FA*
 * @Function          : ComSettings()
 *----------------------------------------------------------------------------
 * @Description       : This function read the Com-Settings from the
 *                      Init-String
 *----------------------------------------------------------------------------
 * @Returnvalue       : void
 *----------------------------------------------------------------------------
 * @Parameters        : String with Com-Settings in the form:
 *                      "ComX,Baudrate,Parity,DataBis,StopBits"
 *----------------------------------------------------------------------------
 * @Functioncalls     : none
 *----------------------------------------------------------------------------
 * @Global_Vars_Used  :
 *----------------------------------------------------------------------------
 * @Copyright         : (c) 2001 Robert Bosch GmbH, Hildesheim
 * @Author            : CM-CR / ESD4  Peter Tuschik
 *----------------------------------------------------------------------------
 *
 * @History           : Initial version
 *
 ************************************************************************@FE*/
void ComSettings(char *pubInitStr,RS232Typ **pComPort)
{ char cParity[] = {'N', 'O', 'E', 'M', 'S'};
  char *EndPtr, *pubCom,*pubStr;
  long lBaud = 9600;
  unsigned char ucDat = k8Bit, ucPar = kNone, ucStp = k1Stop;
  char buf[255];
  strcpy(buf,pubInitStr);
  pubCom = buf;                                                 /* Com-Name */
  pubStr = buf;

  EndPtr = (char*)strchr((const char*)pubStr,',');
  if (EndPtr)
  {  *EndPtr='\0';

     pubStr = EndPtr+1;
     EndPtr = (char*)strchr((const char*)pubStr,',');
     if (EndPtr)
     {  char *ptr;
        *EndPtr = '\0';

        lBaud = strtol((const char*)pubStr,&ptr,10);                /* Baud */

        pubStr = EndPtr+1;
        EndPtr = strchr(pubStr,',');
        if (EndPtr)
        {  *EndPtr='\0';

           ucPar = 4;                                             /* Parity */
           do
           { if (toupper(*pubStr) == cParity[ucPar])
                break;
           } while (--ucPar);

           pubStr=EndPtr+1;
           EndPtr = strchr(pubStr,',');
           if (EndPtr)
           {  *EndPtr='\0';

              ucDat = (unsigned char)atoi(pubStr);              /* DataBits */
              if ((ucDat < k7Bit) || (ucDat > k8Bit))
                 ucDat = k8Bit;

              pubStr = EndPtr+1;

              ucStp = (unsigned char)atoi(pubStr);              /* StopBits */
              if ((ucStp < k1Stop) || (ucStp > k1Stop))
                 ucStp = k1Stop;
           }
        }
     }
     if (*pComPort != NULL)
        PortDelete(*pComPort);
     *pComPort = PortNew(pubCom, FlashParms.uwTelegramTimeOut, lBaud, ucDat, ucPar, ucStp);
  }

} /* end of ComSettings() */

/*************************************************************************@FA*
 * @Function          : ProgressUpdate()
 *----------------------------------------------------------------------------
 * @Description       : This function is used to call the main application
 *                      backcall function for display the progress of the
 *                      internal processing
 *----------------------------------------------------------------------------
 * @Returnvalue       : void
 *----------------------------------------------------------------------------
 * @Parameters        : None
 *----------------------------------------------------------------------------
 * @Functioncalls     : Backcall function of main application
 *----------------------------------------------------------------------------
 * @Global_Vars_Used  :
 *----------------------------------------------------------------------------
 * @Copyright         : (c) 2001 Robert Bosch GmbH, Hildesheim
 * @Author            : CM-CR / ESD4  Peter Tuschik
 *----------------------------------------------------------------------------
 *
 * @History           : Initial version
 *
 ************************************************************************@FE*/
void ProgressUpdate(const char* arg_list, ...)
{
  va_list arg_ptr;                                         /* create buffer */
  char buffer[1000];

  va_start(arg_ptr, (char*)arg_list);
  vsprintf(buffer, arg_list, arg_ptr);
  if (IntParms.pProgressInfo != NULL)                 /* back call, if valid */
     IntParms.pProgressInfo(buffer,true);
  LogFileWrite(buffer);
}

void ProgressRefresh(const char* arg_list, ...)
{
  if (IntParms.pProgressInfo != NULL)
  {  va_list arg_ptr;                                  /* if back call valid */
     char buffer[1000];
     char *format;

     va_start(arg_ptr, arg_list);                           /* create buffer */
     format = (char*)arg_list;
     vsprintf(buffer, format, arg_ptr);
     IntParms.pProgressInfo(buffer,false);
  }
}

/*************************************************************************@FA*
 * @Function          : Flash_ulwDownload
 *----------------------------------------------------------------------------
 * @Description       : This internal function reads the data for the Block-
 *                      typ, passed as parameter, from the DNL-File and
 *                      download the data into the device.
 *----------------------------------------------------------------------------
 * @Returnvalue       : ubyte indicating Success/Error
 *----------------------------------------------------------------------------
 * @Parameters        : uword Typ: Block Type
 *----------------------------------------------------------------------------
 * @Functioncalls     : DFInfo_blReadData,
 *                      DFInfo_ulwGetLen,
 *                      DFInfo_ulwGetStart,
 *                      Flash_ulwErase
 *                      Flash_ulwWrite
 *                      ProgressUpdate
 *----------------------------------------------------------------------------
 * @Global_Vars_Used  :
 *----------------------------------------------------------------------------
 * @Copyright         : (c) 2001 Robert Bosch GmbH, Hildesheim
 * @Author            : CM-CR / ESD4  Peter Tuschik
 *----------------------------------------------------------------------------
 *
 * @History           : Initial Version
 ************************************************************************@FE*/
ulword Flash_ulwDownload(uword Typ, __int64 ulwRNDNum)
{ unsigned char *ptr = NULL;
  ulword RetVal = FLASH_SUCCESS;
  SYSTEMTIME t0;

  GetSystemTime(&t0);

  if (DFInfo_blReadData(IntParms.f_DFile,&DFInfo,&ptr, Typ))
  {  ulword Len = DFInfo_ulwGetLen(&DFInfo,Typ);    // Get BlockLength
     ulword Dest = DFInfo_ulwGetStart(&DFInfo,Typ); // Get StartAddress
     ulword ELen = 0;                               // Parameter for Erase
     ulword EDest = 0;                              // default: Global-Erase
     ubyte MemTyp = (ubyte)(Typ >> 8);

     if ( ( blUpdate &&              // Block erase for Update programming only
            (   ((MemTyp == 0x40) && BlockEra40_Ena)
             || (MemTyp == 0x42)
             || (MemTyp == 0x43)
             || (MemTyp == 0x44)
             || (MemTyp == 0x45)
             || (MemTyp == 0x54) ))
             || (MemTyp == 0x4B)   // Block erase for Virgin/Update programming
             || (MemTyp == 0x4C)
             || (MemTyp == 0x4D)
             || (MemTyp == 0x4F)
             || (MemTyp == 0x34)
             || (MemTyp == 0xC4) )
     {  ELen = Len;                                 // nur diese Devices unter-
        EDest = Dest;                               // stützen ein Block-Erase
     }
                                            // CRC-Check disabled for DAB / DSA
                                            // D-Fire 2 (Bootstrap)
     bool blVerify = !((MemTyp == 0x95) || (MemTyp == 0x97) || (MemTyp == 0x53));

     switch (MemTyp)
     { case 0x40:                           // write int. Flash
                  if (blDebug)
                  {  char OutStr[256];
                     sprintf(OutStr,"Erase and Programm for Device 0x40 ignored, because of Debugmode");
                     ProgressUpdate(OutStr);
                     break;
                  }
                  if (FlashParms.ubReadSignature)
                  {  blVerify = !strstr((char*)aubDevInfo,"EMU");
                     if ( strstr((char*)aubDevInfo,"MASK") ||
                          strstr((char*)aubDevInfo,"EMU" ) )
                     {  char OutStr[256];
                        sprintf(OutStr,"Erase and Programm for Device 0x40 ignored, because %s",aubDevInfo+1);
                        ProgressUpdate(OutStr);
                        blProgInvalid = true;
                        break;              // don't erase + prog. Mask, or Emu
                     }
                  }
       case 0x94:                           // write DAB D-Fire 2 (RAM)
       case 0xC4:                           // write DAB D-Fire 2 (Flash)
       case 0x96:                           // write DSA (RAM)
       case 0xC6:                           // write DSA (Flash)
       case 0x42:                           // write ext. par. Flash
       case 0x43:                           // write secondary Flash
       case 0x44:                           // write ext. ser. Flash #1
       case 0x45:                           // write ext. ser. Flash #2
       case 0x54:                           // write ADR
       case 0x4B:                           // write FGS par. Flash
       case 0x4C:                           // write FGS configuration
       case 0x4D:                           // write FGS 2nd parallel Flash
       case 0x4F:                           // Internal Data Flash
                                            // these devices must be erased
                  // don't erase if chip was erase for device 0x40
                  if (!((MemTyp == 0x40) && blErased))
                     RetVal = Flash_ulwErase(MemTyp,EDest,ELen,ulwRNDNum);
                  if (RetVal != FLASH_SUCCESS)
                     LogFileWrite("Error in Flash_ulwErase detected");

       case 0x53:                           // write ADR Trojan
       case 0x97:                           // write DSA (Trojan)
       case 0x95:                           // write DAB D-Fire 2 (Bootstrap)
       case 0x34:                           // write ext. ser. EEP
       case 0x4E:                           // write FGS serial EEP
                  if (RetVal == FLASH_SUCCESS)
                  {  ProgressUpdate("Start programming of Block %04X",Typ);
                     RetVal = Flash_ulwWrite(MemTyp,ptr,Dest,Len);
                     if (RetVal == FLASH_SUCCESS)
                        ProgressUpdate("\nDevice successful programmed in %s",GetTimeDifference(t0));
                  }
                  break;
       default:   ProgressUpdate("Blocktyp: %X is not supported",Typ);
                  RetVal = FLASH_NO_SUCCESS;
                  break;
     }
                                            // check CRC, if enabled
     if ((RetVal == FLASH_SUCCESS) && (FlashParms.ubVerify) && blVerify && !blAbort)
     {  ulword CRC;
        ProgressUpdate("Start reading CRC from device %X, Adr: %08X-%08X",MemTyp,Dest,Dest+Len);
        if ((RetVal = Flash_uiGetCRC(MemTyp,Dest,Len,(unsigned int*)&CRC)) == FLASH_SUCCESS)
           if (CRC != DFInfo_ulwGetCRC(&DFInfo,Typ))
              RetVal = FLASH_DF_FILE_CRC_ERROR;
     }
  } else RetVal = FLASH_DOWNLOAD_ERROR;
  if (ptr != NULL) delete [] ptr;           // Speicher wieder freigeben
  return (RetVal);
}

/*************************************************************************@FA*
 * @Function          : Flash_ulwWrite
 *----------------------------------------------------------------------------
 * @Description       : This internal subroutine will only be called cy
 *                      Flash_ulwDownload, to process the write of Data to the
 *                      device, depending on the specified Telegrammlength.
 *----------------------------------------------------------------------------
 * @Returnvalue       : ulword indicating Success/Error
 *----------------------------------------------------------------------------
 * @Parameters        : ubyte Typ: Block Type
 *                      ulword Adr,EndAdr: Start and End address for device
 *                      unsigned char *ptr: Ptr to databuf
 *----------------------------------------------------------------------------
 * @Functioncalls     : Flash_uiWrite
 *----------------------------------------------------------------------------
 * @Global_Vars_Used  :
 *----------------------------------------------------------------------------
 * @Copyright         : (c) 2001 Robert Bosch GmbH, Hildesheim
 * @Author            : CM-CR / ESD4  Peter Tuschik
 *----------------------------------------------------------------------------
 *
 * @History           : Initial Version
 ************************************************************************@FE*/
ulword Flash_ulwWrite(ubyte Typ,unsigned char *ptr,ulword Dest,ulword Length)
{ ulword RetVal = FLASH_SUCCESS;
  unsigned char prozent = 0;
  ulword max = Length;

  ProgressUpdate("Programming: 0%%");

  while (Length && (RetVal == FLASH_SUCCESS))   // Loop until error or abort
  { ubyte len = (DevParms.ulwBlindBuf <= 240) ? (ubyte)DevParms.ulwBlindBuf : 240;
                                                // only 3 or 16 multiplyer !!!
    if (Length < (ulword)len) len = (ubyte)Length;

    ubyte i = len;
    if ((blProgSuppress) &&                      // if fast programing is active
        (Typ != 0x95) && (Typ != 0x97))          // and no Trojan-data
       for (;i>0;i--) if (*(ptr+i-1)!=0xff) break; // check for virgin state

    if (i)                                       // if not virgin, then write
       RetVal = Flash_uiWrite(Typ, Dest, len, ptr);
    if (RetVal == FLASH_SUCCESS)                // if no error, the update
    {  ptr += len;                              // progress-state
       Dest += len;
       Length -= len;
       unsigned char proz = (unsigned char)(100-(Length*100/max));
       if (prozent != proz)
          ProgressRefresh("Programming: % 3d%%",(prozent = proz));
       if (blAbort) RetVal = FLASH_ABORTED;
    }
  }
  return RetVal;
}

/*************************************************************************@FA*
 * @Function          : Flash_ulwErase
 *----------------------------------------------------------------------------
 * @Description       : This internal subroutine will only be called by
 *                      Flash_ulwDownload, to process erase a device, if not
 *                      alrady erased.
 *----------------------------------------------------------------------------
 * @Returnvalue       : ulword indicating Success/Error
 *----------------------------------------------------------------------------
 * @Parameters        : ubyte Typ: Block Type,
 *                      ulword Dest: Destination address,
 *                      ulword Len: Blocklength of data top erase,
 *                      __int64 ulwRNDNum: RandomNr for FProm
 *----------------------------------------------------------------------------
 * @Functioncalls     : Flash_uiErase
 *----------------------------------------------------------------------------
 * @Global_Vars_Used  :
 *----------------------------------------------------------------------------
 * @Copyright         : (c) 2001 Robert Bosch GmbH, Hildesheim
 * @Author            : CM-CR / ESD4  Peter Tuschik
 *----------------------------------------------------------------------------
 *
 * @History           : Initial Version
 ************************************************************************@FE*/
ulword Flash_ulwErase(ubyte MemTyp,ulword Dest,ulword Len,__int64 ulwRNDNum)
{
  LogFileWrite("Entered Flash_ulwErase");
                                                 // if erase of all devices is
  if (aubEraseDev[0] == 0xff)     // disabled, then exit after write Random Nr
  {  if ((FlashParms.ubWriteRandomEnable) && (MemTyp == 0x44))
        Flash_uiWriteRandom(MemTyp,ulwRNDNum);
     LogFileWrite("Exit Flash_ulwErase because erase is disabled");
     return FLASH_SUCCESS;
  }

  if ((Dest+Len) == 0)
  {  for (int i = 0; i< FLASH_MAX_DEV; i++)    // if MemTyp already erased,
         if (aubEraseDev[i] == MemTyp)         // then exit
         {  LogFileWrite("Exit Flash_ulwErase because already erased");
            return FLASH_SUCCESS;
         }
     ProgressUpdate("Start erasing Type: %02X",MemTyp);
  }
  else ProgressUpdate("Start block erase Type: %02X from 0x%08x to 0x%08x",MemTyp,Dest,Dest+Len-1);



  ulword RetVal = Flash_uiErase(MemTyp,Dest,Len,ulwRNDNum);

  // !! DAB D-Fire 2 and FGS needs always Blockerase
  if ((MemTyp != 0x4B) && (MemTyp != 0x4D) && (MemTyp != 0xC4))
  {
    if (RetVal == FLASH_SUCCESS)            // if successfully erased
    {  for (int i=0; i< FLASH_MAX_DEV; i++) // store device in queue, to
           if (aubEraseDev[i] == 0)         // disable another erase
           {  aubEraseDev[i] = MemTyp;
              break;
           }
                                            // 0x42 and 0x43 are same device:
       ubyte SecFlash = (MemTyp == 0x42) ? 0x43 : (MemTyp == 0x43) ? 0x42 : 0;
       for (int j=0; j< FLASH_MAX_DEV; j++) // store device in queue, to
           if (aubEraseDev[j] == 0)         // disable another erase
           {  aubEraseDev[j] = SecFlash;
              break;
           }
    }
  }

  LogFileWrite("Exit Flash_ulwErase after erase-process");
  return RetVal;
}

/*****************************************************************************
 *        Internal routines to process Data from the DNL-File                *
 *****************************************************************************/

/*************************************************************************@FA*
 * @Function          : DFInfo_ulwInit()
 *----------------------------------------------------------------------------
 * @Description       : This function reads the DNL-File headers and store the
 *                      Filepositions in the DFInfo-Array
 *----------------------------------------------------------------------------
 * @Returnvalue       : void
 *----------------------------------------------------------------------------
 * @Parameters        : none
 *----------------------------------------------------------------------------
 * @Functioncalls     : rewind, fseek, fread
 *----------------------------------------------------------------------------
 * @Global_Vars_Used  :
 *----------------------------------------------------------------------------
 * @Copyright         : (c) 2001 Robert Bosch GmbH, Hildesheim
 * @Author            : CM-CR / ESD4  Peter Tuschik
 *----------------------------------------------------------------------------
 *
 * @History           : Initial version
 *
 ************************************************************************@FE*/
ulword DFInfo_ulwInit(FILE *DF_file,Flash_strTYInfo *Info)
{ unsigned char buf[255];
  unsigned int uiPos = 0;
  bool blCDMB_used = false;
  bool blKDS_used = false;
  Info->Count = 0;                                     /* default: no entrys */

  rewind(DF_file);
  long FileLen = filelength(DF_file);

  while (fseek(DF_file,uiPos,SEEK_SET) == 0)               /* locate FilePos */
  {
     fread((void*)buf,0x80,1,DF_file);                        /* read header */

     if (Info->Count == FLASH_MAX_DEV)     /* check for device count overrun */
     {  LogFileWrite("More devices found in DF-File than supported");
        return FLASH_NO_SUCCESS;
     }

     /* Create Virtual blocks for CDMB and Konfig-Data */
     if ((buf[0] != 0x40) && blCDMB_used)
     {  blCDMB_used = false;                   /* suppress for future blocks */
        Info->Header[Info->Count].Typ = 0x40EF;        /* CDMB-Create needed */
        Info->Header[Info->Count].Pos = 0;
        Info->Header[Info->Count].Length = 0;
        Info->Header[Info->Count].Start = 0;
        Info->Header[Info->Count].CS = 0;
        Info->Count = Info->Count + 1;                    /* inc # of entrys */
     }
     if ((buf[0] != 0x40) && blKDS_used)
     {  blKDS_used = false;                   /* suppress for future blocks */
        Info->Header[Info->Count].Typ = 0x40ED;        /* KDS-Create needed */
        Info->Header[Info->Count].Pos = 0;
        Info->Header[Info->Count].Length = 0;
        Info->Header[Info->Count].Start = 0;
        Info->Header[Info->Count].CS = 0;
        Info->Count = Info->Count + 1;                    /* inc # of entrys */
     }

     unsigned int FixLen = (buf[14] << 8) + buf[15];
     unsigned int Len = (buf[6]<<24)+(buf[7]<<16)+(buf[8]<<8)+buf[9];
     unsigned int CLen = Len;
     unsigned int Start = (buf[2]<<24)+(buf[3]<<16)+(buf[4]<<8)+buf[5];
     unsigned int CS = (buf[10]<<24)+(buf[11]<<16)+(buf[12]<<8)+buf[13];
     unsigned short HCS = (buf[0x1E]<<8)+buf[0x1F];
     unsigned int HeadLen = (FixLen) ? FixLen : 32;    /* calc len of Header */
                                                               /* store type */
     Info->Header[Info->Count].Typ = (uword)(buf[0] << 8) + buf[1];
     Info->Header[Info->Count].Pos = uiPos + HeadLen; /* and FilePos to data */
     Info->Header[Info->Count].Length = Len;
     Info->Header[Info->Count].Start = Start;
     Info->Header[Info->Count].CS = CS;
                                              /* calc new pos of next header */
     if (FixLen)
     {  unsigned int Rest = Len % FixLen;
        if (Rest) Len += FixLen - Rest;      /* Fill up to fix sector length */
     }

     #ifdef DF_PLAUSI_CHECK
     /* Check for plausibility of Data */

     switch (Info->Header[Info->Count].Typ & 0xff00)/* check for valid Types */
     { case 0x4000:
                    if (Info->Header[Info->Count].Typ != 0x40ED)  // KDS itself
                    {  blCDMB_used = true;
                       blKDS_used = true;
                    }
       case 0x8000:
       case 0x8100:
       case 0x8200:
       case 0x8400:
       case 0x3000:
       case 0x3400:
       case 0x3800:
       case 0x4200:
       case 0x4300:
       case 0x4400:
       case 0x4500:
       case 0x4B00:
       case 0x4C00:
       case 0x4D00:
       case 0x4E00:
       case 0x4F00:
       case 0x5300:
       case 0x5400:
       case 0x5500:
       case 0x9400:
       case 0x9500:
       case 0xC400:
       case 0x9600:
       case 0x9700:
       case 0xC600:
       case 0x8300:
                    break;
       default:     { char Help[100];
                      sprintf(Help,"ERROR: Unknown Blocktype %04X in DNL-File",Info->Header[Info->Count].Typ);
                      LogFileWrite(Help);
                      return FLASH_DF_FILE_HEADER_ERROR;
                    }
     }

     unsigned short CHCS = 0;                       /* Check Header Checksum */
     for (int i = 0; i< 0x1E; i++)
         CHCS += buf[i];
     CHCS = ~(CHCS);
     CHCS++;

     if (CHCS != HCS)
     {  char Help[100];
        sprintf(Help,"ERROR: Wrong Checksum in Header data of Block %04X in DNL-File",Info->Header[Info->Count].Typ);
        LogFileWrite(Help);
        return FLASH_DF_FILE_CRC_ERROR;
     }

     unsigned char *csbuf = new unsigned char [CLen];
     fseek(DF_file,uiPos+HeadLen,SEEK_SET);                     /* read data */
     fread((void*)csbuf,CLen,1,DF_file);
     ulword CCS = DNL_CRC_DEFAULT_CRC;

     for (unsigned int j=0;j<CLen;j++)
         CCS = Flash_ulwGetCRC(csbuf[j],CCS);
     delete [] csbuf;

     if (CCS != CS)
     {
        char Help[100];
        sprintf(Help,"ERROR: Wrong Checksum in data block of Block %04X in DNL-File (%08X)",
          Info->Header[Info->Count].Typ,(unsigned int)CCS);

        LogFileWrite(Help);
        return FLASH_DF_FILE_CRC_ERROR;
     }

     /* End Check for plausibility of Data */
     #endif

     uiPos += (Len + HeadLen);              /* calculate addr to next header */
     Info->Count = Info->Count + 1;                       /* inc # of entrys */

     if ((long)uiPos >= FileLen) break;
  }

  if (blCDMB_used)                          /* if device 0x40 was last, then */
  {  Info->Header[Info->Count].Typ = 0x40EF;           /* CDMB-Create needed */
     Info->Header[Info->Count].Pos = 0;
     Info->Header[Info->Count].Length = 0;
     Info->Header[Info->Count].Start = 0;
     Info->Header[Info->Count].CS = 0;
     Info->Count = Info->Count + 1;                       /* inc # of entrys */
  }
  if (blKDS_used)
  {  Info->Header[Info->Count].Typ = 0x40ED;            /* KDS-Create needed */
     Info->Header[Info->Count].Pos = 0;
     Info->Header[Info->Count].Length = 0;
     Info->Header[Info->Count].Start = 0;
     Info->Header[Info->Count].CS = 0;
     Info->Count = Info->Count + 1;                       /* inc # of entrys */
  }

  return FLASH_SUCCESS;
} /* end of DFInfo_ulwInit() */


/*************************************************************************@FA*
 * @Function          : DFInfo_ulwGetLen()
 *----------------------------------------------------------------------------
 * @Description       : This function reads the length of the block, specified
 *                      by the Typ, passed as parameter
 *----------------------------------------------------------------------------
 * @Returnvalue       : ulword length
 *----------------------------------------------------------------------------
 * @Parameters        : Type of data block
 *----------------------------------------------------------------------------
 * @Functioncalls     : none
 *----------------------------------------------------------------------------
 * @Global_Vars_Used  :
 *----------------------------------------------------------------------------
 * @Copyright         : (c) 2001 Robert Bosch GmbH, Hildesheim
 * @Author            : CM-CR / ESD4  Peter Tuschik
 *----------------------------------------------------------------------------
 *
 * @History           : Initial version
 *
 ************************************************************************@FE*/
ulword DFInfo_ulwGetLen(Flash_strTYInfo *Info,uword Typ)
{ int i = 0;

  while (i < Info->Count)                   /* loop to search entry for typ */
  {     if (Info->Header[i].Typ == Typ)          /* and return block length */
           return (Info->Header[i].Length);
        i++;
  }
  return 0;                                       /* return 0, if not found */

} /* end of DFInfo_ulwGetLen() */


/*************************************************************************@FA*
 * @Function          : DFInfo_ulwGetCRC()
 *----------------------------------------------------------------------------
 * @Description       : This function reads the CRC of the block, specified
 *                      by the Typ, passed as parameter
 *----------------------------------------------------------------------------
 * @Returnvalue       : ulword CRC
 *----------------------------------------------------------------------------
 * @Parameters        : DF-Info Structure, Type of data block
 *----------------------------------------------------------------------------
 * @Functioncalls     : none
 *----------------------------------------------------------------------------
 * @Global_Vars_Used  :
 *----------------------------------------------------------------------------
 * @Copyright         : (c) 2001 Robert Bosch GmbH, Hildesheim
 * @Author            : CM-CR / ESD4  Peter Tuschik
 *----------------------------------------------------------------------------
 *
 * @History           : Initial version
 *
 ************************************************************************@FE*/
ulword DFInfo_ulwGetCRC(Flash_strTYInfo *Info,uword Typ)
{ int i = 0;

  while (i < Info->Count)                   /* loop to search entry for typ */
  {     if (Info->Header[i].Typ == Typ)             /* and return block CRC */
           return (Info->Header[i].CS);
        i++;
  }
  return 0;                                       /* return 0, if not found */

} /* end of DFInfo_ulwGetCRC() */


/*************************************************************************@FA*
 * @Function          : DFInfo_ulwGetStart()
 *----------------------------------------------------------------------------
 * @Description       : This function reads the StartAddress of the block,
 *                      specified by the Typ, passed as parameter
 *----------------------------------------------------------------------------
 * @Returnvalue       : ulword StartAddress
 *----------------------------------------------------------------------------
 * @Parameters        : DF-Info Structure, Type of data block
 *----------------------------------------------------------------------------
 * @Functioncalls     : none
 *----------------------------------------------------------------------------
 * @Global_Vars_Used  :
 *----------------------------------------------------------------------------
 * @Copyright         : (c) 2001 Robert Bosch GmbH, Hildesheim
 * @Author            : CM-CR / ESD4  Peter Tuschik
 *----------------------------------------------------------------------------
 *
 * @History           : Initial version
 *
 ************************************************************************@FE*/
ulword DFInfo_ulwGetStart(Flash_strTYInfo *Info,uword Typ)
{ int i = 0;

  while (i < Info->Count)                   /* loop to search entry for typ */
  {     if (Info->Header[i].Typ == Typ)         /* and return start address */
           return (Info->Header[i].Start);
        i++;
  }
  return 0;                                       /* return 0, if not found */

} /* end of DFInfo_ulwGetStart() */


/*************************************************************************@FA*
 * @Function          : DFInfo_blReadData()
 *----------------------------------------------------------------------------
 * @Description       : This function reads the data of the block, specified
 *                      by the Typ, passed as parameter and calculate a Check-
 *                      sum, which will be stored at the end of the buffer
 *----------------------------------------------------------------------------
 * @Returnvalue       : size of buffer if no error, else 0
 *----------------------------------------------------------------------------
 * @Parameters        : Type of data block
 *----------------------------------------------------------------------------
 * @Functioncalls     : none
 *----------------------------------------------------------------------------
 * @Global_Vars_Used  :
 *----------------------------------------------------------------------------
 * @Copyright         : (c) 2001 Robert Bosch GmbH, Hildesheim
 * @Author            : CM-CR / ESD4  Peter Tuschik
 *----------------------------------------------------------------------------
 *
 * @History           : Initial version
 *
 ************************************************************************@FE*/
int DFInfo_blReadData(FILE *DF_file,Flash_strTYInfo *Info,ubyte **ptr, uword Typ)
{ unsigned char ucCS,i = 0;
  ulword j;

  while (i < Info->Count)                    /* loop to search entry for typ */
  {     if (Info->Header[i].Typ == Typ)           /* and return block length */
        {  if (*ptr != NULL) delete [] *ptr;
           *ptr = new ubyte [Info->Header[i].Length+1];        /* +1 Byte CS */
           if (*ptr == NULL) return 0;

           if (fseek(DF_file,Info->Header[i].Pos,SEEK_SET) != 0)
              return 0;                            /* seek error in DNL-File */
           if (fread((void*)*ptr,Info->Header[i].Length,1,DF_file) == 0)
              return 0;                            /* read error in DNL-File */
           ucCS = 0;
           ubyte *help = *ptr;
           for (j = 0; j < Info->Header[i].Length; j++)
               ucCS += help[j];

           help[j] = ~ucCS + 1;
           return Info->Header[i].Length;
        }
        i++;
  }
  LogFileWrite("ERROR: Blocktype not found in DNL-File");
  return 0;                                    /* return false, if not found */

} /* end of DFInfo_blReadData() */

/*************************************************************************@FA*
 * @Function          : DFInfo_blReadDevParms()
 *----------------------------------------------------------------------------
 * @Description       : This function reads the content of the device data
 *                      from a Software-Information Block, if existing.
 *                      If these data are not existing, the default values of
 *                      a V850 will be set.
 *                      If there is a mistake in the DNL-File content, the
 *                      return value is FLASH_DF_FILE_CONTENT_ERROR.
 *----------------------------------------------------------------------------
 * @Returnvalue       : FLASH_SUCCESS or FLASH_NO_SUCCESS if Device data are
 *                      available or not.
 *                      FLASH_DF_FILE_CONTENT_ERROR if error during analysis
 *                      was detected.
 *----------------------------------------------------------------------------
 * @Parameters        : Type of data block
 *----------------------------------------------------------------------------
 * @Functioncalls     : none
 *----------------------------------------------------------------------------
 * @Global_Vars_Used  :
 *----------------------------------------------------------------------------
 * @Copyright         : (c) 2001 Robert Bosch GmbH, Hildesheim
 * @Author            : CM-CR / ESD4  Peter Tuschik
 *----------------------------------------------------------------------------
 *
 * @History           : Initial version
 *
 ************************************************************************@FE*/
#define ML_CALC(a,b)  ( ((ulword)(BCD_DEZ(a)*10) + (ulword)(BCD_DEZ(b >> 4)) ) * (ulword)pow10(b & 0xf) )
ulword  DFInfo_blReadDevParms(FILE *DF_file,Flash_strTYInfo *Info,Flash_TYDevParms *pstDevParms)
{ unsigned char *ptr = NULL;
  ulword  retval = FLASH_NO_SUCCESS; /* Default: Keine Device Info vorhanden */
  int Len = DFInfo_blReadData(DF_file,Info,&ptr,0x8000);
  //unsigned char *SIB_Ptr = ptr+1; //Satya: removed to correct the warning
  unsigned char *DIB_Ptr = NULL; //, IDB_Ptr = NULL, *Customer_Ptr = NULL;
  memset(aubComNr,0,sizeof(aubComNr));             /* Default: ComNr löschen */
  CDMB_Offset = 0;
  KDS_Offset = 0;
  BlockEra40_Ena = false;
                                 /* Erstmal Defaultwerte für V850 annehmen ! */
  SX2 = SX3 = FX4 = false;                      /* Default: Kein SX2/SX3/FX4 */
  SysClock = 16;                                       /* Default value = 16 */
  IntParms.ulwMode = 0;                       /* set default values for V850 */
  ubDevice = 0;

  pstDevParms->ubToggleCounts = 8;
  pstDevParms->ulwSysClock = 16000000;

  pstDevParms->ulwRomSize = 0;
  memset(pstDevParms->uDevSign.aubDevice,0,sizeof(DevParms.uDevSign));
  pstDevParms->ulwPM_F0    = kT1;
  pstDevParms->ulwPM_F1    = kT1;
  pstDevParms->ulwPM_F2    = kT2;
  pstDevParms->ulwPM_F3    = kT3;
  pstDevParms->ulwPM_F4    = kT6;
  pstDevParms->ulwPM_F5    = kT5;
  pstDevParms->ulwPM_F6    = 500000;
  pstDevParms->ulwPM_F7    = 18;
  pstDevParms->ulwPM_F8    = 1875;
  pstDevParms->ulwPM_F9    = FlashParms.ubTelegramRetries;
  pstDevParms->ulwPM_F10   = 1875;
  pstDevParms->ulwPM_F11   = 2000;
  pstDevParms->ulwPM_01    = 800;
  pstDevParms->ulwPM_08    = 4000;
  pstDevParms->ulwBlindBuf = SetBlindBuf(FlashParms.ubTelegramLength);
  pstDevParms->ulwPM_14    = 130;
  pstDevParms->ulwPM_18    = 200000;
  pstDevParms->ulwMAUS_Buf = 17;
  //pstDevParms->ulwMAUS_Buf = pstDevParms->ulwBlindBuf;
  pstDevParms->ulwPM_M4    = 130;
  pstDevParms->ulwPM_M8    = 200000;

  if (DebOS_Delay) OS_Delay = DebOS_Delay;
  else OS_Delay = 4000;                          /* 4 ms Zeit fürs Betriebs- */
                                                 /* system bei InitBootstrap */

                                        /* DownloadFileNr für ErrHist lesen. */
if (Len > 0)
{
  sprintf((char*)aubDFNr,"%02x%02x%02x%02x%02x",*(ptr+6),*(ptr+7),*(ptr+8),*(ptr+9),*(ptr+10));

  if (Len > 11)                                  /* Device Daten vorhanden ? */
  {  ubyte SIB_Version = *(ptr+11);
     ubyte SIB_SubVersion = *(ptr+12);

     if (SIB_Version == 0x55)                           /* altes V850-Format */
     {                                                  /* ================= */
        retval = FLASH_NO_SUCCESS;
     }
     else
     {
      if (SIB_Version == 0x88)             /* erweitertes Device Info Format */
         DIB_Ptr = ptr+12;                 /* ============================== */
      else if ( ((SIB_Version == 0x03) && (SIB_SubVersion == 0x00)) ||
                ((SIB_Version == 0x04) && (SIB_SubVersion == 0x00)) ||
                ((SIB_Version == 0x05) && (SIB_SubVersion == 0x00))
              )
                                           /* neues Format für Software Info */
      {                                    /* ============================== */
         unsigned char *Ptr = (ptr+13);  /* Start der SIB - Table of content */
         unsigned char H[3];
         WORD Type;
         LONG Offset;
         BlockEra40_Ena = true;          /* Erst ab dieser Version wird Block-
                                           erase vom Flashwriter unterstützt */

         do                              /* Einträge lesen und Pointer holen */
         {  Type = (*Ptr << 8) + *(Ptr+1);
            Ptr+=2;
            Offset = (*Ptr << 24) + (*(Ptr+1) << 16) + (*(Ptr+2) << 8) + *(Ptr+3);
            Ptr +=4;
            if (Type == 0x1)      ; // SIB_Ptr not used in dll
            else if (Type == 0x3) ; // IDB_Ptr not used in dll
            else if (Type == 0x4) ; // Customer_Ptr not used in dll
            else if (Type == 0x2) DIB_Ptr      = (ptr + Offset + 3);
            else if (Type == 0x5) CDMB_Offset  = Offset;
            else if (Type == 0x7) KDS_Offset   = Offset;
            else if (Type == 0x8) RSA_Offset   = Offset;
            else if (Type == 0x6)
            {  for (int i=0;i<5;i++)
               {   sprintf((char*)H,"%02x",*(ptr+Offset+3+i));
                   strcat((char*)aubComNr,(char*)H);
               }
               for (int i=10;i<15;i++)
                   aubComNr[i] = *(ptr+Offset+3-5+i);
            }
            else if (Type == 0xFFFF) ;
            else retval = FLASH_DF_FILE_UNKNOWN_SIB_DATA;
         }
         while (Type != 0xFFFF);

                                        /* Plausibilitätsprüfung der Version */
                                        // 0100 = SX2/SX3
                                        // 0101 = FX4
         if (DIB_Ptr && ((*(DIB_Ptr-1) > 1) || (*(DIB_Ptr-2) != 1)))
            DIB_Ptr = NULL;             // Falls invalid, dann Error
      }
      else retval = FLASH_DF_FILE_INVALID_SIB_VERSION;
                                                   /* Wenn DIB-Ptr vorhanden */
      if ((DIB_Ptr != NULL)&&                      /* und kein andere Fehler */
          (retval == (ulword)FLASH_NO_SUCCESS))    /* dann Parameterstruktur */
      {                                     /* für Applikation aktualisieren */
         SX2 = (*(DIB_Ptr+7) == 4);                  /* SX2: other structure */
         FX4 = (*(DIB_Ptr-1) == 1) && (*(DIB_Ptr-2) == 1); /* FX4: other structure */
         unsigned char *D_Ptr = DIB_Ptr;

         if (FX4)
         { pstDevParms->ulwSysClock = (BCD_DEZ(*D_Ptr)*10000000);
           D_Ptr++;
         }
         else pstDevParms->ulwSysClock = 0;
         pstDevParms->ulwSysClock    = pstDevParms->ulwSysClock+(BCD_DEZ(*D_Ptr)*1000000 + BCD_DEZ(*(D_Ptr+1))*10000 +BCD_DEZ(*(D_Ptr+2))*100 + BCD_DEZ(*(D_Ptr+3)));
         SysClock = pstDevParms->ulwSysClock/1000000;

         if (FX4)
         {  pstDevParms->ulwOscClock    = (BCD_DEZ(*(D_Ptr+4))*10000000 + BCD_DEZ(*(D_Ptr+5))*1000000 + BCD_DEZ(*(D_Ptr+6))*10000 +BCD_DEZ(*(D_Ptr+7))*100 + BCD_DEZ(*(D_Ptr+8)));
            memcpy((char*)pstDevParms->aubSysOpt,(const char*)(D_Ptr+9),4);
            memcpy((char*)pstDevParms->aubSysSec,(const char*)(D_Ptr+13),6);
            memcpy((char*)pstDevParms->aubSysOCD,(const char*)(D_Ptr+19),12);
            D_Ptr+=27;
         }

         pstDevParms->ubToggleCounts = *(D_Ptr+4);

         memcpy((char*)pstDevParms->uDevSign.Parts.aubVendor,(const char*)(D_Ptr+5),3);
         unsigned char *ptrRomSize = (unsigned char*)pstDevParms->uDevSign.Parts.aubRomSize;

         if (SX2) D_Ptr+=2;           /* Device Extension bei SX2 ignorieren */
         if (FX4)                           /* Bei FX4 wird Romsize komplett */
         {  memcpy(ptrRomSize,(const char*)(D_Ptr+18),4);  /* neu gehandelt  */
            D_Ptr -= 3;       /* und Pointer für weiteren Zugriff korrigiert */
         }
         else memcpy(ptrRomSize,(const char*)(D_Ptr+8),3);
         if (SX2) D_Ptr+=9;      /* Flashdata Start & End bei SX2 ignorieren */

         memcpy((char*)pstDevParms->uDevSign.Parts.aubSignature,(const char*)(D_Ptr+11),10);

         pstDevParms->uDevSign.aubDevice[17] = 0;
                               /* remove Bit 7 and spaces from begin and end */
         ConvertSignature(pstDevParms->uDevSign.Parts.aubSignature,pstDevParms->uDevSign.Parts.aubSignature);

         int Derivat = DERIVAT_UNKNOWN;
         ubDevice = DecodeDevice((char*)pstDevParms->uDevSign.Parts.aubSignature,&Derivat);
         SX3 = (Derivat == DERIVAT_SG3) || (Derivat == DERIVAT_SJ3);

         if (FX4)
         {  D_Ptr+=14;
            pstDevParms->ulwRomSize     = *ptrRomSize + (*(ptrRomSize+1)<<8) + (*(ptrRomSize+2)<<16) + (*(ptrRomSize+3)<<24);
         }
         else pstDevParms->ulwRomSize     = *ptrRomSize + ((*(ptrRomSize+1) & 0x7f)<<7) + ((*(ptrRomSize+2) & 0x7f)<<14);
         pstDevParms->ulwPM_F0       = ML_CALC(*(D_Ptr+21),*(D_Ptr+22));
         pstDevParms->ulwPM_F1       = ML_CALC(*(D_Ptr+23),*(D_Ptr+24));
         pstDevParms->ulwPM_F2       = ML_CALC(*(D_Ptr+25),*(D_Ptr+26));
         pstDevParms->ulwPM_F3       = ML_CALC(*(D_Ptr+27),*(D_Ptr+28));
         pstDevParms->ulwPM_F4       = ML_CALC(*(D_Ptr+29),*(D_Ptr+30));
         pstDevParms->ulwPM_F5       = ML_CALC(*(D_Ptr+31),*(D_Ptr+32));
         pstDevParms->ulwPM_F6       = ML_CALC(*(D_Ptr+33),*(D_Ptr+34));
         pstDevParms->ulwPM_F7       = (ML_CALC(*(D_Ptr+35),*(D_Ptr+36)))/SysClock;
         pstDevParms->ulwPM_F8       = (ML_CALC(*(D_Ptr+37),*(D_Ptr+38)))/SysClock;
         pstDevParms->ulwPM_F9       = (*(D_Ptr+39)<<8) + *(D_Ptr+40);
         pstDevParms->ulwPM_F10      = (ML_CALC(*(D_Ptr+41),*(D_Ptr+42)))/SysClock;
         pstDevParms->ulwPM_F11      = ML_CALC(*(D_Ptr+43),*(D_Ptr+44));
         pstDevParms->ulwPM_01       = ML_CALC(*(D_Ptr+45),*(D_Ptr+46));
         pstDevParms->ulwPM_08       = ML_CALC(*(D_Ptr+47),*(D_Ptr+48));
         pstDevParms->ulwMaxBlindBuf = BCD_DEZ(*(D_Ptr+49))*100 + BCD_DEZ(*(D_Ptr+50));
         pstDevParms->ulwBlindBuf    = SetBlindBuf(pstDevParms->ulwMaxBlindBuf-3);

         if (DebOS_Delay) OS_Delay = DebOS_Delay;
         else OS_Delay = 1000;           // Wartezeit für Betriebssystem 1 ms

         pstDevParms->ulwPM_14    = ML_CALC(*(D_Ptr+51),*(D_Ptr+52));
         pstDevParms->ulwPM_18    = ML_CALC(*(D_Ptr+53),*(D_Ptr+54));
         pstDevParms->ulwMAUS_Buf = BCD_DEZ(*(D_Ptr+55))*100 + BCD_DEZ(*(D_Ptr+56));
         pstDevParms->ulwPM_M4    = ML_CALC(*(D_Ptr+57),*(D_Ptr+58));
         pstDevParms->ulwPM_M8    = ML_CALC(*(D_Ptr+59),*(D_Ptr+60));
         retval = FLASH_SUCCESS;
      }
      else if (retval == (ulword)FLASH_NO_SUCCESS)
              retval = FLASH_DF_FILE_CONTENT_ERROR;
     } /* if (SIB_Version == 0x55) */
  } /* if (Len > 11) */
} else LogFileWrite("Warning: There is no Software Information Block available in the DNL-file !");

  if (ptr) delete [] ptr;                          /* free allocated memory */

  LogFileWrite("Read DFInfo (%s). New values are: %d, %d, %d, %s, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d",
           (DIB_Ptr) ? "DIB" : "def",pstDevParms->ubToggleCounts,pstDevParms->ulwRomSize,pstDevParms->ulwSysClock,
           pstDevParms->uDevSign.Parts.aubSignature,pstDevParms->ulwPM_F0,pstDevParms->ulwPM_F1,
           pstDevParms->ulwPM_F2,pstDevParms->ulwPM_F3,pstDevParms->ulwPM_F4,
           pstDevParms->ulwPM_F5,pstDevParms->ulwPM_F6,pstDevParms->ulwPM_F7,
           pstDevParms->ulwPM_F8,pstDevParms->ulwPM_F9,pstDevParms->ulwPM_F10,
           pstDevParms->ulwPM_F11,pstDevParms->ulwPM_01,pstDevParms->ulwPM_08,
           pstDevParms->ulwBlindBuf,pstDevParms->ulwPM_14,pstDevParms->ulwPM_18,
           pstDevParms->ulwMAUS_Buf,pstDevParms->ulwPM_M4,pstDevParms->ulwPM_M8);

  return retval;
}

//---------------------------------------------------------------------------
//-----  Hilfsroutinen
//---------------------------------------------------------------------------

void Flash_vWriteErrHistory(uword Mem,ulword Result)
{ ubyte buf[256];
  unsigned char *ptr = NULL;

  if (Flash_blReadErrHistory(buf,&ptr))
  {
     if (Mem == 0)                             /* Only if succes (Mem = 0): */
     {  buf[0] = *(ptr+4);                                 /* write Version */
        buf[1] = *(ptr+5);
        buf[2] = *(ptr+1);                                          /* Date */
        buf[3] = *(ptr+2);
        buf[4] = *(ptr+3);
        buf[5] = blUpdate ? buf[5]+1 : 0;         /* and inc NOSD on update */
        buf[6] = FLASH_VERSION;
     }

     if (Result != FLASH_SUCCESS)      /* if error detected: Update history */
     {
       int i;
       ubyte MemTyp = (ubyte)(Mem >> 8);

       for (i=0;i<9;i+=3)                            /* search free or same */
       {   if (buf[8+i] == 0) break;
           else if (buf[8+i] == MemTyp) break;
       }
       buf[8+i] = MemTyp;
       buf[9+i] = (ubyte)Result;                        /* store error type */
       buf[10+i] = buf[10+i]+1;                            /* inc err count */
                                                         /* sort err values */
       bool changed;
       do
       { changed = false;
         for (int i=0; i<9; i+=3)
         {
             if (buf[8+i+2] < buf[8+i+2+3])
             {  for (int j=0;j<3;j++)                 /* change 3 bytes of  */
                {   ubyte help = buf[8+i+j];                 /* error entry */
                    buf[8+i+j] = buf[8+i+3+j];
                    buf[8+i+3+j] = help;
                }
                changed = true;
             }
         }
       } while (changed);
     }

     int Len = 20;                                      /* Default: Len = 20 */
     if (aubComNr[0] != 0)                           /* If ComNr avail, then */
     {  memcpy(buf+21,aubDFNr,10);                     /* Len = 31 to update */
        Len = 31;                                              /* DFNr, too. */
     }
     if (Flash_uiWrite(0x34,27,Len,buf) != FLASH_SUCCESS)
        ProgressUpdate("Error while writing history into prot. EEP");
     else ProgressUpdate("History successfully written into prot. EEP");
  }
  if (ptr != NULL) delete [] ptr;
}
//---------------------------------------------------------------------------

bool Flash_blReadErrHistory(unsigned char *buffer,unsigned char **ptr)
{ bool RetVal = false;

  if (DFInfo_blReadData(IntParms.f_DFile,&DFInfo,ptr,0x8000))
  { if (DFInfo_ulwGetLen(&DFInfo,0x3401))     /* only if ext. EEP available */
    { if (Flash_uiRead(0x34,27,46,buffer) == FLASH_SUCCESS)
      {  RetVal = true;
      } else ProgressUpdate("Error while reading history from prot. EEP");
    } else ProgressUpdate("No EEP to read history available");
  } else ProgressUpdate("No Infoblock found in DF_File");

  return RetVal;
}
//---------------------------------------------------------------------------

void LogFileWrite(const char* arg_list, ...)
{ static char old[3500] = {0};

  if (!FlashParms.ubLogEnable) return;            /* ignore Logfile write ? */

  va_list arg_ptr;                                         /* create buffer */
  char buffer[3500];
  char *format;
  char strDatum[30];
  SYSTEMTIME t;

  GetLocalTime(&t);

  #ifdef LINUX
  struct tm *ptm = localtime((const time_t*)&t);

  long mSecs = (t.tv_usec / 1000);
  sprintf(strDatum,"%2d.%02d.%04d - %2d:%02d:%02d.%03lu",
          ptm->tm_mday, ptm->tm_mon + 1, ptm->tm_year + 1900, ptm->tm_hour, ptm->tm_min, ptm->tm_sec, mSecs);
  #else
  sprintf(strDatum,"%2d.%02d.%04d - %2d:%02d:%02d.%03d",
          t.wDay,t.wMonth,t.wYear,t.wHour,t.wMinute,t.wSecond,t.wMilliseconds);
  #endif

  va_start(arg_ptr, arg_list);
  format = (char*)arg_list;
  vsprintf(buffer, format, arg_ptr);

  if (strcmp(old,buffer) != 0)              /* nur bei Änderung speichern ! */
  {  strcpy(old,buffer);

     FILE *fStream = fopen(LogFileName,"a+");
     if (fStream)
     {  fprintf(fStream, "\n%s:  %s",strDatum,buffer);
        fclose(fStream);
     }
  }
}
//---------------------------------------------------------------------------

unsigned int Flash_ulwGetCRC(unsigned char ubData, unsigned int ulwPrevCRC)
{ unsigned char ubCnt;
  unsigned int ulwTemp = (unsigned int) ubData << 24;

  for (ubCnt=0; ubCnt<8; ++ubCnt)               /* Loop for each bit in char */
  {
      if ((ulwTemp^ulwPrevCRC) & 0x80000000)  /* Shift CRC, feed back hi bit */
         ulwPrevCRC = (ulwPrevCRC<<1) ^ DNL_CRC_GREEN_POLYNOM;
      else ulwPrevCRC <<= 1;
      ulwTemp <<= 1;
  }
  return (ulwPrevCRC);
}
//---------------------------------------------------------------------------

unsigned int Flash_ulwBPCRC(unsigned char ubData, unsigned int ulwPrevCRC, unsigned int ulwPolynom)
{ unsigned char ubCnt;
  unsigned int ulwTemp = (unsigned int) ubData << 24;

  for (ubCnt=8; ubCnt>0; ubCnt--)               /* Loop for each bit in char */
  {
      if ((ulwTemp^ulwPrevCRC) & 0x80000000)  /* Shift CRC, feed back hi bit */
         ulwPrevCRC = (ulwPrevCRC<<1) ^ ulwPolynom;
      else ulwPrevCRC <<= 1;
      ulwTemp <<= 1;
  }
  return (ulwPrevCRC);
}
//---------------------------------------------------------------------------

unsigned int Flash_ulwVAGCRC(unsigned char ubData, unsigned int ulwPrevCRC, unsigned int ulwPolynom)
{ unsigned char ubCnt;
  unsigned int ulwCRC = (ulwPrevCRC^ubData) & 0xff;

  for (ubCnt=8; ubCnt>0; ubCnt--)              /* Loop for each bit in char */
  {
      if ((ulwCRC & 1) != 0)
         ulwCRC = (ulwCRC>>1) ^ ulwPolynom;
      else ulwCRC >>= 1;
  }
  return (ulwPrevCRC>>8)^ulwCRC;
}
//---------------------------------------------------------------------------

DLLEXPORT void   __stdcall Flash_vXmData (char *Buf, ulword Len)
{
  vect v = IntParms.pComDebug;                     /* save debug-vector and */
  IntParms.pComDebug = NULL;                   /* clear to suppress display */

  PortSend(pFlashPort,(unsigned char*)Buf,Len);        /* transmit data buf */

  IntParms.pComDebug = v;                           /* restore debug-vector */
}
//---------------------------------------------------------------------------

DLLEXPORT bool   __stdcall Flash_bRmData (char *Buf, ulword ulMaxSize, ulword *ulRecCount)
{
  vect v = IntParms.pComDebug;                     /* save debug-vector and */
  IntParms.pComDebug = NULL;                   /* clear to suppress display */

  *ulRecCount = ulMaxSize;
  bool retval = (PortReceive(pFlashPort, (unsigned char*)Buf, (unsigned long *)ulRecCount, 0) == kOK);

  IntParms.pComDebug = v;                           /* restore debug-vector */
  return retval;
}
//---------------------------------------------------------------------------
#ifdef DROP_READ_TEST
DLLEXPORT ulword __stdcall Flash_ulwReadCTS(void)
{
  return PortReadCTS(pFlashPort);
}

DLLEXPORT ulword __stdcall Flash_ulwReadDSR(void)
{
  return PortReadDSR(pFlashPort);
}
#endif

ulword SetDropDetection(ulword ulwSet) /* set VCC-/VPP Drop Control on or off */
{ bool DropCtrl = false;
  ulword RetVal = FLASH_SUCCESS;

  ulwSet &= 0x3;
  if (ulwSet == 0x03)                                                // off ?
  {  if (pFlashPort->ucCts || pFlashPort->ucDsr)
     {  LogFileWrite("Reset any VPP-/ VCC Drop - detection");
        pFlashPort->ucCts = 0;
        pFlashPort->ucDsr = 0;
        DropCtrl = true;
     }
  }                                                                  // or on ?
  else
  {
     unsigned char ucCts = (ulwSet & FLASH_MODE_VPP) ? 0 : 1;
     unsigned char ucDsr = (ulwSet & FLASH_MODE_VCC) ? 0 : 1;
     if ((pFlashPort->ucCts != ucCts) || (pFlashPort->ucDsr != ucDsr))
     {  LogFileWrite("Set Port configuration to defined state of VPP-/ VCC detection: %x",ulwSet);
        pFlashPort->ucCts = ucCts;
        pFlashPort->ucDsr = ucDsr;
        DropCtrl = true;
     }
  }

  if (DropCtrl && !PortSetup(pFlashPort))
  {  LogFileWrite("ERROR: VPP-/VCC Drop cannot be set in Port configuration");
     return FLASH_BAUDRATE_SET_FAILED;
  }

  if (PortReadDSR(pFlashPort))                            /* Check VCC-Drop */
     RetVal = FLASH_VCC_DROP;

  if (PortReadCTS(pFlashPort))                            /* Check VPP-Drop */
     RetVal = FLASH_VPP_DROP;

  return RetVal;
}

//---------------------------------------------------------------------------

ulword MausSendWaitAck(RS232Typ *pPort, unsigned char cmd,
                                        unsigned int XmLen,
                                        unsigned long ulTries,
                                        unsigned int uiMode)
{
  if (ChkMausSend(pFlashPort, cmd, ucBuffer, XmLen) == true)
  {  unsigned char ucLength = 255;

     ucBuffer[0] = 0;
     if (MausReceive (pFlashPort, 0x71, ucBuffer, &ucLength, DevParms.ulwPM_F9, uiMode) == true)
     {  if ((ucBuffer[1] == 0x01) && (ucBuffer[2]== 0x55))
           return FLASH_SUCCESS;                // Success !
                                                // command in progress ?
        if ((ucBuffer[1] == 0x01) && (ucBuffer[2] == 0x00))
        {  if (WaitChkMausSend (pFlashPort, cmd, (unsigned char*)"\x00", 1) == true)
           {  ucBuffer[0] = 0;
              ucLength = 255;
              if (MausReceive (pFlashPort, 0x71, ucBuffer, &ucLength, ulTries, uiMode) == true)
                 if (ucBuffer[1] == 0x01)
                 {  if (ucBuffer[2]== 0x55)
                       return FLASH_SUCCESS;    // Success

                    if (ucBuffer[2]== 0xAA)
                       return FLASH_NO_SUCCESS; // No Succes
                 }
           }
        }
     }
  } else ucBuffer[1] = 0;                   // destroy buffer for global error
  return Flash_ulwMausResult(0,0,0);
}
//---------------------------------------------------------------------------

bool WaitChkMausSend (RS232Typ *pPort, unsigned char cCommand,
                                   unsigned char *pcDataBuffer,
                                   unsigned char cLength)
{
  TimeWait(FlashParms.uwTelegramTimeOut*1000);      // first delay and the xmit
  return ChkMausSend(pPort, cCommand, pcDataBuffer, cLength);
}
//---------------------------------------------------------------------------

bool ChkMausSend (RS232Typ *pPort, unsigned char cCommand,
                                   unsigned char *pcDataBuffer,
                                   unsigned char cLength)
{
  if ( MausSend(pPort,cCommand,pcDataBuffer,cLength) == true )
     return true;

  bool retvalCts = !PortReadCTS(pFlashPort);
  bool retvalDsr = !PortReadDSR(pFlashPort);
  if (!retvalCts) LogFileWrite("No Success on ChkMausSend! (CTS is HIGH)");
  if (!retvalDsr) LogFileWrite("No Success on ChkMausSend! (DSR is HIGH)");
  return retvalCts && retvalDsr;
}
//---------------------------------------------------------------------------

bool SendFrame(char *pcBuffer, unsigned long lLength)
{ bool ok = false;

  for (ulword j=0;((j<DevParms.ulwPM_F9) && !ok);j++)
  {  PortSend(pFlashPort,(unsigned char*)pcBuffer,lLength); /* Send Frame ! */
     TimeWait(DevParms.ulwPM_F7);                               /* wait and */
     unsigned long ulLength = 12;
     memset(ucBuffer,0,10);                                /* get ACK-Frame */
     unsigned long result = PortReceive(pFlashPort, ucBuffer, &ulLength, kDebug);

     if ( !(ok = ((result == kOK) && (memcmp(ucBuffer,"\x2\x1\x6\xF9\x3",5) == 0))) )
     {  LogFileWrite("ERROR: No Ack after transmit the Intro-loader (%x)",ucBuffer[2]);
        TimeWait(10000);
     }
  }
  return ok;                /* return true, if receveid Ack-Data of 5 Bytes */
}
//---------------------------------------------------------------------------

bool SendFrameMsg(unsigned char Start, int len, unsigned char *pcBuffer, unsigned char End, int Ack, int Timeout)
{ bool ok = false;

  if ((ubDevice == DEVICE_V850E) || (ubDevice == DEVICE_V850E2M))
  {
     unsigned char MsgBuf[1030] = { 0 };
     unsigned char CS = 0;
     unsigned long idx = 0;
     int LenSize = 1;

     MsgBuf[0] = Start;
     if (ubDevice == DEVICE_V850E2M)
     {  MsgBuf[++idx] = (unsigned char) (len >> 8);
        LenSize = 2;
        CS = (unsigned char)(len>>8) + (unsigned char)len;
     }
     else CS = (unsigned char)len;

     MsgBuf[++idx] = (unsigned char) (len & 0xFF);

     for (int i=0;i<len;i++)
     {   CS += *(pcBuffer+i);
         MsgBuf[++idx] = *(pcBuffer+i);
     }
     MsgBuf[idx+1] = 0-CS;
     MsgBuf[idx+2] = End;

     unsigned long lLength = idx+3;

     for (ulword j=0;((j<DevParms.ulwPM_F9) && !ok);j++)
     {  PortSend(pFlashPort,MsgBuf,lLength);                /* Send Frame ! */
        TimeWait(DevParms.ulwPM_F7);                            /* wait and */
        int RespLen = (!Ack) ? 0 : Ack+LenSize+3;
        unsigned long result, ulLength = RespLen;
        if (EchoCom) ulLength += lLength;

        if (ulLength > sizeof(ucBuffer))
        {  LogFileWrite("Error. Protect to overwride too small buffer in SendFrameMsg");
           return false;
        }
        memset(ucBuffer,0,ulLength);                       /* get ACK-Frame */

        unsigned long MsgLen = ulLength;

        /* Receive until response received or Timeout */
        for (int i=0;i<Timeout;i++)
        {
          result = PortReceive(pFlashPort, ucBuffer, &ulLength, kDebug);
          TimeWait(DevParms.ulwPM_F7);        /* wait and */
          if (result == kOK)
          {  if (!Ack) return true;           /* Exit, wenn kein Ack benötigt */
             if (ulLength == MsgLen) break;   /* Ende wenn response gelesen */
             else MsgLen = RespLen;           /* Wenn echo da, dann Antwortlänge */
          }
        }

        if (result == kOK)
        {  if (!(ok = (ucBuffer[ulLength-Ack-2] == 0x6)))
           {  if (ucBuffer[ulLength-Ack-2] == 0x10)
              {  ProgressUpdate("Status Msg from V850 is Protect Error");
                 return false;
              }
              else ProgressUpdate("Status Msg from V850 is No Ack (%x)",ucBuffer[ulLength-Ack-2]);
           }
        }
        else ok = false;
        if (!ok) TimeWait(10000);
     }
  }
  else ProgressUpdate("Not supported device for SendFrameMsg");

  return ok;                /* return true, if receveid Ack-Data of 5 Bytes */
}
//---------------------------------------------------------------------------

ulword SendChipErase(void)
{  int retry = 2;
   ulword RetVal = FLASH_NO_SUCCESS;


   ProgressUpdate("Do Chip erase");

 while (retry != 0 && (RetVal == (ulword)FLASH_NO_SUCCESS))
 {
   if (blConnected)
   {
    ucBuffer[0] = 0x40;                               /* Chip erase command */
    ucBuffer[1] = 0x20;
    unsigned long ulTries = 1 + FlashParms.ubEraseTimeOut * 1000 / FlashParms.uwTelegramTimeOut;
    if (MausSendWaitAck(pFlashPort, 0x8F, 2, ulTries, kDebug|EchoCom) != FLASH_SUCCESS)
    {  LogFileWrite("ERROR: No response for Chip erase");
       if (--retry) ProgressUpdate("Retry");
    }
    else RetVal = FLASH_SUCCESS;
   }
   else
   {
     if (!SendFrameMsg(0x1,0x1,(unsigned char *)"\x20",3,1,40))
     {  LogFileWrite("ERROR: No response for Chip erase");
        if (--retry) ProgressUpdate("Retry");
     }
     else RetVal = FLASH_SUCCESS;
   }
 }
   if (RetVal == FLASH_SUCCESS)
   {  blErased = true;
      FX4Virgin = 7;                           /* set flags to virgin state */
   }
   return RetVal;
}
//---------------------------------------------------------------------------

ulword ReadPortSendResult(void)    /* Read result after sending Introloader */
{ unsigned char NOK = 0xFF;
  ulword err = FLASH_SUCCESS;

  LogFileWrite("ReadPortSendResult");
  if (PortReadOK(pFlashPort, 0x3c, (char*)&NOK) == false)
  {  if (NOK == 0x3D)
     {  LogFileWrite("ERROR: Checksum error during transmit the Intro-loader");
        err = FLASH_INTROLOADER_CS_ERROR;
     }
     else
     {  if (NOK != 0xFF)
           LogFileWrite("ERROR: Unresolved response from Intro-loader (0x%x),NOK");
        else LogFileWrite("ERROR: No response after transmit the Intro-loader");
        err = FLASH_INTROLOADER_FAILED;
     }
  }
  return err;
}
//---------------------------------------------------------------------------

unsigned long GetV850Info(char * Text, unsigned char code)
{  unsigned long ulLength = 100;

   if (!SendFrameMsg(0x1,0x1,&code))
   {  LogFileWrite("ERROR: No response for %s",Text);
      return FLASH_REJECT_OPTIONSETTINGS;
   }
   TimeWait(DevParms.ulwPM_01);    /* Recv Ack -> Send Data Wait */
   if (!SendFrameMsg(0x11,0x1,(unsigned char *)"\x06",0x3,0))
      return FLASH_REJECT_OPTIONSETTINGS;

   if (PortReceive(pFlashPort, ucBuffer, &ulLength, kDebug)  != kOK)
   {  LogFileWrite("ERROR: Reading of Info not successful for %s",Text);
      return FLASH_REJECT_OPTIONSETTINGS;
   }

   /* check status */
   unsigned char aubVirgin[14] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00  };
   struct
   {  unsigned char code;
      int anz;
      ubyte *DefData;
      ubyte *VirData;
   } strInfodata[3] = { {0xA1, 6, DevParms.aubSysSec, aubVirgin+11 }, {0xA7, 12,DevParms.aubSysOCD, aubVirgin}, {0xAA, 4,DevParms.aubSysOpt, aubVirgin} };

   int idx = 2;
   while (idx > 0)
   {   if (code != strInfodata[idx].code)
          idx --;
       else break;
   }
   // OCD will no more be checked. Virgin and default will be set
   if ((memcmp(strInfodata[idx].VirData,&ucBuffer[3],3) == 0) || (code == 0xA7))
      FX4Virgin |= (1 << idx);
   if ((memcmp(strInfodata[idx].DefData,&ucBuffer[3],strInfodata[idx].anz) == 0) || (code == 0xA7))
      FX4Default |= (1 << idx);


   char buffer[255] = " -> Data: ";
   char help[4];
   for (unsigned long i=0; i<ulLength - 5; i++)
   {   sprintf(help,"%02X ",ucBuffer[3+i]);
       strcat (buffer,help);
   }
   strcat((char*)OptLog,(const char*)Text);
   strcat((char*)OptLog,(const char*)buffer);
   strcat((char*)OptLog,"\n");
   LogFileWrite("%s%s",Text,buffer);
   return FLASH_SUCCESS;
}
//---------------------------------------------------------------------------

unsigned long SetV850Info(unsigned char code)
{  ubyte *data = NULL;
   char Text[255] = {0};
   int Anz;

   switch (code)
   {
     case 0xA6:
                strcpy(Text,"Set OCD_ID");
                Anz = 12;
                data = DevParms.aubSysOCD;
                break;
     case 0xA0:
                strcpy(Text,"Set Security flags");
                Anz = 6;
                data = DevParms.aubSysSec;
                break;
     case 0xA9:
                strcpy(Text,"Set Option Bytes");
                Anz = 4;
                data = DevParms.aubSysOpt;
     default:   break;
   }

   if (data)
   { ProgressUpdate(Text);
     if (!SendFrameMsg(0x1,0x1,&code))
     {  LogFileWrite("ERROR: No response for %s",Text);
        return FLASH_NO_SUCCESS;
     }
     if (!SendFrameMsg(0x11,Anz,data))
     {  LogFileWrite("ERROR: No response for %s",Text);
        return FLASH_NO_SUCCESS;
     }
   }
   return FLASH_SUCCESS;
}
//---------------------------------------------------------------------------

char *GetTimeDifference(SYSTEMTIME t0)
{
  SYSTEMTIME t1;
  static char buffer[50];

  GetSystemTime(&t1);

  #ifdef LINUX
  time_t Sec = t1.tv_sec-t0.tv_sec;
  long mSec;
  int Min = 0;

  if (t1.tv_usec < t0.tv_usec)
  {  Sec--;
     mSec = t1.tv_usec + 1000000 - t0.tv_usec;
  }
  else mSec = t1.tv_usec - t0.tv_usec;

  if (Sec>60)
  {  Min = Sec/60;
     Sec = Sec - (60*Min);
  }

  sprintf(buffer,"%02d ' %02d \" %03d",Min,(int)Sec,(int)mSec);
  #else
  if ((t1.wHour >= 24) || (t1.wMinute >= 60) || (t1.wSecond >= 60))
  {  LogFileWrite("Error in GetSystemTime: t1.wHour = %d, t1.wMinute = %d, t1.wSecond = %d, t1.wMilliseconds = %d",
                  t1.wHour, t1.wMinute, t1.wSecond, t1.wMilliseconds);
     GetSystemTime(&t1);
     if ((t1.wHour >= 24) || (t1.wMinute >= 60) || (t1.wSecond >= 60))
     {  LogFileWrite("Schon wieder Error in GetSystemTime: t1.wHour = %d, t1.wMinute = %d, t1.wSecond = %d, t1.wMilliseconds = %d",
                     t1.wHour, t1.wMinute, t1.wSecond, t1.wMilliseconds);
        GetSystemTime(&t1);
        if ((t1.wHour >= 24) || (t1.wMinute >= 60) || (t1.wSecond >= 60))
        {  LogFileWrite("Und noch mal Error in GetSystemTime: t1.wHour = %d, t1.wMinute = %d, t1.wSecond = %d, t1.wMilliseconds = %d",
                        t1.wHour, t1.wMinute, t1.wSecond, t1.wMilliseconds);
        }
     }
  }

  if (t0.wMilliseconds > t1.wMilliseconds)
  {  t1.wMilliseconds += 1000;
     t1.wSecond--;
  }
  if (t0.wSecond > t1.wSecond)
  {  t1.wSecond += 60;
     t1.wMinute--;
  }
  if (t0.wMinute > t1.wMinute)
  {  t1.wMinute += 60;
     t1.wHour--;
  }
  int Min = t1.wMinute - t0.wMinute;
  int Sec = t1.wSecond - t0.wSecond;
  int mSec = t1.wMilliseconds - t0.wMilliseconds;
  sprintf(buffer,"%02d ' %02d \" %03d",Min,Sec,mSec);
  if ((Min >= 60) || (Sec >= 60) || (mSec >= 1000))
  {  LogFileWrite("Error in GetSystemTime: t0.wHour = %d, t0.wMinute = %d, t0.wSecond = %d, t0.wMilliseconds = %d, t1.wHour = %d, t1.wMinute = %d, t1.wSecond = %d, t1.wMilliseconds = %d",
                  t0.wHour, t0.wMinute, t0.wSecond, t0.wMilliseconds, t1.wHour, t1.wMinute, t1.wSecond, t1.wMilliseconds);
  }
  #endif
  return buffer;
}
//---------------------------------------------------------------------------

unsigned char *ConvertSignature(unsigned char *DeviceInfo, unsigned char *Buffer)
{ int i,j,k;

  for (i = 0; i < 10; i++)                                    /* find start */
      if ( Buffer[i] != ' ' ) break;

  for (j = 9; j; j--)                                           /* find end */
      if ((Buffer[j] != ' ') && (Buffer[j] != 0)) break;

  for (k = 0; k < 10; k++)              /* Remove spaces from begin and end */
  {   if (k<=(j-i)) DeviceInfo[k] = Buffer[k+i] & 0x7f;     /* Remove Bit 7 */
      else DeviceInfo[k] = 0;
  }
  return DeviceInfo;
}
//---------------------------------------------------------------------------

unsigned char DecodeDevice(char *Signature, int *iDerivat)
{ char *Type;
  ubyte ubRetVal =  DEVICE_UNKNOWN;

                                                  /* Jetzt Device bestimmen */
  for (int i=0;i<MAX_DEVICE_TYPES;i++)
  {
      if (strstr(cstDevice[i].Name,Signature))
      {  ubRetVal = cstDevice[i].Type;
         if (iDerivat) *iDerivat = cstDevice[i].Derivat;
      }
  }


  switch (ubRetVal)
  {
    case     DEVICE_V850E:
             Type = (char*)"DEVICE_V850E";
             IntParms.ulwMode = 20;
             break;
    case     DEVICE_V850E2M:
             Type = (char*)"DEVICE_V850E2M";
             IntParms.ulwMode = 30;
             break;
    case     DEVICE_TUAREG:
             Type = (char*)"TUAREG";
             IntParms.ulwMode = 10;
             break;
    case     DEVICE_UNKNOWN:
             ProgressUpdate("WARNING !!!!!!!!!!!!!! - Unknown device found. Use default values. Result is random !");
    default: ubRetVal = DEVICE_V850;               /* Default = DEVICE_V850 */

             Type = (char*)"DEVICE_V850";
             IntParms.ulwMode = 0;
             break;
  }
  LogFileWrite("Found Type %s of %s",Type,Signature);

  return ubRetVal;
}
//---------------------------------------------------------------------------

bool bCheckCompatibility(void)
{ bool retval = true;
  ubyte buf[256];

  if (KDS_Offset)
  {  LogFileWrite("Read CompatibilityNr from KDS");
     if (MausSend(pFlashPort, 0x94,(unsigned char*)"\x77\x12\x0\x03\x0\x20\x0\x0A", 8) == true)
     {  unsigned char ucLength = 255;

        ucBuffer[0] = 0;
        if (MausReceive (pFlashPort, 0x95, ucBuffer, &ucLength, 3, kDebug|EchoCom) == true)
        {  if (ucBuffer[2] == 0x55)        // Achtung: C-Nr steht verkehrt herum im KDS !!
           {  int idx = 12;                // Nur weil man früher alles ver-
              for (int i=0; i<5;i++)       // kompliziert hat: wieder wandeln
                  sprintf((char*)&(buf[i*2]),"%02x",ucBuffer[idx--]);
              for (int i=0; i<5;i++)
                  buf[10+i] = ucBuffer[idx--];
              if (memcmp(buf,aubComNr,15) != 0)
              {
                 retval = false;
              }
           }
           else retval = false;
        } else retval = false;
     } else retval = false;
  }
  else
  { unsigned char *ptr = NULL;
    LogFileWrite("Read CompatibilityNr from EEP");
    if (Flash_blReadErrHistory(buf,&ptr) && (memcmp(buf+31,aubComNr,15) != 0))
       retval = false;
    if (ptr != NULL) delete [] ptr;
  }

  #if 1
  LogFileWrite("Compare Result:");
  char Help[40] = { 0 };
  sprintf(Help,"KDS: ");
  for (int i=0;i<15;i++)
  {   char H[5];
      sprintf(H,"%02x",buf[i]);
      strcat(Help,H);
  }
  LogFileWrite("%s",Help);
  sprintf(Help,"DNL: ");
  for (int i=0;i<15;i++)
  {   char H[5];
      sprintf(H,"%02x",aubComNr[i]);
      strcat(Help,H);
  }
  LogFileWrite("%s",Help);
  #endif

  return retval;
}
//---------------------------------------------------------------------------

bool bCheckValidBlock(uword Typ)
{ bool RetVal = false;                              // default: nicht gefunden

  if (BlockList != NULL)                            // raus, falls keine Liste
  {  uword *Liste = BlockList;                      // vorhanden !
     do
     { if (Typ == *Liste)
          RetVal = true;                            // Block ist gefunden !
       else                                         // sonst
       {                                            // falls
          if ( (*Liste == 0xffff) ||                // Alle Blöcke sind erlaubt
               (    ((*Liste & 0xFF) == 0xff)       // oder alle Blöcke des De-
                 && (((*Liste ^ Typ) & 0xFF00) == 0)// vice sind erlaubt !
               )
             )
      {                                             // dann Update prüfen:
             if (blUpdate)                          // Im Update-Fall nur gerade
                RetVal = ((Typ & 0x1) == 0);        // Blocknummern zulassen
             else RetVal = true;
      }
       }
       Liste++;
     } while (*Liste && !RetVal);
  }
  return RetVal;
}
//---------------------------------------------------------------------------

ulword ulwGetDeviceSize(unsigned char ucMemType)
{ ulword ulwSize = 0;

  for (int i = 0; i < DFInfo.Count; i++)
  {   if (ucMemType == (DFInfo.Header[i].Typ >> 8))
         ulwSize += DFInfo.Header[i].Length;
  }
  return ulwSize;
}
//---------------------------------------------------------------------------

ulword SetBlindBuf(ulword Size)
{
  ulword ModVal = SX2 ? 48 : 12;

  /***********Debug Test ********/
  if (Size < 9)
     LogFileWrite("3 - Blindbuf is now too small %d. MaxBlindbuf-value = %d" , Size,DevParms.ulwMaxBlindBuf);
  /***********Debug Test ********/


  if (Size < ModVal)              // Das sollte möglichst nie passieren
     Size = ModVal;
  else                            // sonst
  {                               // BlindRom-Buffer darf nie größer als
    if (Size > (ulword)FlashParms.ubTelegramLength)  // Telegrammlänge
       Size = FlashParms.ubTelegramLength;           // und
    else while ((Size % ModVal) != 0) Size--;        // gemeinsam erlaubten
  }                                                  // Vielfachen sein
  return Size;
}
//---------------------------------------------------------------------------

ulword CreateKDSDataMemBlock()
{ ulword RetVal = FLASH_SUCCESS;
  unsigned char *KDS_Buffer = NULL;

  LogFileWrite("Entered CreateKDSDataMemBlock()");
  if (KDS_Offset && !blDebug)
  { unsigned char *ptr = NULL,*ptrKDS;

    if (DFInfo_blReadData(IntParms.f_DFile,&DFInfo,&ptr, 0x8000))
    { ptrKDS = ptr + KDS_Offset + 1 + 2;

      //ubyte Vers = *(ptrKDS++);            // Wird vorerst noch nicht benötigt
      //ubyte SubVers = *(ptrKDS++);
      unsigned long StartAddr = (*(ptrKDS) << 24) + (*(ptrKDS+1) << 16) +
                                (*(ptrKDS+2) << 8) + *(ptrKDS+3);
      unsigned long Size      = (*(ptrKDS+4) << 24) + (*(ptrKDS+5) << 16) +
                                (*(ptrKDS+6) << 8) + *(ptrKDS+7);
                                  /* Buffer für zu flashende Daten anfordern */

      if ((KDS_Buffer = (unsigned char*)malloc(Size)) != NULL)
      {
        unsigned char *src = ptrKDS+8;
        unsigned char *dest =&KDS_Buffer[Size-1];
        memset (KDS_Buffer,0xff,Size);          /* Init KDS-Buffer with 0xff */

        do                                  /* Daten aus KDS-Block rückwärts */
        { *dest = *src;                           /* an das Ende des Buffers */
          dest--;                                               /* schreiben */
          src++;
        } while ((memcmp(src,"\x55\xFA\xAF\xFA\xAF\x55",6) != 0) && (dest >= &KDS_Buffer[0]));

        /* If exchange Fkt for KDS-data, then overwrite buffer */
        if (IntParms.pKDSXchg)
           RetVal = IntParms.pKDSXchg((char*)KDS_Buffer,false);

        if (RetVal == FLASH_SUCCESS)
           RetVal = Flash_ulwErase(0x40,StartAddr,Size,0);

        if (RetVal == FLASH_SUCCESS)
        {  RetVal = Flash_ulwWrite(0x40,KDS_Buffer,StartAddr,Size);

           #if 0
           if (RetVal == FLASH_SUCCESS)
           {  if (FlashParms.ubVerify && !blAbort)
              {  ulword CRC, CCS = DNL_CRC_DEFAULT_CRC;
                 for (unsigned int j=0;j<Size;j++)
                     CCS = Flash_ulwBPCRC(KDS_Buffer[j],CCS,DNL_CRC_GREEN_POLYNOM);

                 ProgressUpdate("Start reading CRC from device %X, Adr: %08X-%08X",0x40,StartAddr,StartAddr+Size);
                 if ((RetVal = Flash_uiGetCRC(0x40,StartAddr,Size,(unsigned int*)&CRC)) == FLASH_SUCCESS)
                    if (CRC != CCS)
                    {  ProgressUpdate("Expected value for block 0x40ED is: %08X",CCS);
                       RetVal = FLASH_DF_FILE_CRC_ERROR;
                    }
              } /* else no verify */
           } else LogFileWrite("Error in Flash_ulwWrite detected");
           #else
           if (RetVal != FLASH_SUCCESS)
              LogFileWrite("Error in Flash_ulwWrite detected");
           #endif
        } else LogFileWrite("Error in Flash_ulwErase detected");
                                   /* Buffer mit Flashdaten wieder freigeben */
        free(KDS_Buffer);
      }
      else RetVal = FLASH_NO_SUCCESS;                    /* if (KDS_Buffer) */

    } /* if (DFInfo_blReadData) */
    else RetVal = FLASH_NO_SUCCESS;

    if (ptr != NULL) delete [] ptr;

  } /* if (KDS_Offset && !blDebug) */

  LogFileWrite("Exit CreateKDSDataMemBlock()");
  return RetVal;
}
//---------------------------------------------------------------------------

static const int cSize[] = { 1, 2, 4, 2, 4, 1, 2, 4, 2, 4 };

ulword CreateConfigDataMemBlock(bool blUpdate)
{ ulword RetVal = FLASH_SUCCESS;
  unsigned char *CDMB_Buffer = NULL;

  LogFileWrite("Entered CreateConfigDataMemBlock()");
  if (CDMB_Offset && !blDebug)
  { unsigned char *ptr = NULL,*ptrCDMB;

    if (DFInfo_blReadData(IntParms.f_DFile,&DFInfo,&ptr, 0x8000))
    { ptrCDMB = ptr + CDMB_Offset + 1;

      //ubyte Vers = *(ptrCDMB++);            // Wird vorerst noch nicht benötigt
      //ubyte SubVers = *(ptrCDMB++);
      unsigned long StartAddr = (*(ptrCDMB) << 24) + (*(ptrCDMB+1) << 16) +
                                (*(ptrCDMB+2) << 8) + *(ptrCDMB+3);
      unsigned long Size      = (*(ptrCDMB+4) << 24) + (*(ptrCDMB+5) << 16) +
                                (*(ptrCDMB+6) << 8) + *(ptrCDMB+7);
      ptrCDMB += 8;
                                  /* Buffer für zu flashende Daten anfordern */
      CDMB_Buffer = (unsigned char*)malloc(Size);

      if (CDMB_Buffer)
      { unsigned char *CRCPtr = CDMB_Buffer;    /* StartPtr für CRC-Berechn. */
        unsigned char *DatPtr = CDMB_Buffer;    /* Pointer für Bufferzugriff */

        memset(CDMB_Buffer,0xfe,Size);

        while ((*ptrCDMB != 0xff) && (*(ptrCDMB+1) != 0xff) && (RetVal == FLASH_SUCCESS))
        { unsigned int VarTyp     = (*(ptrCDMB) << 8) + *(ptrCDMB+1);
          unsigned int VarSize    = (*(ptrCDMB+2) << 8) + *(ptrCDMB+3);
          unsigned int VarNameLen = strlen((char*)(ptrCDMB+4));
          unsigned int DataLen    = (VarSize * cSize[VarTyp-1]);
          unsigned int VarLen     = 7 + VarNameLen + DataLen;
          unsigned int VarAction  = (*(ptrCDMB+VarNameLen+5) << 8) + *(ptrCDMB+VarNameLen+6);
          unsigned char *VarBuf   = (unsigned char*)malloc(VarLen);
          unsigned char *VarData  = VarBuf + 7 + VarNameLen;

          if (VarBuf)
          {  memcpy((void*)VarBuf,(const void*)ptrCDMB,VarLen);

             if ((VarTyp >= 1) && (VarTyp <= 0x0A) && (VarAction <= 5))
             {
                                               /* Solldaten lesen oder holen */
               switch (VarAction)
               {
                 case 0x0000:                                         // Const
                      break;
                 case 0x0001:                                         // Get Random
                 case 0x0002:                                         // Get Fixed
                      if (IntParms.pCDMBXchg)
                         RetVal = IntParms.pCDMBXchg((char*)VarBuf,false);
                      break;
                 case 0x0003:                                         // CRC32
                 case 0x0004:                                         // CRC_VAG
                 case 0x0005:                                         // CRC_CDMB
                 {    unsigned char *pubPtr = (VarAction == 3) ? CRCPtr : CDMB_Buffer;
                      unsigned int Len = DatPtr-pubPtr;
                      unsigned int (*CRCVec)(unsigned char ubData, unsigned int ulwPrevCRC, unsigned int Polynom);
                      unsigned int ulwPoly = (*(VarData) << 24) + (*(VarData+1) << 16) +
                                             (*(VarData+2) << 8) + *(VarData+3);
                      CRCVec = (VarAction == 4) ? Flash_ulwVAGCRC : Flash_ulwBPCRC;

                      ulword CCS = DNL_CRC_DEFAULT_CRC;
                      for (unsigned int j=0;j<Len;j++)
                          CCS = (CRCVec)(*pubPtr++,CCS,ulwPoly);

                      if ((VarTyp == 3) || (VarTyp == 8))
                      {  *(VarData)   = (unsigned char)(CCS >> 24);
                         *(VarData+1) = (unsigned char)(CCS >> 16);
                         *(VarData+2) = (unsigned char)(CCS >> 8);
                         *(VarData+3) = (unsigned char)(CCS);
                      }
                      else if ((VarTyp == 5) || (VarTyp == 10))
                      {  *(VarData)   = (unsigned char)(CCS);
                         *(VarData+1) = (unsigned char)(CCS >> 8);
                         *(VarData+2) = (unsigned char)(CCS >> 16);
                         *(VarData+3) = (unsigned char)(CCS >> 24);
                      }
                      else RetVal = FLASH_NO_SUCCESS;

                      CRCPtr = DatPtr+4;
                      break;
                 }
                 default: RetVal = FLASH_NO_SUCCESS;
               } /* switch VarAction */

               memcpy((void*)DatPtr,(const void*)VarData,DataLen);
             }
             else RetVal = FLASH_NO_SUCCESS;            /* Falsche Parameter */

             free(VarBuf);                      /* Speicher wieder freigeben */

             DatPtr += DataLen;                        /* Nächste Variable ! */
             ptrCDMB += VarLen;
          }
          else RetVal = FLASH_NO_SUCCESS;   /* Systemfehler: Kein Speicher ! */
        } /* while () */

        if ((RetVal == FLASH_SUCCESS) && blUpdate)
           RetVal = Flash_ulwErase(0x40,StartAddr,Size,0);

        if (RetVal == FLASH_SUCCESS)
        {  RetVal = Flash_ulwWrite(0x40,CDMB_Buffer,StartAddr,Size);

           #if 0
           /* Test */
           LogFileWrite("Data of CDMB:");
           AnsiString H = "";
           for (unsigned int j=0;j<Size;j++)
           {   H = H+IntToHex((__int64)CDMB_Buffer[j],2);
               if (((j+1)%32) == 0) {LogFileWrite(H.c_str()); H=""; }
           }
           if (H!= "") LogFileWrite(H.c_str());
           /* Ende Test */
           #endif

           if ((RetVal == FLASH_SUCCESS) && blUpdate)
           {  if (FlashParms.ubVerify && !blAbort)
              {  ulword CRC, CCS = DNL_CRC_DEFAULT_CRC;
                 for (unsigned int j=0;j<Size;j++)
                     CCS = Flash_ulwBPCRC(CDMB_Buffer[j],CCS,DNL_CRC_GREEN_POLYNOM);

                 ProgressUpdate("Start reading CRC from device %X, Adr: %08X-%08X",0x40,StartAddr,StartAddr+Size);
                 if ((RetVal = Flash_uiGetCRC(0x40,StartAddr,Size,(unsigned int*)&CRC)) == FLASH_SUCCESS)
                    if (CRC != CCS)
                    {  ProgressUpdate("Expected value for block 0x40EF is: %08X",CCS);
                       RetVal = FLASH_DF_FILE_CRC_ERROR;
                    }
              }
           } /* else no verify */
        } else LogFileWrite("Error in Flash_ulwErase detected");
                                   /* Buffer mit Flashdaten wieder freigeben */
        free(CDMB_Buffer);
      }
      else RetVal = FLASH_NO_SUCCESS;                    /* if (CDMB_Buffer) */
    } /* if (CDMB_Ptr) */
    else RetVal = FLASH_NO_SUCCESS;

    if (ptr != NULL) delete [] ptr;
  } /* if (CDMB_Offset && !blDebug) */

  LogFileWrite("Exit CreateConfigDataMemBlock()");
  return RetVal;
}
//---------------------------------------------------------------------------
#define FLASH_GETVALUE_32(p)  (*(p+3) << 24) + (*(p+2) << 16) + (*(p+1) <<  8) + *(p);
ulword TranslateFGSBlk(unsigned char *Ptr)
{
  ulword ulwID = FLASH_GETVALUE_32(Ptr+4);  // Convert Seg-ID to Block-ID
  if (ulwID == 0) ulwID = 0x4B03;           // 0 = Downloader
  else if (ulwID == 1) ulwID = 0x4B04;      // 1 = Testmanager
  else if (ulwID == 2) ulwID = 0x4B06;      // 2 = Normal Application
  else if (ulwID == 0x10000001) ulwID = 0x4BFE;  // Manufacturer data
  else ulwID = 0x0;                         // tbd. or undefined
                                            // See. FGS_DNL_Communication.doc
  return ulwID;
}
//---------------------------------------------------------------------------
const ubyte aubFDSEmpty[] = { 0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
                              0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
                              0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
                              0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff };

ulword FGS_ulwDownload(ulword ulwMode)
{ ulword ulwStart = DFInfo_ulwGetStart(&DFInfo,0x4B02);
  ulword ulwLen = DFInfo_ulwGetLen(&DFInfo,0x4B02);
  ulword RetVal = FLASH_NO_SUCCESS;
  unsigned char *OldFDS = new ubyte [ulwLen];
  unsigned char *NewFDS = NULL;

  LogFileWrite("Entered FGS-Download");

  if (OldFDS)
  { if (DFInfo_blReadData(IntParms.f_DFile,&DFInfo,&NewFDS, 0x4B02))
    {  ulword ulwFDSAdr = ulwStart;
       ubyte *OldPtr = OldFDS-32, *NewPtr;
       unsigned char *OldEnd = OldFDS + ulwLen;

       memset((void*)OldFDS,0xff,ulwLen);   // Alles löschen: für späteren CRC

       do                                   // Alten FDS einlesen
       { OldPtr+=32;
         RetVal = Flash_uiRead(0x4B, ulwFDSAdr, 32, OldPtr);
         ulwFDSAdr+=32;
       } while ((RetVal == FLASH_SUCCESS) && (memcmp(OldPtr,aubFDSEmpty,32) != 0) && (OldPtr < (OldEnd-32)));

       if (RetVal == FLASH_SUCCESS)         // nur weiterarbeiten, wenn alles
       {                                    // fehlerfrei gelesen wurde

         /* Zuerst auf partielles Update und Größenänderungen prüfen ! */
         /* ========================================================== */
         ulword ulwTyp;
         bool blPartUpd = false, blConfErr = false, blErr = false;
         unsigned char *NewEnd = NewFDS + ulwLen;

         OldPtr = OldFDS;
         NewPtr = NewFDS;
         ulword ulwFDSAdr = ulwStart;

         while (((memcmp(OldPtr,aubFDSEmpty,32) != 0)  ||         // FDS-Ende ?
                 (memcmp(NewPtr,aubFDSEmpty,32) != 0)) &&
                (NewPtr < NewEnd))
         {  if ((ulwTyp = TranslateFGSBlk(OldPtr)) == 0)
               ulwTyp = TranslateFGSBlk(NewPtr);

            if (bCheckValidBlock(ulwTyp))   // Soll Block programmiert werden ?
            {  ulword ulwEnd     = DFInfo_ulwGetStart(&DFInfo,ulwTyp)+DFInfo_ulwGetLen(&DFInfo,ulwTyp)-1;
               ulword ulwTypNext = TranslateFGSBlk(OldPtr+32);
               ulword ulwStNext  = FLASH_GETVALUE_32(OldPtr+32+16);
               if (ulwTypNext == 0)
               {  ulwTypNext = TranslateFGSBlk(NewPtr+32);
                  ulwStNext = FLASH_GETVALUE_32(NewPtr+32+16);
               }
                                              // ist die Blockgröße plausibel ?
               if (!bCheckValidBlock(ulwTyp) && (ulwEnd >= ulwStNext))
               {
                 blConfErr = true;
               }
            }
            else
            {
               if ((TranslateFGSBlk(OldPtr) == 0) && (ulwTyp != 0x4BFE))
               {                        // Es gibt offensichtlich neue Blöcke,
                  blErr = true;  // die aber nicht programmiert werden sollen !
               }
               blPartUpd = true;
            }
            ulwFDSAdr += 32;
            OldPtr += 32;                       // nächsten Eintrag untersuchen
            NewPtr += 32;
         } /* while check config */

         /* Wenn keine Größenänderungen oder kein partielles Update: */
         /* dann kann programmiert werden.                           */
         /* ======================================================== */
         if ((!blPartUpd || !blConfErr) && !blErr)
         { SYSTEMTIME t0;
                                                // FDS default: Löschen
           RetVal = Flash_ulwErase(0x4B,ulwStart,ulwLen,0);
           //ProgressUpdate("Erase complete FDS");
           if (RetVal == FLASH_SUCCESS)
           {  ulwFDSAdr = ulwStart;
              OldPtr = OldFDS;
              NewPtr = NewFDS;
              GetSystemTime(&t0);

              while ((memcmp(OldPtr,aubFDSEmpty,32) != 0) && (OldPtr <= OldEnd) && (RetVal == FLASH_SUCCESS))
              {  // Sofort zurückschreiben, wenn Block nicht programmiert werden soll
                 if (!bCheckValidBlock(TranslateFGSBlk(OldPtr)))
                    RetVal = Flash_ulwWrite(0x4B,OldPtr,ulwFDSAdr,32);

                 ulwFDSAdr += 32;
                 OldPtr += 32;                 // nächsten Eintrag untersuchen
              } /* while check config */

              ulwFDSAdr = ulwStart;
              OldPtr = OldFDS;

              while (((memcmp(OldPtr,aubFDSEmpty,32) != 0)  ||    // FDS-Ende ?
                      (memcmp(NewPtr,aubFDSEmpty,32) != 0)) &&
                     (NewPtr <= NewEnd)                     &&
                     (RetVal == FLASH_SUCCESS)
                    )
              {  if ((ulwTyp = TranslateFGSBlk(OldPtr)) == 0)
                     ulwTyp = TranslateFGSBlk(NewPtr);

                 if (bCheckValidBlock(ulwTyp))
                 {  SYSTEMTIME t2;
                    GetSystemTime(&t2);

                    RetVal = Flash_ulwDownload(ulwTyp,0);
                    if (RetVal == FLASH_SUCCESS)
                    {  memcpy(OldPtr,NewPtr,32);
                       ProgressUpdate("Download of Block %04X successfull in %s",ulwTyp,GetTimeDifference(t2));
                    }
                    else
                    {
                      if (!(ulwMode & FLASH_MODE_HISTORY))
                         Flash_vWriteErrHistory(ulwTyp,RetVal); // write error history
                      ProgressUpdate("ERROR: %x in Typ : %x during Flash_ulwDownload",RetVal,ulwTyp);
                    }
                    Flash_ulwWrite(0x4B,OldPtr,ulwFDSAdr,32);
                 }

                 ulwFDSAdr += 32;
                 OldPtr += 32;
                 NewPtr += 32;
              }
              if (RetVal == FLASH_SUCCESS)             // CRC vom FDS prüfen
              {  ulword CCS = DNL_CRC_DEFAULT_CRC, CRC;

                 for (unsigned int j=0;j<ulwLen;j++)
                     CCS = Flash_ulwGetCRC(*(OldFDS+j),CCS);

                 ProgressUpdate("Start reading CRC from device 0x4B02, Adr: %08X-%08X",ulwStart,ulwStart+ulwLen);
                 if ((RetVal = Flash_uiGetCRC(0x4B,ulwStart,ulwLen,(unsigned int*)&CRC)) == FLASH_SUCCESS)
                    if (CRC != CCS)
                    {  ProgressUpdate("Expected value for block 0x4B02 is: %08X",CCS);
                       RetVal = FLASH_DF_FILE_CRC_ERROR;
                    }
              }
           } /* Erase FDS */
           if (RetVal == FLASH_SUCCESS)
              ProgressUpdate("Device successful programmed in %s",GetTimeDifference(t0));
         }
         else
         {  ProgressUpdate("Error during FGS-Configuration. No partial Update possible !");
            RetVal = FLASH_NO_SUCCESS;
         }
      } /* ReadMemory */
    } /* DFInfo_blReadData() */
    if (OldFDS != NULL) delete [] OldFDS;
    if (NewFDS != NULL) delete [] NewFDS;
  } /* if (OldData) */
  LogFileWrite("Exit FGS-Download");
  return RetVal;
}
//---------------------------------------------------------------------------

void CopyU32ToBuf(unsigned char *ptr, unsigned long value)
{   *ptr = (unsigned char)(value>>24);
    *(ptr+1) = (unsigned char)(value>>16);
    *(ptr+2) = (unsigned char)(value>>8);
    *(ptr+3) = (unsigned char)value;
}
//---------------------------------------------------------------------------

const char* toUpper(const char* str)
{
    for (char* cp = const_cast<char*>(str); char c = *cp; cp++)
    {
        if (c >= 'a' && c <= 'z')
            *cp = c - 'a' + 'A';
    }
    return str;
}
//---------------------------------------------------------------------------

ulword GetMyKDSData(char *ptrKDS, bool blDummy)
{
  memcpy(ptrKDS,KDS_Buf,KDS_Size);
  return FLASH_SUCCESS;
}

#undef FLASH_CPP

/********************************************************** END OF FILE *****/


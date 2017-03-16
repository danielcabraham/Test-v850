//------------------------------------------------------------------------------
// MausBus.cpp:
//    MausBus protocol
//
// Copyright:
//    (C) Blaupunkt, 2001 all rights reserved
//
// Programmer:
//    Michael Langer - BPW/TEF71
//
// Version:
//    0.11
//
// LastChange:
//    11.06.2001
//
// depend on:  Serial.cpp
//
// CPU:         all
//
// Compiler:
//    Inprise C++ Builder 5.0
//
//------------------------------------------------------------------------------
// History:
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//--------------- begin definition and includes --------------------------------
//------------------------------------------------------------------------------
//--------------- Compiler/OS depending includes -------------------------------
#include <stdio.h>

//--------------- Modul depending includes -------------------------------------
#ifdef FTDI_SUPPORT
  #include "usbserial.h"
#else
  #include "serial.h"
#endif
#include "Timing.h"
#include "MausBus.h"
#include "globals.h"
#include "Flash850.h"
#include "flash.h"

extern Flash_TYInternalParms IntParms;
extern void LogFileWrite(const char* arg_list, ...);

//--------------- defines ------------------------------------------------------
#define kuCACK 0x55
#define kuCNACK 0xaa
#define kuCREJEKT 0x09

#define kPCACK "\x5d\x00\x5d"
#define kPCNACK  "\xa2\x00\xa2"
#define kPCNACKLength  0x03

extern bool blAbort;

//--------------- Modul Global Variables ---------------------------------------
unsigned char gcMausTransmitBuffer[259];
unsigned char gcMausTransmitLength = 0;

//--------------- local Prototypes ---------------------------------------------
unsigned char GetCheckSum ( const unsigned char *pucData, const unsigned long ulLength, unsigned char ucCheckSumme );

//unsigned char GetCheckSum(unsigned char* pcData,  unsigned int iLength, unsigned char cPreInit);

//------------------------------------------------------------------------------
//--------------- end definition and includes ----------------------------------
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//----------- begin of Modul routines ------------------------------------------
//------------------------------------------------------------------------------
bool MausSend (RS232Typ *pPort, unsigned char cCommand,
                                unsigned char *pcDataBuffer,
                                unsigned char cLength)
{
  gcMausTransmitBuffer[0] = cCommand;
  gcMausTransmitBuffer[1] = cLength;

  if (pcDataBuffer != NULL)
     memcpy(&gcMausTransmitBuffer[2], pcDataBuffer, cLength);

  gcMausTransmitBuffer[cLength + 2] = GetCheckSum(gcMausTransmitBuffer, cLength + 2, 0x00);
  gcMausTransmitLength              = cLength + 3;

  return ( PortSend(pPort, gcMausTransmitBuffer, gcMausTransmitLength) == (unsigned long)gcMausTransmitLength );  //Langer
}
//------------------------------------------------------------------------------

bool MausReceive (RS232Typ *pPort, unsigned char cCommand,
                                   unsigned char *pucReceiveBuffer,
                                   unsigned char *pucReceiveBufferLength,
                                   unsigned long ulTrys,
                                   unsigned int uiMode)
{ char ErrText[3][10] = { "Timeout","PowerFail","ReadError" };
  unsigned long li;
  unsigned int  uiM = uiMode;
  unsigned long ulReceiveBufferLength;
  bool bDone;

  for (li = 0, bDone = false; bDone == false; li++)
  {
    if (blAbort) return true;
    if (li > ulTrys)
    {  LogFileWrite("No Success on MausReceive ! (retry overflow) ");
       return false;                         // after max trys
    }
                                             // if repeat, then
    uiMode = uiM;                            // start always with wanted mode

R0:

    ulReceiveBufferLength = 2L;
    int err = PortReceive ( pPort, pucReceiveBuffer, &ulReceiveBufferLength, ( uiMode | kClearDebugBuffer ) & ~kDebugOut ); /* Header nicht anzeigen */


    if (err != kOK)
       LogFileWrite("ERROR: -1- Portreceive mit Fehler (%s)",ErrText[err-1]);

    switch ( err )
    {
      case kOK :
           ulReceiveBufferLength = (*pucReceiveBufferLength > pucReceiveBuffer[1]) ? pucReceiveBuffer[1] : *pucReceiveBufferLength;
           break;
      case kTimeout :
      case kReadError :
           if (li < ulTrys)
           {  PortSend ( pPort, gcMausTransmitBuffer, gcMausTransmitLength);
              if (IntParms.pProgressInfo != NULL)     /* back call, if valid */
        IntParms.pProgressInfo((char *)"",true);
           }
           continue;
      case kPowerFail :
           LogFileWrite("No Success on MausReceive ! (Power fail) ");
           return false;
    }

    ulReceiveBufferLength ++;
    err = PortReceive ( pPort, &pucReceiveBuffer[2], &ulReceiveBufferLength, ( uiMode | kDebugOut ) & ~kClearDebugBuffer, pucReceiveBuffer );
    if (err != kOK)
       LogFileWrite("ERROR: -2- Portreceive mit Fehler (%s)",ErrText[err-1]);

    if (uiMode & kEcho)                       /* Read data after echo is set */
    {  uiMode &= ~kEcho;
       goto R0;
    }

    // Check Checksum
    if (GetCheckSum(pucReceiveBuffer, pucReceiveBuffer[1] + 3, 0x00) != 0)
    {  LogFileWrite("Wrong Checksum received !");

      // request to repeat last telegram (uC -> PC)

      if (li < ulTrys)
      {
         memcpy(gcMausTransmitBuffer, (unsigned char*)kPCNACK, kPCNACKLength);
         gcMausTransmitLength = kPCNACKLength;
         PortSend(pPort, (unsigned char*) kPCNACK, kPCNACKLength);
                         /* letzte Daten merken, sonst geht bei 0xAA schief */
      }
      continue;
    }

    switch (pucReceiveBuffer[0])
    {
      case kuCACK :
           if (cCommand == kuCACK)          // 0x55
           {
              bDone = true;
              break;                        // switch
           }
                                            // Acknolege the Acknolege ;->
   /* Sonst wird beim Acknowledge auf die Reizung sofort ein 5D gesendet. Wir
      warten bei der 0x52 aber auf eine 0x05            */
           if (li < ulTrys)
           {
              memcpy(gcMausTransmitBuffer, (unsigned char*)kPCACK, 3);
              gcMausTransmitLength = 3;
              if (ulTrys < 50) ulTrys = 50;
              PortSend(pPort, (unsigned char*)kPCACK, 3L);

           }
           break;

      case kuCNACK :                        // 0xAA
           if (li < ulTrys)
           {  TimeWait (5000);
              PortSend(pPort, gcMausTransmitBuffer, gcMausTransmitLength);
           }
           break;

                                            // Cmd need more time or simple OK
                                            // Command unknown by device
      case kuCREJEKT :                      // 0x09
           LogFileWrite("No Success on MausReceive ! (Reject) ");
           return false;

      default :
                                            // check Commandbyte
           if (cCommand != pucReceiveBuffer[0])
           {  LogFileWrite("No Success on MausReceive ! (Wrong commandbyte) ");
              return false;
           }
           bDone = true;
    }
  }
  *pucReceiveBufferLength = pucReceiveBuffer[1];
  return true;
}
//------------------------------------------------------------------------------
unsigned char GetCheckSum ( const unsigned char *pucData, const unsigned long ulLength, unsigned char ucCheckSumme )
{
  for ( unsigned long ulCount = 0L; ulCount < ulLength; ulCount ++ )
  {
    ucCheckSumme ^= pucData [ ulCount ];
  }
  return ucCheckSumme;
}
//------------------------------------------------------------------------------


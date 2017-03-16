#ifndef _MausBus
#define _MausBus
bool MausSend (RS232Typ *pPort, unsigned char cCommand, unsigned char *pcDataBuffer, unsigned char cLength);
bool MausReceive (RS232Typ *pPort, unsigned char cCommand, unsigned char *pucReceiveBuffer, unsigned char *pucReceiveBufferLength, unsigned long ulTrys, unsigned int uiMode);
#endif

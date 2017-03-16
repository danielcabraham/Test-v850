/**
 * @file 	serial.h
 * @brief 	Header file of the serial.cpp
 * @author 	Satyanarayana Venkatesh (RBEI/ECG2)
 * 		kuk5cob<Kumar.Kandasamy@in.bosch.com>
 * @copyright 	(c) 2013 Robert Bosch GmbH
 * @version 	0.2
 * @history
 * 		0.1 - initial version
 * 		0.2 - code cleaned.
 */

#ifndef serial_h__
#define serial_h__

#include <string.h>
#include "v850_Macro.h"


/**Macro used to define default gpio for v850 reset*/
#define GPIO_V850_RESET "229" /*168 + 61*/
/**Macro used to define default gpio for DNL-Enable*/
#define GPIO_V850_VPP	"226" /*168 + 58*/

/**enum used to give the basic members for parity setting*/
enum
{
	/** Used to denote no parity*/
	kNone,
	/** Used to denote odd parity*/ 
	kOdd,
	/** Used to denote even parity*/ 
	kEven
};

/**enum used to give the basic members for bit-mask setting*/
enum 
{
	/** Used to denote 7 bits in a single data transmitted*/ 
	k7Bit = VAL_7,
	/** Used to denote 8 bits in a single data transmitted*/ 
	k8Bit
};

/**enum used to give the basic members for stop bit setting*/
enum 
{
	/** used to assign stop bit value as 1 */
	k1Stop = VAL_1
};

/**enum used to give basic flags to validate/set Maus data */
enum
{

	kNoDebug = INIT_INDEX,
	kGuard = VAL_1,
	kClearDebugBuffer = VAL_2,
	kDebugOut = VAL_4,
      	kDebug = kDebugOut | kClearDebugBuffer,
	kDelay = VAL_8,
	kEcho  = VAL_16,
	kEmpty = VAL_32
};

/**enum used to find out transfer is ok/not*/
enum 
{
	kOK,
	kTimeout,
	kPowerFail,
	kReadError
};

/**enum used to find out transfer is ok/not*/
enum 
{
	P_OFF,
	P_NORM,
	P_CROSS
};


//=============================================================================
//			NAMESPACE DECLARATIONS
//=============================================================================

//=============================================================================
//			GLOBALS - GENERAL
//=============================================================================

typedef void *HANDLE;

/** RS232Typ is structure which is hold basic and essential info for rs232*/
struct RS232Typ
{
	/** COM port name */
	char *pcName;
	/** COM port handle */
	int hID;
	/** Used to give ms delay(timer opt)*/
	unsigned long ulTimeOut;
	/** COM port baud rate */
	unsigned long ulBaudRate;
	/** Data word size/bits in expected data */
	unsigned char ucBit;
	/** Used to provide stop bit */
	unsigned char ucStop;
	/** Total bit in a rs232 transfer */
	unsigned char ucBitCount;
	/** Used to provide the parity data(no/odd/even parity)*/
	unsigned char ucParity;
	/** Clear ti send flag */
	unsigned char ucCts;
	/** Data set ready */
	unsigned char ucDsr;
};
typedef struct RS232Typ RS232Typ;

//=============================================================================
//	FUNCTION PROTOTYPES - some prototypes for serial comms
//=============================================================================

bool PortExit(void);
bool PortOpen(RS232Typ *pPort);
void PortClose(RS232Typ *pPort);
bool PortSetup(RS232Typ *pPort);
bool PortReadCTS (RS232Typ *pPort);
bool PortReadDSR (RS232Typ *pPort);
void PortClear   (RS232Typ *pPort);
void PortDelete(RS232Typ *pPort);
bool PortSetDTR (RS232Typ *pPort, bool bSet);
bool PortSetRTS  (RS232Typ *pPort, bool bSet);
unsigned long PortSend(RS232Typ *pPort, unsigned char *pcBuffer, long lLength);
bool PortReadOK (RS232Typ *pPort, char cOK, char *cNOK, bool CheckEnd = false);
bool PortInit(const char* GPIO_RESET = GPIO_V850_RESET,
		const char* GPIO_VPP = GPIO_V850_VPP);
int PortReceive(RS232Typ *pPort, unsigned char *pucRcvBuf,
		unsigned long *pulRcvBufLen, unsigned int uiMode,
		unsigned char *pucOutBuf = NULL);
RS232Typ *PortNew (char *pcName, unsigned long ulTimeOut,
		unsigned long ulBaudRate, unsigned char ucBit,
		unsigned char ucParity, unsigned char ucStop);

#endif //serial_h__

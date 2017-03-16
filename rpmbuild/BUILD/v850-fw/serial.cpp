/**
 * @file 	serial.cpp
 * @brief 	Handles Serial communication between Intel and v850
 * @author 	Satyanarayana Venkatesh (RBEI/ECG2)
 * 		kuk5cob<Kumar.Kandasamy@in.bosch.com>
 * @copyright 	(c) 2013 Robert Bosch GmbH
 * @version 	0.2
 * @history
 * 		0.1 - initial version
 * 		0.2 - code cleaned.
 */


#include <unistd.h> 
#ifndef UTEST
#include <fcntl.h>
#endif
#include <errno.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/ioctl.h>

extern "C" {
#include <linux/serial.h>
}

#include "serial.h"
#include "globals.h"
#include "Timing.h"
#include "Flash850.h"
#include "flash.h"
#include "cpld_gpio_swdnl.h"

/**Macro to enable certain portion of the code*/
#define DEBUGCOM
/**Gobal file ptr for VPP gpio*/
FILE	*fp_gpio_Vpp;
/**Gobal file ptr for DNL-EN gpio*/
FILE	*fp_gpio_Reset;
/**Extern varaible for application-parameter*/
extern Flash_TYParms	FlashParms;
/**Extern varaible for internal-parameter*/
extern Flash_TYInternalParms IntParms;
/** Extern function used to log msg from SW */
extern void   LogFileWrite(const char* arg_list, ...);

/**Used to match the baud rate*/
unsigned long Baudrate[] = {	0, 50, 110, 134, 150, 200, 300, 600,
                                1200, 1800, 2400, 4800, 19200, 38400,
                                57600, 115200, 230400, 460800, 500000,
                                576000, 921600, 1000000, 1152000, 1500000,
                                2000000, 2500000, 3000000, 3500000, 4000000
                           };

/**Used to hold baud rate Macro*/
unsigned long Brate_match[] = {	B0, B50, B110, B134, B150, B200, B300, B600,
                                B1200, B1800, B2400, B4800, B19200, B38400,
                                B57600, B115200, B230400, B460800, B500000,
                                B576000, B921600, B1000000, B1152000, B1500000,
                                B2000000, B2500000, B3000000, B3500000, B4000000
                              };

/* local proto types */
int verified_tcsetattr(int fd, int extopt, struct termios rOptions);
speed_t GetUnixBaudRate(unsigned long ulBaudRate);
tcflag_t GetUnixSizeMask(unsigned char ucByteSize);
tcflag_t GetUnixStopBitsMask(unsigned char ucStopBits);
tcflag_t GetUnixParityMask(unsigned char ucParity);



/**
 * @brief 	This function must be used to init the GPIO configuration
 * 		to set Reset and VPP to the V850 to be able to enter the 
 * 		blindrom mode.
 * @param 	GPIO_RESET - Gpio reset pin number
 * @param 	GPIO_VPP   - Gpio vpp pin number
 * @return	True if PortInit is successful
 * @author	Vigneshwaran Karunanithi(RBEI/ECA5)
 * @history	
 * 		0.1	-initial version
 * 		0.2	-code cleaup work.
 */
bool PortInit(const char *GPIO_RESET, const char *GPIO_VPP)
{
	bool bRet = false;

	/*create the GPIO file pointer*/
	fp_gpio_Vpp = fp_createGpioOut(GPIO_VPP);
	fp_gpio_Reset = fp_createGpioOut(GPIO_RESET);

	if(NULL != fp_gpio_Vpp)
	{
		/*Deactivate VPP pin of V850*/
		bRet = bSetGpioPin(fp_gpio_Vpp, GPIO_VPP,
					(FLASH_VPP == false));
		if (!bRet)
		{
			LogFileWrite("Error on Deactivate VPP\n");
		}
	}

	if(bRet && (NULL != fp_gpio_Reset))
	{
		/*Deactivate RESET pin of V850*/
		bRet = bSetGpioPin(fp_gpio_Reset, GPIO_RESET,
					(false == FLASH_RESET));
		if (!bRet)
		{
			LogFileWrite("Error on Deactivate Reset\n");
		}
	}
	/*start the timer from Timing.cpp*/
	TimeInit();
	return bRet;
}

/**
 * @brief	This function must be used to release the GPIO configuration.
 * @param	void
 * @return	True if PortExit is successful
 * @author 	Vigneshwaran Karunanithi(RBEI/ECA5)
 * @history
 * 		0.1	-Initial version
 * 		0.2	-Code cleanup work.
 */
bool PortExit()
{
	LogFileWrite("Serial: PortExit mit %s, %s", FlashParms.GPIO_Reset,
			FlashParms.GPIO_VPP);

	if(fp_gpio_Vpp)
	{

		/*Deactivate VPP pin of V850*/
		bSetGpioPin(fp_gpio_Vpp, FlashParms.GPIO_VPP,
				(FLASH_VPP == false));
		fclose(fp_gpio_Vpp);
		vDeleteGpioOut(FlashParms.GPIO_VPP);
		fp_gpio_Vpp = NULL;
	}

	if(fp_gpio_Reset)
	{

		/*Deactivate RESET pin of V850*/
		bSetGpioPin(fp_gpio_Reset, FlashParms.GPIO_Reset,
				(false == FLASH_RESET));
		fclose(fp_gpio_Reset);
		vDeleteGpioOut(FlashParms.GPIO_Reset);
		fp_gpio_Reset = NULL;
	}
	
	/*Deinitialise the Timing module*/
	TimeExit();	
	return true;
}

/**
 * @brief  This function can be used to create a new port based on the
 * configuration parameters provided
 * @param pcName Serial Port name
 * @param ulTimeOut Timeout Value interms of milliseconds
 * @param ulBaudRate Baudrate of serial communication
 * @param ucBit
 * @param ucParity Parity Bit
 * @param ucStop Stop Bit
 * @return pointer to the new RS232Typ structure created
 */
RS232Typ *PortNew (char *pcName, unsigned long ulTimeOut,
			unsigned long ulBaudRate, unsigned char ucBit,
			unsigned char ucParity, unsigned char ucStop)
{
	RS232Typ *pNewPort = new RS232Typ;

	if(NULL != pNewPort && NULL != pcName)
	{
		pNewPort->ulTimeOut  = ulTimeOut;
		pNewPort->ulBaudRate = ulBaudRate;
		pNewPort->ucBit      = ucBit;
		pNewPort->ucStop     = ucStop;
		pNewPort->ucBitCount = ucBit + ucStop + NEXT_INDEX;
		pNewPort->ucParity   = ucParity;
		pNewPort->ucCts      = false;
		pNewPort->ucDsr      = false;

		pNewPort->pcName = new char[strlen(pcName) + NEXT_INDEX];
		if(NULL != pNewPort->pcName)
		{
			strcpy(pNewPort->pcName, pcName);
		}
		else
		{
			LogFileWrite("ERROR: NO ENOUGH MEMORY FOR PORT-NAME");
		}

		LogFileWrite("Serial : New Port with Params -- Name: %s "
				"TimeOut:%d BaudRate:%d, BitSize:%d "
				"StopBits:%d Parity:%d", pNewPort->pcName,
				pNewPort->ulTimeOut, pNewPort->ulBaudRate,
				pNewPort->ucBit, pNewPort->ucStop, 
				pNewPort->ucParity);
	}
	return pNewPort;
}

/**
* @brief	This function can be used to delete a particular port.
* @param 	pPort - RS232Typ Pointer which needs to be deleted.
* @return	void
* @author	Vigneshwaran Karunanithi(RBEI/ECA5)
*/
void PortDelete(RS232Typ *pPort)
{
	if (NULL != pPort)
	{
		if (NULL != pPort->pcName)
		{
			delete []pPort->pcName;
			pPort->pcName = NULL;
		}
		delete pPort;
	}
}

/**
 * @brief	This function is used to open a particulat port based on the
 * 		parameter provided by the RS232Typ
 * @param 	pPort - RS232Typ Pointer which needs to be opened to establish
 * 		serial communication.
 * @return	True for successful creation of port/false for error.
 * @author	Vigneshwaran Karunanithi(RBEI/ECA5).
 */
bool PortOpen( RS232Typ *pPort )
{
	bool bRet	= false;
	int ret 	= 0;
	struct flock lock;

	if (NULL != pPort)
	{
		pPort->hID = open(pPort->pcName, (O_RDWR | O_NOCTTY));

		if (-EPERM == pPort->hID)
		{
			LogFileWrite("Serial :  PortOpen failed! "
					"errorno :%d error Info: %s "
					, errno, strerror(errno));
		}
		else
		{
			lock.l_type	= F_WRLCK;
			lock.l_start 	= INIT_INDEX;
			lock.l_whence 	= SEEK_SET;
			lock.l_len 	= INIT_INDEX;
			lock.l_pid 	= getpid();

			ret = fcntl(pPort->hID, F_SETLK, &lock);
			if (SUCCESS == ret)
			{
				LogFileWrite("Serial : Serial connection "
						"established on %s",
						pPort->pcName);
				bRet = true;
			}
			else
			{
				LogFileWrite("Serial: %s is in use and can't"
						" be locked \nError:%s",
						pPort->pcName,
						sys_errlist[errno]);
				bRet = false;
			}
		}
	}
	return bRet;
}

/**
 * @brief	This function is used to close a particular port based on the
 * 		parameter provided by the RS232Typ.
 * @param 	pPort -RS232Typ Pointer which has file descriptor which needs
 *		to be closed.
 * @return	void
 * @author	Vigneshwaran Karunanithi(RBEI/ECA5)
 * @history	
 * 		0.1	-initial version.
 * 		0.2	-code cleaned.		
 */
void PortClose(RS232Typ *pPort)
{
	if ((NULL != pPort) && (false != pPort->hID))
		close(pPort->hID);
}

/**
 * @brief	This function must be used to configure the port based on the
 * 		parameter provided by the RS232Typ.
 * @param 	pPort -RS232Typ Pointer whose flags to be set as required.
 * @return 	True if PortSetup is successful else False is returned.
 * @author	Vigneshwaran Karunanithi(RBEI/ECA5)
 * @history	
 * 		0.1	-initial version.
 * 		0.2	-code cleaned.		
 */

bool PortSetup(RS232Typ *pPort)
{
	int res = 0;
	struct termios rOptions;
	char buffer[TMP_ARR_SIZE4/*256*/];
	
	if (!pPort)
	{
		/* That should never happen*/
		return false;
	}
	
	res = tcgetattr(pPort->hID, &rOptions);/*to get the current rOptions*/
	if (!res)
	{
		/*Clear all flags and start from scratch*/
		bzero(&rOptions, sizeof(rOptions));
		
		/* Set flags in different stages, otherwise tcsetattr will not work as
		 * expected.
		 * 1. Set Databyte, Parity and Stopbits
		 */
		rOptions.c_cflag = (rOptions.c_cflag & ~CSIZE & ~CSTOPB & ~PARENB &
		                    ~CRTSCTS)
		                   | GetUnixSizeMask(pPort->ucBit)
		                   | GetUnixParityMask(pPort->ucParity)
		                   | GetUnixStopBitsMask(pPort->ucStop)
		                   | CLOCAL;
		res = verified_tcsetattr(pPort->hID, 0, rOptions);
	}
	if (!res)
	{
		/* 2. Set flags for behaviour*/
		rOptions.c_lflag = (rOptions.c_lflag & ~ICANON & ~ISIG & ~ECHO);
		res = verified_tcsetattr(pPort->hID, 0, rOptions);
	}
	if (!res)
	{
		/* 3. Set flags for behaviour*/
		rOptions.c_oflag = (rOptions.c_oflag & ~OPOST );
		res = verified_tcsetattr(pPort->hID, 0, rOptions);
	}
	if (!res)
	{
		/* 3.5 Set flags for behaviour*/
		rOptions.c_iflag = (rOptions.c_iflag & ~IXON & ~IXOFF );
		res = verified_tcsetattr(pPort->hID, 0, rOptions);
	}

	if (!res)
	{
		/* 4. Set timeout*/
		rOptions.c_cc[VTIME] = (uint8_t)(pPort->ulTimeOut / VAL_100);
		rOptions.c_cc[VMIN]  = INIT_INDEX;
		res = verified_tcsetattr(pPort->hID, 0, rOptions);
	}

	if (!res)
	{
		/* 5. Set baudrate*/
		speed_t unix_speed = GetUnixBaudRate(pPort->ulBaudRate);
		cfsetispeed(&rOptions, unix_speed);
		cfsetospeed(&rOptions, unix_speed);
		res = verified_tcsetattr(pPort->hID, 0, rOptions);
	}
	if (!res)
	{
		/* 6. Activate receiver*/
		rOptions.c_cflag |= CREAD;
		res = verified_tcsetattr(pPort->hID, 0, rOptions);
	}

	if (!res)
	{
		sprintf(buffer,"PortSetup Port: %s Handle: %d BaudRate:%u ByteSize:%d "
		        "Parity:%d StopBits:%d", pPort->pcName,pPort->hID,
		        (unsigned int)pPort->ulBaudRate,pPort->ucBit,
		        pPort->ucParity,pPort->ucStop);
	}
	else 
	{
		sprintf(buffer,"Error during PortSetup - errno: %d "
			"errorInfo: %s", errno, strerror(errno));
	}

	LogFileWrite("<<<<<<c_cflag = %x, c_lflag = %x, c_oflag = %x, c_iFlags=%x\n",
	             rOptions.c_cflag,rOptions.c_lflag,rOptions.c_oflag,rOptions.c_iflag);

	LogFileWrite(buffer);

	return (!res);
}


/**
 * @brief	This function is used to send the data to a particular port 
 * 		based on pPort parameter.
 * @param	pPort -RS232Typ ptr
 * @param	pcBuffer -char ptr(data need to transmit).
 * @param	lLength	-long value(denote no of bytes need to transmit)	
 * @return 	Length of the bytes sent
 * @author	Vigneshwaran Karunanithi(RBEI/ECA5)
 * @history	
 * 		0.1	-initial version.
 * 		0.2	-code cleanup.		
 */
unsigned long PortSend(RS232Typ *pPort, unsigned char *pcBuffer, long lLength)
{
	ssize_t iStatus = false;

	if (NULL == pPort)
	{
		LogFileWrite("Error: invalid pPort in %s\n", __func__);
		return false;
	}
	
	iStatus = write(pPort->hID, pcBuffer, lLength);
	if (iStatus < SUCCESS)
	{
		LogFileWrite("Serial : write failed! errorno :%d "
				"error Info :%s ", errno,
				strerror(errno));
		lLength = false;
	}
	else
	{
#ifdef DEBUGCOM
		if (NULL != IntParms.pComDebug)
		{
			char buf[TMP_ARR_SIZE6/*3072*/] = "Tx: ";
			for (long i = 0;
				((i < lLength) && (i <= (TMP_ARR_SIZE5)));
				i++)
			{
				/*macro used to avoid magic num err*/
				char Help[TMP_ARR_SIZE0/*4*/];
				sprintf(Help, "%02X ", *(pcBuffer+i));
				strcat(buf, Help);
			}
			
			if (lLength > TMP_ARR_SIZE5/*1020*/)
				strcat(buf," ...");

			IntParms.pComDebug(buf, true);
		}
#endif
	}

	return (unsigned long)lLength;
}


/**
 * @brief	This function is used to receive the data from the port and 
 * 		the received data and the length will be updated in the 
 * 		parameters pucRcvBuf and pulRcvBufLen.
 * @param 	pPort Serial Port pointer from which data to received
 * @param 	pucRcvBuf Data buffer to which data received is saved
 * @param 	pulRcvBufLen Length of data to be received
 * @param 	uiMode
 * @return 	Length of the bytes read
 * @autho	Vigneshwaran Karunanithi(RBEI/ECA5)
 * @history
 * 		0.1	-Initial version.
 * 		0.2	-Code cleanup work.
 */
int PortReceive(RS232Typ *pPort, unsigned char *pucRcvBuf,
                unsigned long *pulRcvBufLen, unsigned int uiMode,
                unsigned char *)
{
	int u8Ret = kReadError;
	ssize_t iCount = INIT_INDEX;
	unsigned long ulBufLen  = *pulRcvBufLen;
	unsigned long ulBufCount = 0L;
	bool bDone = false;


	if (NULL == pPort)
	{
		LogFileWrite("%s:invalide ptr\n", __func__);
		return u8Ret;
	}
	
	while (!bDone)
	{
		iCount = read(pPort->hID, &pucRcvBuf[ulBufCount],
				ulBufLen -ulBufCount);
		if (iCount > INIT_INDEX)
		{
			ulBufCount += (unsigned long)iCount;
			if (ulBufCount >= ulBufLen )
			{
				bDone = true;
			}
		}
		else 
			bDone = true;
	}

	if (ulBufCount > INIT_INDEX)
	{
#ifdef DEBUGCOM
		if ((NULL != IntParms.pComDebug) && (uiMode & kDebugOut))
		{
			char buf[TMP_ARR_SIZE6] = "Rx: ";

			if (uiMode & kClearDebugBuffer)
			{
				sprintf(buf, "Rx: ");
			}
			
			for (unsigned long i = 0; ((i < ulBufCount) && 
				(i <= (TMP_ARR_SIZE5))); i++)
			{
				char Help[TMP_ARR_SIZE0];

				sprintf(Help,"%02X ",
					(unsigned char)*(pucRcvBuf+i));

				if (strlen(buf) < (TMP_ARR_SIZE5 * VAL_3))
					strcat(buf, Help);
			}

			if (ulBufCount > TMP_ARR_SIZE5)
				strcat(buf," ...");

			IntParms.pComDebug(buf, true);     /* Ausgabe dauert mindestens 3 ms */
		}
		else
#endif
			if (uiMode & kDelay)
			{
				TimeWait(3000);     /* Maus Delay falls keine Flashkommunikation */
			}

		u8Ret = kOK;
	}
	else
	{
		char buffer[VAL_100];

		if (INIT_INDEX == iCount)
			sprintf(buffer,"Serial : No data Available");
		else
			sprintf(buffer,"Serial : receive Error no: %d "
				"errorInfo: %s", errno, strerror(errno));
		if (INIT_INDEX == (uiMode & kEmpty))
		{
			LogFileWrite(buffer);
			u8Ret = kTimeout;
		}
		else
			u8Ret = kOK;
	}
	*pulRcvBufLen = ulBufCount;

	return u8Ret;
}

/**
 * @brief	This function is used to Set/Reset the RESET-Pin.
 * 		In flash.cpp the DTR (Data Terminal Ready)-Pin of
 * 		COM-port was used, now it is GPIO.
 * @param	pPort - RS232Typ ptr
 * @param 	bSet - Value to be set to GPIO_Reset
 * @return 	True if PortSetDTR is successful else False is returned
 * @author	Vigneshwaran Karunanithi(RBEI/ECA5)
 * @history
 * 		0.1	-Initial version.
 * 		0.2	-Code cleanup work.
 */
bool PortSetDTR(RS232Typ *pPort, bool bSet)
{
	if (NULL != fp_gpio_Reset)
	{
		return(bSetGpioPin(fp_gpio_Reset, FlashParms.GPIO_Reset,
			bSet));
	}
	else 
		return false;
}

/**
 * @brief	This function is used to Set/Reset the VPP-Pin.
 * 		In flash.cpp the RTS(Request To Send)-Pin of COM-port
 * 		was used, now it is GPIO.
 * @param	pPort - RS232Typ ptr
 * @param 	bSet Value to be set to GPIO_VPP
 * @return 	True if PortSetRTS is successful else False is returned
 * @author	Vigneshwaran Karunanithi(RBEI/ECA5)
 * @history
 * 		0.1	-Initial version.
 * 		0.2	-Code cleanup work.
 */
bool PortSetRTS(RS232Typ *pPort, bool bSet)
{
	if (NULL != fp_gpio_Vpp)
	{
		return (bSetGpioPin(fp_gpio_Vpp, FlashParms.GPIO_VPP, bSet));
	}
	else 
		return false;
}

/**
 * @brief	This function is used to read the current value of 
 * 		CTS(Clear To Send) from the port.
 * @param	pPort - RS232Typ ptr.
 * @return 	True if PortReadCTS is successful else False is returned.
 * @author	Vigneshwaran Karunanithi(RBEI/ECA5)
 * @history
 * 		0.1	-Initial version.
 * 		0.2	-Code cleanup work.
 */
bool PortReadCTS(RS232Typ *pPort)
{
	bool bRet = false;

	if (NULL != pPort)
		bRet = ((pPort->ucCts) > SUCCESS);
	
	return bRet;
}

/**
 * @brief	This function is used to read the current value of 
 * 		DSR(Data Set Ready) for the port.
 * @param	pPort - RS232Typ ptr.
 * @return 	True if PortReadDSR is successful else False is returned
 * @author	Vigneshwaran Karunanithi(RBEI/ECA5)
 * @history
 * 		0.1	-Initial version.
 * 		0.2	-Code cleanup work.
 */
bool PortReadDSR(RS232Typ *pPort)
{
	bool bRet = false;

	if (NULL != pPort)
		bRet = ((pPort->ucDsr) > SUCCESS);
	
	return bRet;
}


/**
 * @brief 	Clear the serial port until it becomes empty
 * @param	pPort - RS232Typ ptr.
 * @return 	True if PortReadDSR is successful else False is returned
 * @author	Vigneshwaran Karunanithi(RBEI/ECA5)
 * @history
 * 		0.1	-Initial version.
 * 		0.2	-Code cleanup work.
 */
void PortClear(RS232Typ *pPort)
{
	unsigned char cBuffer[TMP_ARR_SIZE4/*256*/];
	unsigned long len = sizeof(cBuffer);

	do
	{
		/*read serial Port until empty*/
		TimeWait(50);
		PortReceive(pPort, cBuffer, &len, (kDebug | kEmpty), NULL);
	}
	while (len > SUCCESS);
}


/**
 * @brief	This function is used to read the the final response 
 * 		from the V850 after a transmission.
 * @param	pPort - RS232Typ ptr
 * @param	cOk   - character value used to check data is ok/not.
 * @param	cNOK  - char ptr (validate whether invalid data is received)
 * @param	CheckEnd - bool value, used to decide value of idx.
 * @return 	True if PortReadOk is successful else False is returned.
 * @author	Vigneshwaran Karunanithi(RBEI/ECA5)
 * @history
 * 		0.1	-Initial version.
 * 		0.2	-Code cleanup work.
 */
bool PortReadOK (RS232Typ *pPort, char cOK, char *cNOK, bool CheckEnd)
{
	if (NULL != pPort)
	{
		ssize_t iCount = INIT_INDEX;
		char cBuffer[TMP_ARR_SIZE7];

		TimeSetStart();
		TimeSetStop(pPort->ulTimeOut);

		do
		{
			cBuffer[INIT_INDEX] = false;

			iCount = read(pPort->hID, cBuffer, 1);
			if (iCount > INIT_INDEX)
			{
				int idx = CheckEnd ? 
					(iCount-NEXT_INDEX) : INIT_INDEX;

				if ((cBuffer[idx] == cOK) || 
					(cBuffer[INIT_INDEX] == cOK))
					/*valide data received*/
					return true; 
				else
				{
					if (*cNOK && (cBuffer[idx] == *cNOK))
						/*invalide data received*/
						return false;					
				}
			}
			else
			{
				char buffer[VAL_100];

				if (INIT_INDEX == iCount)
					sprintf(buffer,"Serial : No"
						" data Available");
				else
					sprintf(buffer,"Serial : receive "
						"Error no: %d errorInfo: %s",
						errno, strerror(errno));
				
				LogFileWrite(buffer);
				return false;
			}
		}
		while (false == TimeIsStop());
	} 

	*cNOK = false;
	LogFileWrite("Serial: Exit PortReadOK() after TimeOut");

	return false;
}

/*--------------------------------------------------------------------------*/
/*------------- Internal functions to set the serial port ------------------*/
/*--------------------------------------------------------------------------*/

/**
 * @brief	Used to verify the COM settings.
 * @param	fd -COM port handle.
 * @param	extopt -Optional operation for tcsetattr
 * @param	rOptions - struct termios variable.
 * @return 	return value of tcset/tcgetattr.
 * @author	Vigneshwaran Karunanithi(RBEI/ECA5)
 * @history
 * 		0.1	-Initial version.
 * 		0.2	-Code cleanup work.
 */
int verified_tcsetattr(int fd, int extopt, struct termios rOptions)
{
	/*setting COM port settings */
	int res = tcsetattr(fd , extopt, &rOptions);
	if (!res)
	{
		struct termios rOptions_old = rOptions;
		/*read back the previously set value*/
		res = tcgetattr(fd, &rOptions);
		if (memcmp(&rOptions_old, &rOptions, sizeof(struct termios)))
			printf("Read back settings differ!\n");
	}
	return res;
}

/**
 * @brief	Used to return Baud rate based on input.
 * @param	ulBaudRate - input baud rate from cfg/cmdline arg.
 * @return 	Input is matched then specific baud rate is returned/B9600
 * @author	Vigneshwaran Karunanithi(RBEI/ECA5)
 * @history
 * 		0.1	-Initial version.
 * 		0.2	-Code cleanup work.
 */
speed_t GetUnixBaudRate(unsigned long ulBaudRate)
{
	unsigned char loop;
	for (loop = 0; loop < (sizeof(Baudrate)/sizeof(Baudrate[0])); loop++)
	{
		if (ulBaudRate == Baudrate[loop])
		{
			return Brate_match[loop];
		}
	}
	/*ret default baudrate if input value is not match*/
	return B9600;
}

/**
 * @brief	Used to return mask(bit) value.
 * @param	ucByteSize - input value from cfg/cmdline arg.
 * @return 	Input is matched then specific bit size is returned/CS8.
 * @author	Vigneshwaran Karunanithi(RBEI/ECA5)
 * @history
 * 		0.1	-Initial version.
 * 		0.2	-Code cleanup work.
 */
tcflag_t GetUnixSizeMask(unsigned char ucByteSize)
{
	switch(ucByteSize) 
	{
		case 5 :
			return CS5;
		case 6 :
			return CS6;
		case 7 :
			return CS7;
		case 8 :
		default:
			return CS8;
	}
}

/**
 * @brief	Used to return stop bit value.
 * @param	ucStopBits - input value from cfg/cmdline arg.
 * @return 	Input is 2 then CSTOPB/0 will be returned from fun.
 * @author	Vigneshwaran Karunanithi(RBEI/ECA5)
 * @history
 * 		0.1	-Initial version.
 * 		0.2	-Code cleanup work.
 */
tcflag_t GetUnixStopBitsMask(unsigned char ucStopBits)
{
	return (VAL_2 == ucStopBits) ? CSTOPB : INIT_INDEX;
}

/**
 * @brief	Used to provide parity mask.
 * @param	ucParity - input value from cfg/cmdline arg.
 * @return 	parity will be returned based on input.
 * @author	Vigneshwaran Karunanithi(RBEI/ECA5)
 * @history
 * 		0.1	-Initial version.
 * 		0.2	-Code cleanup work.
 */
tcflag_t GetUnixParityMask(unsigned char ucParity)
{
	switch(ucParity)
	{
		case 1 :
			return PARENB|PARODD;
		case 2 :
			return PARENB;
		case 0 :
		case 4 :
		default:
			return 0;
	}
}

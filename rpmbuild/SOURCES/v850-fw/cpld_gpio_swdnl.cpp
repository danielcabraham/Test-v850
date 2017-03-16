/**
 * @file	cpld_gpio_swdnl.cpp
 * @brief 	A user-space program to communicate with V850 Device
 *        	i.e. to enable certains pins(gpios) for V850 device for 
 *        	JLR-NGI project.
 * @author 	Satyanarayana Venkatesh - RBEI/ECG2
 * 		kuk5cob<Kumar.Kandasamy@in.bosch.com>
 * @version 	0.2
 * @copyright 	(c) 2015 Robert Bosch GmbH
 * @history	
 * 		0.1 	-initial version
 * 		0.2	-code cleaning (16/12/2015,vrk5cob).
 */

#include <sys/ioctl.h>
#ifndef UTEST
#include <fcntl.h>
#endif
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>

#include "cpld_gpio_swdnl.h"



/*fn used to store the log details in file*/
extern void LogFileWrite(const char * arg_list, ...);

/**
 * @brief 	fp_createGpioOut creates device interface for the specified
 *          	GPIO pin number
 * @param 	pchGpioNum The GPIO interface device file to be created
 * @return  	File pointer of the created GPIO device interface file.
 * @author	vrk5cob
 * @history	
 * 		0.1	-initial version
 * 		0.2	-code cleaup work.
 */
FILE *fp_createGpioOut(const char *pchGpioNum)
{
	FILE *fp;
	char szRequest[TMP_ARR_SIZE1];
	char szPath[TMP_ARR_SIZE2];
	char szPreq[TMP_ARR_SIZE2];

	memset(szRequest, 0, sizeof(szRequest));
	memset(szPath, 0, sizeof(szPath));
	memset(szPreq, 0, sizeof(szPreq));

	if (!(fp = fopen(GPIO_EXPORT, "ab")))
		LogFileWrite("Error, Cannot open GPIO export file!\n");
	else
		rewind(fp);

	if (fp && pchGpioNum)
	{
		unsigned long len = strlen(pchGpioNum);
		strcpy(szRequest,pchGpioNum);
		fwrite(szRequest, sizeof(char), len, fp);
		fclose(fp);

		/* Entry created, build complete szPath */
		sprintf(szPath, GPIO_PATH, pchGpioNum);
		strcpy (szPreq, szPath);
		strcat (szPreq, "/direction");

		/* Open new szPath */
		if (!(fp = fopen(szPreq, "rb+")))
			LogFileWrite("Error,Can't open GPIO direction file\n");
		else
		{
			rewind(fp);
			/* Create GPIO for Output */
			strcpy(szRequest, "out");
			fwrite(szRequest, sizeof(char), strlen(szRequest), fp);
			fclose(fp);

			/* Open new szPath */
			strcpy (szPreq, szPath);
			strcat (szPreq, "/value");

			if (!(fp = fopen(szPreq, "rb+")))
			{
				LogFileWrite("Error,Can't open GPIO %s file\n"
						, szPreq);
			}
			else
				rewind(fp);
		}
	}
	return fp;
}


/**
 * @brief 	Delete the GPIO Pin device interface
 * @param 	pGpioNum - GPIO pin number whose interface to be deleted.
 * @return 	void
 * @author	vrk5cob
 * @history	
 * 		0.1	-initial version
 * 		0.2	-code cleaup work.
 */
void vDeleteGpioOut(const char* pGpioNum)
{
	FILE* fp;

	if (!(fp = fopen(GPIO_UNEXPORT, "ab")))
		LogFileWrite("Error, Cannot open GPIO unexport file!");
	else
	{
		fwrite(pGpioNum, sizeof(char), strlen(pGpioNum),  fp);
		fclose(fp);
	}
}

/**
 * @brief 	Write to CPLD GPIO Output via Sysfs
 * @param 	fp -  File pointer to the GPIO device interface file to
 *          	which data to be written
 * @param 	pGpioval -  Value to be written to GPIO pin
 * @return 	void
 * @author	vrk5cob
 * @history	
 * 		0.1	-initial version
 * 		0.2	-code cleaup work.
 */
void vWriteGpioOut(FILE* fp, const char* pGpioval)
{
	if (fp)
	{
		/* write given input in value file of gpio */
		if(fwrite(pGpioval, sizeof(char), 1, fp))
			rewind(fp);
	}
	else
		LogFileWrite("invalid file ptr in %s\n", __func__);
}

/**
 * @brief 	To set the GPIO pin
 * @param 	fp -  File pointer to the GPIO device interface file 
 * 		to which data to be written.
 * @param 	pGpioNum - GPIO pin number
 * @param 	bset - "Set value" used to set GPIO pin
 * @return 	Result of setting pin
 * @author	vrk5cob
 * @history	
 * 		0.1	-initial version
 * 		0.2	-code cleaup work.
 */
bool bSetGpioPin(FILE* fp, const char* pGpioNum, bool bset)
{
	bool bRet = false;

	if (pGpioNum)
	{
		LogFileWrite("Inside bSetGpioPinNew(%s) %d", pGpioNum, bset);
		if(!fp)
			LogFileWrite("Error in Access of GPIO %s", pGpioNum);
		else
		{
			char gpioValue[] = {false};

			fread(gpioValue, sizeof(char), 1, fp);
			LogFileWrite("GPIO VALUE [BEFORE]= %d, %c",
					(int)gpioValue[INIT_INDEX],
					gpioValue[INIT_INDEX]);

			if (bset)
			{
				/*making gpio high*/
				gpioValue[INIT_INDEX] = '1';
				vWriteGpioOut(fp,gpioValue);/*Activate Chip*/
			}
			else
			{
				/*making gpio low*/
				gpioValue[INIT_INDEX] = '0';
				vWriteGpioOut(fp,gpioValue);/*Reset*/
			}

			gpioValue[INIT_INDEX] = 'X';
			
			/* read back current value in gpio*/
			fread(gpioValue, sizeof(char), 1, fp);
			LogFileWrite("GPIO VALUE [AFTER]= %d, %c"
					,(int)gpioValue[INIT_INDEX],
					gpioValue[INIT_INDEX]);

			LogFileWrite("OK, GPIO Set Successfully !");
			bRet = true;
		}
	}
	else
		LogFileWrite("No GpioPinNum received");

	return bRet;
}

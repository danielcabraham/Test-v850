/**
 * @file	v850_dnl_if.cpp
 * @brief 	This creates wrapper over flash programmer modules
 * 		& exposes minimum required API for flash download.
 * @author 	ShashiKiran HS (RBEI/ECG2)
 * 		kuk5cob<Kumar.Kandasamy@in.bosch.com>
 * @copyright 	(c) 2013 Robert Bosch GmbH
 * @version 	0.3
 * @history
 * 		0.1 - initial version
 * 		0.2 - done reset of v850 before shutdown
 * 		0.3 - Code cleanup work.
 */

#ifndef LINUX
#include <sysutils.hpp>
#endif

#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include "globals.h"
#include "Flash850.h"
#include "v850_dnl_if.h"
#include "flash.h"
#include "parser.h"
#include "MAUSProc.h"
#include "v850_Macro.h"

/**Customized macro for size of function*/
#define SIZEOF(x) (sizeof(x)-1)
/**Macro give current shared lib function*/
#define VERSION "1.0.11"
/**Default path for flash.cfg file*/
#define FLASH_CFG "/opt/jlr/etc/v850/flash.cfg"

/**Extern function to log the SW logs*/
extern void LogFileWrite(const char * arg_list, ...);
/**Extern function to check the file length*/
extern long filelength(FILE *);
/**Extern function to change lower char to upper char*/
extern const char* toUpper(const char* str);
/**Function's declaration - this is used to get dnl file content*/
unsigned char * GetDF_Data(FILE *f_DFile, ulword BlockTyp, ulword *BlockLen);
/**Function's declaration - function used to change the KDS data
 * from file to ptr*/
ulword KDS_XChg(char *ptrKDS, bool blDummy);

/**ProgData is structure which will handle and pass
 * config data to controll the shared lib's behavior,
 * mode and type of flashing
 */
struct ProgData
{
	/** This flag used to enable/disable debug info log in SW log file */
	bool blDebOut;
	/** This flag used to control flashing percentage printing */
	bool blProgOut;
	/** This flag is set to true when update flash is required */
	bool blProgUpdate;
	/** This flag is set to true when virgin flash is required */
	bool blProgVirgin;
	/** Flag set to true when connection testing alone is required */
	bool blTestMode;
	/** Set to true when KDS section need to be unchanged */
	bool blKDSSave;
	/** Set to true will completely erase the chip */
	bool blChipErase;
	/** When set to true it will erase only the data flash */
	bool blDataFlashErase;
	/** Member will hold COM setting */
	char szComSettings[TMP_ARR_SIZE4/*256*/];
	/** Will hold dnl file path */
	char szDnlFileName[TMP_ARR_SIZE8/*512*/];
	/** Will hold log file path */
	char szLogFilePath[TMP_ARR_SIZE8];
	/** Will hold MAUS file path */
	char szMAUSScript[TMP_ARR_SIZE8];
	/** Will hold KDS file path -if kds need to flash in virgin flash */
	char szKDSFileName[TMP_ARR_SIZE8];
	/** Will hold v850's reset gpio name */
	char GPIO_Reset[TMP_ARR_SIZE1/*5*/];
	/** Will hold v850's dnl enable name */
	char GPIO_VPP[TMP_ARR_SIZE1];
	/** Will hold black's info when /flash opt is set */
	uword BlkData[TMP_ARR_SIZE9/*20*/];

} ProgData = { false /*blDebOut*/, true /*blProgOut*/, false /*blProgUpdate*/,
		false /*blProgVirgin*/, false /*blTestMode*/, false
		/*blKDSSave*/, false /*blChipErase*/, false
		/*blDataFlashErase*/, "/dev/ttyAM0,115200,N,8,1"
		/*szComSettings*/, "/etc/master.dnl" /*szDnlFileName*/,
		"/etc/" /*szLogFilePath*/, "" /*szMAUSScript*/, ""
		/*szKDSFileName*/, "229" /*GPIO_Reset*/, "226"/*GPIO_VPP*/,
		{ 0x0 }/*BlkData*/ };


/** Declaration for call back function */
T_fCallback g_fCallback;
/*total number of flash blocks provided in flash.cfg*/
uint8_t g_u8NumberOfBlocks;
/* Erase blocks*/
ubyte  g_u8EraseBlk;
ulword g_u32EraseStart;
ulword g_u32EraseLen;
/**max size of char buffer which LogFileWrite can afford. Early
 * log should not exceed this size
 */
char g_EarlyLog[TMP_ARR_SIZE10];
/** Structure to hold flash parameters */
Flash_TYParms g_rFlashParms;
/** Variable which holds Com port information */
char gComPort[TMP_ARR_SIZE4];

/**
 * @brief	This function takes the progress data provided by
 * 		Flash_vSetProgressBC and computes the percentage of
 * 		download which will be passed to specified call back function.
 *
 * @param 	Data Variable which contains the download progress data being
 * 		update by function Flash_vSetProgressBC.
 * @param 	newline
 * @return 	FLASH_SUCCESS when progress data sent successfully.
 * @author	Vigneshwaran Karunanithi(RBEI/ECA5)
 * @history
 * 		0.1 - Initial version
 * 		0.2 - Code cleanup.
 */
uint32_t UpdateReport(char *Data, bool newline)
{
	char *pTemp = NULL;
	uint8_t u8Temp = false;
	static uint8_t cBlock;
	static uint8_t u8LastTemp;
	uint8_t u8PercentageDownload = false;

	if (!Data)
	{
		/* return avoid seg problem */
		return FLASH_SUCCESS;
	}


	if (false == (pTemp = strstr(Data, "Programming:  ")))
	{
		/* return avoid seg problem */
		return FLASH_SUCCESS;
	}

	pTemp = pTemp + SIZEOF("Programming:  ");
	u8PercentageDownload = strtoul(pTemp, NULL, 10);

	u8Temp = (u8PercentageDownload +
			(VAL_100 * cBlock ))/g_u8NumberOfBlocks;

	if (u8Temp != u8LastTemp)
	{
		u8LastTemp = u8Temp;
		if (u8Temp < VAL_100)
		{
			if (g_fCallback)
				g_fCallback(u8Temp);
		}
	}

	if (VAL_100 == u8PercentageDownload)
		++cBlock;

	LogFileWrite("%s", Data);

	return FLASH_SUCCESS;
}

/**
 * @brief	Function to recreate the logfile.
 * @param	void
 * @return 	FLASH_SUCCESS is logfile is created successfully else
 * 		FLASH_NO_SUCCESS will be returned.
 * @author	Vigneshwaran Karunanithi(RBEI/ECA5)
 * @history
 * 		0.1 -Initial version.
 * 		0.2 -Cleanup work.
 */
uint32_t dnlif_LogFileEnable()
{
	/* copy to help-Buffer, not to modify */
	char help[TMP_ARR_SIZE_KB] = {false};
	FILE *fStream = NULL;
	char dnlif_LogFile[TMP_ARR_SIZE_KB] = {false};

	/* the original file name */
	strcpy(help, ProgData.szLogFilePath);

	/* logic should be in sync with LogFile initialization func of
 	 * flash.cpp else there is possiblity of creation of two log files
 	 */
	if (SUCCESS == strstr(toUpper(help), ".TXT"))
		sprintf(dnlif_LogFile,
			"%sLogFile.txt", ProgData.szLogFilePath);
	else
		sprintf(dnlif_LogFile,"%s",ProgData.szLogFilePath);

	fStream = fopen(dnlif_LogFile, "w+");
	if (!fStream)
	{
		printf("Error while accessing LogFile %s \n", dnlif_LogFile);
		return FLASH_NO_SUCCESS;
	}

	fclose(fStream);
	return FLASH_SUCCESS;
}

/**
 * @brief	Function to write log to LogFile, to be used before
 * 		Flash_ulwInit
 * @param	arg_list - char ptr
 * @return 	FLASH_SUCCESS when EarlyLog written successfully
 * @author	Vigneshwaran Karunanithi(RBEI/ECA5)
 * @history
 * 		0.1 - Initial version
 * 		0.2 - Code cleanup.
 */
uint32_t dnlif_EarlyLog(const char *arg_list, ...)
{
	/*early log should be called with statement of less than 500 bytes*/
	char buffer[TMP_ARR_SIZE11] = " ";
	/* create buffer */
	va_list arg_ptr;
	char *format;

	va_start(arg_ptr, arg_list);
	format = (char *)arg_list;
	vsprintf(buffer, format, arg_ptr);

	if ((strlen(g_EarlyLog) + strlen(buffer)) < TMP_ARR_SIZE10)
	{
		strcat(g_EarlyLog, buffer);
	}

	return FLASH_SUCCESS;
}
/**
 * @brief	Helper function to ProcProgArgs function to parse the
 * 		input arg.
 * @param	argv -  char ptr
 * @return 	void
 * @author	Vigneshwaran Karunanithi(RBEI/ECA5)
 * @history
 * 		0.1 - Initial version(Vigneshwaran Karunanithi(RBEI/ECA5), 18/12/2015)
 */
void vProc_BasicParam(char *argv)
{
	if (!argv )
		return ;

	/* Option will allow program to print more debug info */
	if (strstr(argv, "/debug"))
	{
		ProgData.blDebOut = true;
	}

	/* Option to print precentage */
	if (!strncmp(argv, "/progress",SIZEOF("/progress")))
	{
		ProgData.blProgOut = true;
	}

	/* Option to avoid printing the precentage */
	if (!strncmp(argv, "/noprogress", SIZEOF("/noprogress")))
	{
		ProgData.blProgOut = false;
	}

	/* COM port setting parameter */
	if (strstr(argv, "/com="))
	{
		strcpy(ProgData.szComSettings, argv+SIZEOF("/com="));
	}

	/* Dnl file path */
	if (strstr(argv, "/dnl="))
	{
		strcpy(ProgData.szDnlFileName, argv+SIZEOF("/dnl="));
	}

	/* Log file path */
	if (strstr(argv, "/log="))
	{
		strcpy(ProgData.szLogFilePath, argv+SIZEOF("/log="));
		int len = strlen(ProgData.szLogFilePath);
		char temp_log[TMP_ARR_SIZE_KB/*1024*/];

		strcpy(temp_log, ProgData.szLogFilePath);

		if (!strstr(toUpper(temp_log), ".TXT"))
		{
			if ((ProgData.szLogFilePath[len-NEXT_INDEX] != '/')
				&& len)
			{
				strcat(ProgData.szLogFilePath, "/");
			}
		}
	}
}

/**
 * @brief	Helper function to ProcProgArgs function to parse the
 * 		input arg.
 * @param	argv -  char ptr
 * @return 	void
 * @author	Vigneshwaran Karunanithi(RBEI/ECA5)
 * @history
 * 		0.1 - Initial version(Vigneshwaran Karunanithi(RBEI/ECA5), 18/12/2015)
 */
void vProc_flashDecisionArg(char *argv)
{
	if (!argv)
	{
		return ;
	}
	/* v850's reset gpio no */
	if (strstr(argv, "/gpio_res="))
	{
		strncpy(ProgData.GPIO_Reset, argv+SIZEOF("/gpio_res="), 5);
	}

	/* v850's DNL-Enable gpio no */
	if (strstr(argv, "/gpio_vpp="))
	{
		strncpy(ProgData.GPIO_VPP,argv+SIZEOF("/gpio_vpp="), 5);
	}

	/* Check for update flash */
	if (!strncmp(argv, "/u", SIZEOF("/u")))
	{
		ProgData.blProgUpdate = true;
	}

	/* Check for virgin flash */
	if (!strncmp(argv, "/v", SIZEOF("/v")))
	{
		ProgData.blProgUpdate = false;
		ProgData.blProgVirgin = true;
	}

	/* Check for testmode flash */
	if (!strncmp(argv, "/testmode", SIZEOF("/testmode")))
	{
		ProgData.blTestMode = true;
	}

	/* Check for keep kds flag */
	if (!strncmp(argv, "/kdssave", SIZEOF("/kdssave")))
	{
		ProgData.blKDSSave = true;
	}

	/* Check for complete erase */
	if (!strncmp(argv, "/chiperase", SIZEOF("/chiperase")))
	{
		ProgData.blChipErase = true;
	}

	/* Check for keep alive mode option */
	if (!strncmp(argv, "/dataflasherase", 15))
	{
		ProgData.blDataFlashErase = true;
	}
}
/**
 * @brief 	Function to extract values from parameters present in
 * 		array argv
 * @param 	argc -Total number of parameters in array argv
 * @param 	argv -This array holds the parameters and its values.
 * @return 	void
 * @author	Vigneshwaran Karunanithi(RBEI/ECA5)
 * @history
 * 		0.1 - Initial version
 * 		0.2 - Code cleanup.
 */
void ProcProgArgs(int argc, char* argv[])
{
	if (argc > NEXT_INDEX)
	{
		for( int i = NEXT_INDEX; i < argc; i++ )
		{
			/* code breaked to reduce Cyclomatic value*/
			vProc_BasicParam(argv[i]);
			/* code breaked to reduce Cyclomatic value*/
			vProc_flashDecisionArg(argv[i]);

			/* MAUS file path - Maus opt will happen at end*/
			if (strstr(argv[i], "/maus="))
				strcpy(ProgData.szMAUSScript, argv[i]+SIZEOF("/maus="));

			/* KDS file path */
			if (strstr(argv[i], "/kds="))
				strcpy(ProgData.szKDSFileName, argv[i]+SIZEOF("/kds="));

			/* Old option to flash the blacks */
			if (strstr(argv[i], "/flash="))
			{
				char FlashBlock[TMP_ARR_SIZE4];
				strcpy(FlashBlock, argv[i]+SIZEOF("/flash="));
				dnlif_EarlyLog("EarlyLog: Flash Block: %s\n",
						FlashBlock);
				char *ptr = FlashBlock;
				int idx = INIT_INDEX;
				char *brk;

				while( (brk = strpbrk(ptr, " ,")) ||
					((*ptr != SUCCESS) &&
					(brk = (ptr+strlen(ptr)))) )
				{
					char blk[TMP_ARR_SIZE4] = {false};
					strncpy(blk,ptr,brk-ptr);
					ProgData.BlkData[idx] =
							strtoul(ptr, NULL, 16);
					dnlif_EarlyLog("EarlyLog: Flash Block ");
					dnlif_EarlyLog("[%d] = 0x%x\n", idx,
							ProgData.BlkData[idx]);
					idx++;
					ptr = brk + NEXT_INDEX;
				}
			}

			/* Old opt - to erase particular blacks */
			if (strstr(argv[i], "/erase="))
			{
				char EraseData[TMP_ARR_SIZE4];
				char Start[VAL_10] = {false};
				char Len[VAL_10] = {false};

				strcpy(EraseData, argv[i] + VAL_7);

				char *ptr;
				char *brk = strpbrk(EraseData, " ,\0");
				if (brk)
				{
					*brk = INIT_INDEX;
					g_u8EraseBlk = (ubyte)
						strtoul(EraseData,NULL,16);
					ptr = brk + NEXT_INDEX;
					brk = strpbrk(ptr, " ,\0");
					strncpy(Start, ptr, (brk - ptr));
					g_u32EraseStart =
						strtoul(Start, NULL, 16);
					ptr = brk + NEXT_INDEX;
					brk = (ptr + strlen(ptr));
					strncpy(Len, ptr, (brk - ptr));
					g_u32EraseLen =
						strtoul(Len, NULL, 16);
				}
			}
		} //for loop
	}    //if
}

/**
 * @brief 	Function sets the parameters suitable for testmode
 * @param 	void
 * @return	FLASH_SUCCESS if the the testmode settings done successfully
 * @author	Vigneshwaran Karunanithi(RBEI/ECA5)
 * @history
 * 		0.1 -Initial version
 * 		0.2 -Code cleanup work
 */
ulword dnlif_SetTestMode()
{
	memset(ProgData.szMAUSScript, 0, sizeof(ProgData.BlkData));

	/*disable update flash*/
	ProgData.blProgUpdate = false;
	/*disable virgin flash*/
	ProgData.blProgVirgin = false;

	memset(ProgData.BlkData, 0, sizeof(ProgData.BlkData));

	return FLASH_SUCCESS;
}

/**
 * @brief	Function to read the flash block details from dnl file
 * @param	void
 * @return 	FLASH_SUCCESS if the the read is successful
 * @author	Vigneshwaran Karunanithi(RBEI/ECA5)
 * @history
 * 		0.1 -Initial version
 * 		0.2 -Code cleanup work
 */
ulword dnlif_ulwReadFlashBlocks()
{
	int idx 	= INIT_INDEX;
	unsigned char *Help;
	ulword err = Flash_ulwGetDFContents((ubyte*)ProgData.szDnlFileName,
						&Help);

	if (FLASH_SUCCESS == err)
	{
		char *ptr = (char *) Help;
		char *endptr = strstr(ptr, ",");/*Search end of Versiondata*/
		bool err = false;

		memset(ProgData.BlkData, 0, sizeof(ProgData.BlkData));

		if (endptr)
		{
			ptr = ++endptr;/*skip Versionsdaten*/
			endptr = strstr(ptr, ",");
		}
		while (endptr && !err)
		{
			if (!(endptr = strstr(ptr, ",")))
			{
				break;
			}

			ulword Typ = strtoul(ptr, NULL, 16);

			if (ProgData.blProgVirgin ||
				(!(Typ & NEXT_INDEX)))
			{
				ProgData.BlkData[idx++] = Typ;
			}

			ptr = ++endptr;
			endptr = strstr(ptr, ",");
			if (endptr)
			{
				/*skip start address*/
				ptr = ++endptr;
				endptr = strstr(ptr, ",");

				if (endptr)
				{
					/*skip len*/
					ptr = ++endptr;
					endptr = strstr(ptr, ",");
					/* skip CS */
					if (endptr)
						ptr = ++endptr;
				}
				else
					err = true;
			}
			else
				err = true;

			if (err)
			{
				printf("Unexpected end of Data in receive from "
						"Flash_ulwGetDFContents()");
				return FLASH_DF_FILE_HEADER_ERROR;
			}
		} /*while end */
	}

	return FLASH_SUCCESS;
}

/**
 * @brief	Function to log parameters specified in flash.cfg
 * 		& assign to flash params
 * @return FLASH_SUCCESS after successful initialization of flash params
 * @author	Vigneshwaran Karunanithi(RBEI/ECA5)
 * @history
 * 		0.1 -Initial version
 * 		0.2 -Code cleanup work
 */
ulword dnlif_InitFlashParams()
{
	dnlif_EarlyLog("EarlyLog: Com-Setting = %s\n",
			ProgData.szComSettings);
	dnlif_EarlyLog("EarlyLog: Progress flag is %s\n",
			(ProgData.blProgOut)? "True":"False");
	dnlif_EarlyLog("EarlyLog: Debug flag is %s\n",(ProgData.blDebOut)?
			"True":"False");
	dnlif_EarlyLog("EarlyLog: Update-flash flag is %s\n",
			(ProgData.blProgUpdate)? "True":"False");
	dnlif_EarlyLog("EarlyLog: LogFile path is %s\n",
			ProgData.szLogFilePath);
	dnlif_EarlyLog("EarlyLog: DNL file path is %s\n",
			ProgData.szDnlFileName);
	dnlif_EarlyLog("EarlyLog: Reset-GPIO is %s\n",
			ProgData.GPIO_Reset);
	dnlif_EarlyLog("EarlyLog: VPP-GPIO is %s\n",
			ProgData.GPIO_VPP);

	/*write below log only if corresponding argument is enabled */
	if (ProgData.blProgVirgin)
	{
		dnlif_EarlyLog("EarlyLog: Virgin-flash flag is %s\n",
				(ProgData.blProgVirgin)? "True":"False");
	}

	if (ProgData.blTestMode)
	{
		dnlif_EarlyLog("EarlyLog: Testmode flag is %s\n",
				(ProgData.blTestMode)? "True":"False");
	}

	if (ProgData.blKDSSave)
	{
		dnlif_EarlyLog("EarlyLog: KDSSave flag is %s\n",
				(ProgData.blKDSSave)? "True":"False");
	}

	if (ProgData.blChipErase)
	{
		dnlif_EarlyLog("EarlyLog: ChipErase flag is %s\n",
				(ProgData.blChipErase)? "True":"False");
	}

	if ('\0' != ProgData.szMAUSScript[0])
	{
		dnlif_EarlyLog("EarlyLog: MAUS Script file is %s\n",
				ProgData.szMAUSScript);
	}

	if ('\0' != ProgData.szKDSFileName[0])
	{
		dnlif_EarlyLog("EarlyLog: KDS file is %s\n",
				ProgData.szKDSFileName);
	}

	if (ProgData.blDataFlashErase)
	{
		dnlif_EarlyLog("EarlyLog: DataFlagErase flag is %s\n",
				(ProgData.blDataFlashErase)? "True":"False");
	}

	/*changed from 250 to 500 */
	g_rFlashParms.uwTelegramTimeOut = TELE_TIMEOUT;
	g_rFlashParms.ubTelegramLength = TELE_LEN;/* changed from [0x80]*/
	g_rFlashParms.ubVerify = true;/* 1 for ON */
	g_rFlashParms.ubReadSignature = true;/* 1 for ON */
	g_rFlashParms.ubTelegramRetries = TELE_RETRY;/*[1..10]*/
	g_rFlashParms.ubProgrammingRetries= PRG_RETRY;
	g_rFlashParms.ubEraseTimeOut = ERASE_TIMEOUT;/*changed from 200 to 20*/
	g_rFlashParms.ubWriteTimeOut = WRITE_TIMEOUT;
	g_rFlashParms.uwPreDnlDelay = PRE_DNL_DELAY;
	g_rFlashParms.uwFLWTimeOut = FWL_TIMEOUT;/*early it is commented*/
	g_rFlashParms.ubWriteRandomEnable = true;
	g_rFlashParms.ubLogEnable = true;
	g_rFlashParms.ubIgnoreOptionFlags = false;
	g_rFlashParms.pRS232 = NULL;/*Filled by Flash Module*/
	/*Logfilewrite Path*/
	g_rFlashParms.LogPath = (char *)ProgData.szLogFilePath;

	/*GPIO address for Reset*/
	g_rFlashParms.GPIO_Reset = (char *)ProgData.GPIO_Reset;
	/*GPIO address for VPP */
	g_rFlashParms.GPIO_VPP = (char *)ProgData.GPIO_VPP;

	strcpy(gComPort,ProgData.szComSettings);

	return FLASH_SUCCESS;

}

/**
 * @brief 	Helper function to dnlif_ulwDownload
 * @return 	void
 * @param	ulwMode -ulword ptr
 * @author	Vigneshwaran Karunanithi(RBEI/ECA5)
 * @history
 * 		0.1 -Initial version(created to reduce CC value)
 */
void  vModeUpdate(ulword *ulwMode)
{
	if (!ProgData.blTestMode)
	{
		/* disable Testmode if not required */
		*ulwMode |= FLASH_MODE_TESTMODE;
	}
	if (ProgData.blProgVirgin)
	{
		/* disable Update if virgin flash */
		*ulwMode |= FLASH_MODE_UPDATE;
	}

	if (!ProgData.blDataFlashErase)
	{
		/* disable DataFlashErase */
		*ulwMode |= FLASH_MODE_DATAFLASH_ERASE;
	}

	LogFileWrite("v850_dnl_if: Call Flash_ulwDoConnect with Portinfo: %s",
			gComPort);

	/*update with percentage of download as 0*/
	if (ProgData.blProgOut)
	{
		if (g_fCallback)
		{
			g_fCallback(0);
		}
	}
}

/**
 * @brief 	Helper function to dnlif_ulwDownload
 * @param	void
 * @return 	void
 * @author	Vigneshwaran Karunanithi(RBEI/ECA5)
 * @history
 * 		0.1 -Initial version(created to reduce CC value)
 */
ulword vStartFlashing()
{

	if (ProgData.blProgVirgin &&
		(ProgData.szKDSFileName[INIT_INDEX]))
	{
		if (FileExists(ProgData.szKDSFileName))
			Flash_vSetKDSXchgBC(KDS_XChg);
		else
			LogFileWrite("KDS-Blockfile cannot be found."
					"Default data will be written");
	}

	LogFileWrite("Call Flash_ulwDoDownloadFileTransfer()");

	/* deciding flashing mode */
	ulword ulwMode = ProgData.blProgVirgin ? MODE_VIRG : MODE_UPDATE;

	if (ProgData.blChipErase)
	{
		/*enable ChipErase*/
		ulwMode &= ~FLASH_MODE_CHIP_ERASE;
	}
	if (ProgData.blKDSSave)
	{
		/*enable KDSSave*/
		ulwMode &= ~FLASH_MODE_KDS_SAVE;
	}

	return (Flash_ulwDoDownloadFileTransfer(&ProgData.BlkData[INIT_INDEX],
						ulwMode, 0x0));
}

/**
 * @brief 	Function to perform flashing steps
 * @return 	FLASH_SUCCESS if it is successful/error message
 * @param	void
 * @author	Vigneshwaran Karunanithi(RBEI/ECA5)
 * @history
 * 		0.1 -Initial version
 * 		0.2 -Code cleanup work
 */
ulword dnlif_ulwDownload()
{
	ulword Err = FLASH_SUCCESS;

	if (ProgData.blProgOut)
	{
		LogFileWrite("v850_dnl_if: Set progress output vector");
		Flash_vSetProgressBC(UpdateReport);
	}

	if (ProgData.blDebOut)
	{
		LogFileWrite("v850_dnl_if: Set debug output vector");
		Flash_vSetDebugBC(UpdateReport);
		/* if Debug data must be routed to a separate output, the following
		   function can be used instead. But in that case it is more difficult
		   to assign the communication data to the text output which gives a
		   hint about the program activities.
		*/
	}

	strcpy(gComPort,ProgData.szComSettings);

	/* set correct Mode flag register */
	ulword ulwMode = DEVICE_V850E2M +
				FLASH_MODE_GLOB_ERASE +
				FLASH_MODE_FAST_PROG +
				FLASH_MODE_FIX_LOAD_ADDR +
				FLASH_MODE_CHIP_ERASE;

	/* code breaked for CC */
	vModeUpdate(&ulwMode);

	ubyte ubToggleCnt = false;

	/* establish communication with v850*/
	Err = Flash_ulwDoConnect((ubyte *)gComPort,
				(ubyte *)ProgData.szDnlFileName,
				ulwMode, ubToggleCnt);

	if ((FLASH_SUCCESS == Err) ||
		((FLASH_INVALID_OPTIONSETTINGS == Err) &&
				ProgData.blChipErase))
	{
		Err = FLASH_SUCCESS;

		/*perform download only if testmode is false*/
		if (!ProgData.blTestMode)
		{
			/*Erase job*/
			if ((!Err) && (g_u8EraseBlk))
			{
				LogFileWrite("Call Flash_ulwEraseMemory with"
						" Parameter(%x,%x,%x)"
						,g_u8EraseBlk, g_u32EraseStart,
						g_u32EraseLen);

				Err = Flash_ulwEraseMemory(g_u8EraseBlk,
								g_u32EraseStart
								, g_u32EraseLen
								);
			}

			/*Flash job */
			if ((!Err) && (ProgData.BlkData[INIT_INDEX]))
				Err = vStartFlashing();

			/*MAUS Script-file */
			if ((!Err) && FileExists(ProgData.szMAUSScript))
			{
				Err = MAUSProcessing(ProgData.szMAUSScript,
							Flash_ulwMausSend,
							Flash_ulwMausReceive);
			}
		}/*blTestmode*/
	}/*Flash_ulwDoConnect*/

	return Err;
}

/**
 * @brief 	Helper function for Flash_ulwDownloadFile to init
 * 		basic operations.
 *
 * @return 	SUCCESS if it is successful/error message
 * @param	szDownloadFilePath - dnl file path/NULL
 * @author	Vigneshwaran Karunanithi(RBEI/ECA5)
 * @history
 * 		0.1 -Initial version
 * 		0.2 -Code cleanup work
 */
ulword ulBasic_init(const char* szDownloadFilePath)
{
	char **args;
	int c;/*First try to read configfile*/

	printf("Libvirgin %s\n",VERSION);

	dnlif_EarlyLog("EarlyLog: Libvirgin %s\n", VERSION);

	if (!cfg_get(FLASH_CFG, &c, &args))
	{
		ProcProgArgs(c, args);
		cfg_free(c, args);
	}
	else
	{
		printf("Error in reading %s\n", FLASH_CFG);
		return FLASH_NO_SUCCESS;
	}

	if (dnlif_LogFileEnable())
		return FLASH_NO_SUCCESS;

	if (szDownloadFilePath)
	{
		/*take the file path passed with API*/
		strcpy(ProgData.szDnlFileName, szDownloadFilePath);
	}
	if (ProgData.blTestMode)
	{
		dnlif_SetTestMode();
	}
	else if ((ProgData.blProgUpdate || ProgData.blProgVirgin) &&
			FileExists((char *)ProgData.szDnlFileName))
	{
		dnlif_ulwReadFlashBlocks();
	}


	g_u8NumberOfBlocks = INIT_INDEX;

	/*Count the number of blocks to be flashed*/
	for (int i = INIT_INDEX; ProgData.BlkData[i]; i++)
	{
		++g_u8NumberOfBlocks;
	}

	return SUCCESS;
}

/**
 * @brief 	Flash_ulwDownloadFile is a single point interface which
 * 		performs the overall flash download  of the file provided
 * 		in szDownloadFilePath and it updates the progress into the
 * 		function specified in fCallback
 * @param 	fCallback Callback function to which download progress status will be
 *        	updated
 * @param 	szDownloadFilePath The exact path of master.dnl
 * @return 	result of Flash download
 * @author	Vigneshwaran Karunanithi(RBEI/ECA5)
 * @history
 * 		0.1 -Initial version
 * 		0.2 -Code cleanup work
 */
ulword Flash_ulwDownloadFile(T_fCallback fCallback,
                             const char* szDownloadFilePath)
{


	if (ulBasic_init(szDownloadFilePath))
	{
		return FLASH_NO_SUCCESS;
	}
	/*Update the calll back function*/
	g_fCallback = fCallback;

	/*To return error if no valid blocks & testmode is false*/
	if ((g_u8NumberOfBlocks == SUCCESS) && !(ProgData.blTestMode) &&
	                ('\0' == ProgData.szMAUSScript[INIT_INDEX]))
	{
		dnlif_EarlyLog("EarlyLog: No flash blocks specified");
		return FLASH_DF_FILE_IO_ERROR;
	}

	if ('\0' != ProgData.szMAUSScript[INIT_INDEX])
	{
		if (!(FileExists(ProgData.szMAUSScript)))
		{
			dnlif_EarlyLog("EarlyLog: Err in access Maus-Script");
			return FLASH_MAUS_SEQ_FILE_IO_ERROR;
		}
	}

	dnlif_InitFlashParams();
	ulword Err = FLASH_SUCCESS;
	dnlif_EarlyLog("EarlyLog: Call Flash_ulwInit\n");

	/*If Flash init is true, then only perform flash connect & download*/
	if (!(Err = Flash_ulwInit ( (ubyte *)gComPort, &g_rFlashParms)))
	{
		LogFileWrite(g_EarlyLog);
		Err = dnlif_ulwDownload();
	}

	/*Flash shutdown should not be carried out in testmode*/
	if (!ProgData.blTestMode)
	{
		Flash_ulwDoReset();
		Flash_ulwShutDown();
	}

	if (FLASH_SUCCESS == Err && ProgData.blProgOut)
	{
		/*update with percentage of download as 0*/
		if (g_fCallback)
		{
			g_fCallback(VAL_100);
		}
	}

	return Err;
	//End of the main running loop -- END
}

/**
 * @brief	function is called by the flash library to load the
 * 		KDS-data from a file and pass the data to the pointer
 * 		given as parameter.
 * @param	ptrKDS	- Kds data ptr
 * @param	blDummy - bool value
 * @return 	Reports the result of the existence of the KDS-data file to
 *         	indicate that the data is valid
 * @author	Vigneshwaran Karunanithi(RBEI/ECA5)
 * @history
 * 		0.1 -Initial version
 * 		0.2 -Code cleanup work
 */
ulword KDS_XChg(char *ptrKDS, bool blDummy)
{
	LogFileWrite("XChange KDS-Data");

	FILE *stream = fopen(ProgData.szKDSFileName, "rb");

	if (stream)
	{
		ulword KDS_Size;
		unsigned char *pubData = GetDF_Data(stream, 0x40ED, &KDS_Size);

		if (pubData)
		{
			ulword idx = INIT_INDEX;
			for (ulword i = KDS_Size; i> INIT_INDEX; i--)
			{
				*(ptrKDS+idx) = *(pubData+i-NEXT_INDEX);
				idx++;
			}
			free(pubData);
		}
		else
		{
			fclose(stream);
			return FLASH_NO_SUCCESS;
		}
	}
	else
	{
		LogFileWrite("KDS-Data file not available");
		return FLASH_NO_SUCCESS;
	}
	fclose(stream);

	return FLASH_SUCCESS;
}

/**
 * @brief	Function is called to read the blockdata from the File
 * @param	f_DFile - file ptr (dnl file)
 * @param	BlockTyp - Used to give idea about the block
 * @paran	BlockLen - Used	to provide length of the block.
 * @return 	Reports the result of finding the data in the file by returning
 *         	the pointer to the Data or NULL. The memory must be freed in the
 *         	calling function.
 * @author	Vigneshwaran Karunanithi(RBEI/ECA5)
 * @history
 * 		0.1 -Initial version
 * 		0.2 -Code cleanup work
 */
unsigned char * GetDF_Data(FILE *f_DFile, ulword BlockTyp, ulword *BlockLen)
{
	rewind(f_DFile);

	long FileLen = filelength(f_DFile);
	unsigned char buf[TMP_ARR_SIZE12 /*255*/];
	unsigned int uiPos = INIT_INDEX;
	unsigned char *DataBuf;
	ulword Typ;

	while (!fseek(f_DFile, uiPos, SEEK_SET))
	{
		fread((void*)buf, 0x80, 1, f_DFile);/*read header*/

		unsigned int FixLen = (buf[VAL_14] << VAL_8) + buf[VAL_15];
		unsigned int Len = (buf[VAL_6] << VAL_24) +
					(buf[VAL_7] << VAL_16) +
					(buf[VAL_8]<<VAL_8) +
					buf[VAL_9];

		Typ = ((buf[INIT_INDEX] << VAL_8) + buf[NEXT_INDEX]);        // Check type

		if (Typ == BlockTyp)
		{
			ulword Pos = uiPos + FixLen;

			if (fseek(f_DFile, Pos, SEEK_SET))
				return NULL;

			*BlockLen = Len;
			DataBuf = (unsigned char *)malloc(Len);

			if (fread((void *)DataBuf, Len, 1, f_DFile))
			{
				return DataBuf;
			}

			delete DataBuf;
			return NULL;
		}

		if (FixLen)
		{
			unsigned int Rest = Len % FixLen;
			if (Rest) Len += FixLen - Rest;
		}
		else
		{
			FixLen = DEF_LEN;/* standard length of Header if 0*/
		}
		Len += FixLen;
		uiPos += Len;
		if ((long)uiPos >= FileLen)
			break;
	}

	return NULL;
} /* end of GetDF_Data() */
//---------------------------------------------------------------------------

/**
 * @file 	serial.h
 * @brief 	Hander file for  Serial(serial.cpp) communication.
 * @author 	Satyanarayana Venkatesh (RBEI/ECG2)
 * 		Kumar K - (RBEI/ECA1)
 * @copyright 	(c) 2013 Robert Bosch GmbH
 * @version 	0.2
 * @history
 * 		0.1 - initial version
 * 		0.2 - code cleaned.
 */

#ifndef v850_dnl_if_h
#define v850_dnl_if_h

extern "C"
{

/*---------------- Return value defines ----------------------------- */
/*---------------- From connect functions --------------------------- */

/**no error*/
#define FLASH_SUCCESS                     0x00   
/**common error*/
#define FLASH_NO_SUCCESS                  -1     
/**Unknown Type in DF-Header*/
#define FLASH_DF_FILE_HEADER_ERROR        0x30
/**Header- or Data block- mismatch*/   
#define FLASH_DF_FILE_CRC_ERROR           0x31 
/**File-Open Error in DF*/  
#define FLASH_DF_FILE_IO_ERROR            0x32   
/**DataBlock not found in DF-Header*/
#define FLASH_DF_FILE_CONTENT_ERROR       0x33
/**Device Connection did not response*/
#define FLASH_CONNECT_FAILED              0x34 
/** No response on Bootstrap request*/
#define FLASH_BOOTSTRAP_FAILED            0x35
/** No response from Maus request*/
#define FLASH_REQ_MAUS_FAILED             0x36
/** No response from Introloader*/
#define FLASH_INTROLOADER_FAILED          0x37
/** No response from Flashwriter*/
#define FLASH_FLASHWRITER_FAILED          0x38
/** Error in Introloader Data-Block*/
#define FLASH_INTROLOADER_DF_ERROR        0x39
/** Introloader > 256 Bytes*/
#define FLASH_INTROLOADER_SIZE_TOO_BIG    0x3A
/** Error in Flashwriter Data-Block*/
#define FLASH_FLASHWRITER_DF_ERROR        0x3B
/** Request for DevInfo failed*/
#define FLASH_DEVINFO_FAILED              0x3C
/** reject cmd, because Bootstrap*/
#define FLASH_DEVINFO_REJECT              0x3D
/** cannot set Bautrate to init UART*/
#define FLASH_BAUDRATE_SET_FAILED         0x3E
/** cannot write History to E2Prom*/
#define FLASH_HISTORY_WRITE_FAILED        0x3F
/** Vpp drop detected by Hardware Interface*/
#define FLASH_VPP_DROP                    0x40
/** Vcc drop detected by Hardware Interface*/
#define FLASH_VCC_DROP                    0x41
/** no access during active tranfer*/
#define FLASH_ACCESS_DENIED               0x42
/** Mkey.exe not found for connect*/
#define FLASH_NO_MKEY_EXE                 0x43
/** wrong Mkey code generated*/
#define FLASH_WRONG_MKEYCODE              0x44
/** Maus-Server not found*/
#define FLASH_NO_MKEY_SERVER              0x45
/** Invalid data in device information (Version conflict!)*/
#define FLASH_DF_FILE_UNKNOWN_SIB_DATA    0x46
/** not supported SIB-Version*/
#define FLASH_DF_FILE_INVALID_SIB_VERSION 0x47
/** abort last command*/
#define FLASH_ABORTED                     0x50
/** error during Maus-Sequence file*/
#define FLASH_SEQ_ERROR                   0x60
/** Maus-Sequence file open error*/
#define FLASH_MAUS_SEQ_FILE_IO_ERROR      0x61
/** Error in CS for Introloader*/
#define FLASH_INTROLOADER_CS_ERROR        0x70
/** Compatibility Nr is different*/
#define FLASH_COMP_NUM_INVALID            0x71

/** No success in comparing IDB*/
#define FLASH_IDB_MISMATCH                0x80
/** FX4 has invalid option*/
#define FLASH_INVALID_OPTIONSETTINGS      0x90
/** FX4 can't read Security Setting*/
#define FLASH_REJECT_OPTIONSETTINGS       0x91

/*------------- From Maus functions ----------------------------*/

/** Vpp error*/
#define FLASH_VPP_ERROR                   0x01
/** gen. Pwr fail (low voltage)*/
#define FLASH_POWER_FAIL                  0x02
/** Device cannot be erased*/
#define FLASH_ERASE_FAIL                  0x03
/** Device cannot be written*/
#define FLASH_WRITE_FAIL                  0x04
/** Address out of range*/
#define FLASH_ADDRESS_ERROR               0x05
/** Invalid Address access*/
#define FLASH_ACCESS_ERROR                0x06
/** Device not present*/
#define FLASH_DEVICE_NOT_PRESENT          0x07
/** Device protected by Hardware*/
#define FLASH_DEVICE_PROTECTED            0x08
/** command rejected*/
#define FLASH_REJECT                      0x09
/**not yet defined error*/
#define FLASH_OTHER_ERROR                 0x0A

/*--------- From other functions -----------------------------*/
/** COM setting failure */
#define FLASH_COM_SET_FAIL                0x10
/**COM tx/write failed */
#define FLASH_COM_WRITE_FAIL              0x11
/**COM read/rx failed */
#define FLASH_COM_READ_FAIL               0x12
/** COM operation timeout */
#define FLASH_COM_TIMEOUT                 0x13
/** TMR_NAV */
#define FLASH_PERFORM_TMR_NAV             0x14
/**common system error (alloc etc)*/
#define FLASH_SYSTEM_ERROR                0x15
/** Port Parm error*/
#define FLASH_PARM_ERROR                  0x16

/** CRC-Error on verify with DNL*/
#define FLASH_DEVICE_CRC_ERROR            0x23
#define FLASH_DEVICE_NOT_BLANK            0x22
/** CRC-Error/ Wrong data block type or Nr*/
#define FLASH_DOWNLOAD_ERROR              0x21 

//==============================================================================
//                               Function interfaces
//==============================================================================

typedef uint32_t (*T_fCallback)(uint8_t u8DownloadProgress);
/*
 * Flash_ulwDownloadFile is a single point interface which 
 * performs the overall flash download  of the file provided
 * in szDownloadFilePath and it updates the progress into the
 * function specified in fCallback.
 */
uint32_t Flash_ulwDownloadFile(T_fCallback fCallback,
      				const char* szDownloadFilePath);
}

#endif//v850_dnl_if_h

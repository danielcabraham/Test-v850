/**
 * @file Main.cpp
 * @brief Test application to  test the API of libvirgindownload
 * @param GPIO_RESET
 * @param GPIO_VPP
 * @return If flashing is successful "Result ok" is returned, else corresponding
 *       error will be returned
 */
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include "v850_dnl_if.h"
#include <string.h>

#define SIZEOF(x) (sizeof(x)-1)
#define VERSION "1.0.5"
//=============================================================================
//    MAIN LOOP
//
//  Basically designed just for the testing purpose.
//=============================================================================


uint32_t UpdateReport(uint8_t u8PercentageOfDownload );
const char * Err_Report(uint32_t err);
void display_help();


int main(int argc, char* argv[])
{
   char szDnlFileName[512];
   const char* pszDnlFileName = NULL;
   uint32_t ErrorCode = 0;
#ifndef FLW
   printf("VirginDownload Application %s\n", VERSION);
#else
   printf("Flashwriter %s\n", VERSION);
#endif

   if (argc > 1){
      for( int i = 1; i < argc; i++ ){
         if (strstr(argv[i],"/dnl=") != 0){
            strcpy(szDnlFileName,argv[1]+SIZEOF("/dnl="));
            pszDnlFileName = szDnlFileName;
         }
         else if (strncmp(argv[i],"-v",SIZEOF("-v")) == 0){
            printf("version-%s\n", VERSION);
            goto RETURN_ERR;
         }
         else if ((strncmp(argv[i],"-h",SIZEOF("-h")) == 0)||(strncmp(argv[i],
               "--help",SIZEOF("--help")) == 0)){
            display_help();
            goto RETURN_ERR;
         }
         else{
            printf("Invalid argument provided\n");
            display_help();
            ErrorCode = FLASH_NO_SUCCESS;
            goto RETURN_ERR;
         }
      }
   }

   ErrorCode = Flash_ulwDownloadFile(UpdateReport, pszDnlFileName);
   printf("\nResult %s\n",Err_Report(ErrorCode));

   RETURN_ERR:
   return ErrorCode;
}


/*
uint32_t UpdateReport(char *Data, bool newline)
{
  if (Data[0] != 0)
  {  if (Data[0] == '\n')
     {  Data++;
        printf("\n");
     }
	 if (newline) printf("%s\n",Data);
  	 else printf("%s\r",Data);
     fflush(stdout);
  }
  return FLASH_SUCCESS;
}
*/
/**
 * @brief Function which prints flash process status
 * @param u8PercentageOfDownload Percentage of flash completed
 * @return v850 flash process result
 */
uint32_t UpdateReport(uint8_t u8PercentageOfDownload)
{
  printf("\rPercentage of Download : %d %%", u8PercentageOfDownload);
  fflush(stdout);
  return FLASH_SUCCESS;
}

/**
 * @brief Function to print help information
 */
void display_help()
{
   printf("Usage: virgindownloadtestapp <argument>, argument is optional\n");
   printf("\nArgument can be of following type\n");
   printf("\t -h : to get this help\n");
   printf("\t -v : to get the version\n");
   printf("\t /dnl=<path to master.dnl> : to specify the dnl to be flashed\n");
}

const char * Err_Report(uint32_t err)
{
  switch (err)                       // Fehlermeldungen aus Error dekodieren
  { case FLASH_SUCCESS:
         return "Okay";
    case FLASH_PARM_ERROR:
         return "Error: Missing parameter for port configuration";
    case FLASH_NO_SUCCESS:
         return "Last command was not successful";
    case FLASH_DF_FILE_HEADER_ERROR:
         return "Unknown Type in DF-Header";
    case FLASH_DF_FILE_CRC_ERROR:
         return "CRC-Error in DF-Header or Data Block";
    case FLASH_DF_FILE_IO_ERROR:
         return "File I/O Error in DF-File";
    case FLASH_DF_FILE_UNKNOWN_SIB_DATA:
         return "Unknown Data in SIB-Table of DNL-Device Info";
    case FLASH_DF_FILE_INVALID_SIB_VERSION:
         return "Invalid Version of SIB-Table in DNL-Device Info";
    case FLASH_DF_FILE_CONTENT_ERROR:
         return "Invalid or missing data of DNL-Device Info";
    case FLASH_COM_SET_FAIL:
         return "Set of Com-Port failed";
    case FLASH_COM_WRITE_FAIL:
         return "Write to Com-Port failed";
    case FLASH_COM_READ_FAIL:
         return "Read from Com-Port failed";
    case FLASH_COM_TIMEOUT:
         return "Error: Time Out on Com-Port";
    case FLASH_PERFORM_TMR_NAV:
         return "Windows Performance Timer not available or GPIO set failure";
    case FLASH_CONNECT_FAILED:
         return "Device Connection did not response";
    case FLASH_BOOTSTRAP_FAILED:
         return "No response on Bootstrap request";
    case FLASH_REQ_MAUS_FAILED:
         return "No response from Maus request";
    case FLASH_INTROLOADER_FAILED:
         return "No response from Introloader";
    case FLASH_FLASHWRITER_FAILED:
         return "No response from Flashwriter";
    case FLASH_INTROLOADER_DF_ERROR:
         return "DF-File error in Introloader";
    case FLASH_INTROLOADER_SIZE_TOO_BIG:
         return "Error: Size for Introloader int DF-File too large";
    case FLASH_FLASHWRITER_DF_ERROR:
         return "DF-File error in Flashwriter";
    case FLASH_DEVINFO_FAILED:
         return "No response for Device-Info request";
    case FLASH_DEVINFO_REJECT:
         return "Device-Info cannot requested, because Bootstrap is already "
               "requested";
    case FLASH_BAUDRATE_SET_FAILED:
         return "cannot set Bautrate to init UART";
    case FLASH_VPP_ERROR:
         return "VPP Error";
    case FLASH_POWER_FAIL:
         return "gen. Pwr fail (low voltage)";
    case FLASH_ERASE_FAIL:
         return "Device cannot be erased";
    case FLASH_WRITE_FAIL:
         return "Device cannot be written";
    case FLASH_ADDRESS_ERROR:
         return "Error: Address out of range";
    case FLASH_ACCESS_ERROR:
         return "Error: Invalid Address access";
    case FLASH_DEVICE_NOT_PRESENT:
         return "Error: Device not present";
    case FLASH_DEVICE_PROTECTED:
         return "Error: Device is protected by Hardware";
    case FLASH_SYSTEM_ERROR:
         return "no success for actual cmd. System error detected";
    case FLASH_DEVICE_NOT_BLANK:
         return "Device is not blank";
    case FLASH_REJECT:
         return "command rejected";
    case FLASH_VPP_DROP:
         return "Error: VPP-Drop detected";
    case FLASH_VCC_DROP:
         return "Error: VCC-Drop detected";
    case FLASH_ACCESS_DENIED:
         return "Error: Command ignored, because transfer is in progress";
    case FLASH_ABORTED:
         return "Last command aborted by user";
    case FLASH_DOWNLOAD_ERROR:
         return "Download error: CRC is wrong";
    case FLASH_INTROLOADER_CS_ERROR:
         return "Checksum error during transmit the Introloader";
    case FLASH_NO_MKEY_EXE:
         return "MKey.exe not found in working directory";
    case FLASH_WRONG_MKEYCODE:
         return "No key generated by MKey.exe";
    case FLASH_NO_MKEY_SERVER:
         return "Maus-Server not available";
    case FLASH_COMP_NUM_INVALID:
         return "Update flash not possible, because compatibility number "
               "is different";
    case FLASH_INVALID_OPTIONSETTINGS:
         return "Error: FX4-OptionBytes are invalid - Chip erase necessary";
    case FLASH_REJECT_OPTIONSETTINGS:
         return "Error: FX4-OptionBytes and Secure Settings can't be read";
    case FLASH_MAUS_SEQ_FILE_IO_ERROR:
         return "Error: Maus-Sequence file open error";
    case FLASH_OTHER_ERROR:
    default:
         return "unknown error #";
  }
}
//---------------------------------------------------------------------------

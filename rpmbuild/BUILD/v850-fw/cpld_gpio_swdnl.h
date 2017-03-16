/**
 * @file        cpld_gpio_swdnl.h
 * @brief       Act as header file for cpld_gpio_swdnl.cpp.
 * @author      Satyanarayana Venkatesh - RBEI/ECG2
 * 		kuk5cob<Kumar.Kandasamy@in.bosch.com>
 * @version     0.2
 * @copyright   (c) 2015 Robert Bosch GmbH
 * @history     
 *      0.1     -initial version
 *      0.2     -code cleaning (16/12/2015,vrk5cob).
 */

#ifndef _CPLD_GPIO_SWDNL_H
#define _CPLD_GPIO_SWDNL_H

#include "v850_Macro.h"

/**Macro for gpio export file's path*/
#define GPIO_EXPORT	"/sys/class/gpio/export"
/**Macro for gpio unexport file's path*/
#define GPIO_UNEXPORT	"/sys/class/gpio/unexport"
/**Macro provide access to particular gpio*/
#define GPIO_PATH	"/sys/class/gpio/gpio%s"

/***************** Function Declarations *******************/
FILE* fp_createGpioOut(const char* chGpioNum);
void vWriteGpioOut(FILE* fp, const char* gpioval);
void vDeleteGpioOut(const char* gpionum);
bool bSetGpioPin(FILE* fp, const char* pGpioNum, bool bset);
/****************   END ***********************************/

#endif //_CPLD_GPIO_SWDNL_H

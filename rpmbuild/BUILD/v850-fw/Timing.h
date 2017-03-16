/**
 * @file	Timing.h
 * @copyright 	(c) 2013 Robert Bosch GmbH
 * @brief 	Header file for Timing.cpp.
 * @version 	0.2
 * @date  	16.12.2015
 * @author	kuk5cob<Kumar.Kandasamy@in.bosch.com>
 * @history	
 * 		0.1 -Initial version
 * 		0.2 -Code cleanup work
 */

#ifndef Timing_h__
#define Timing_h__

extern "C" {
#include <unistd.h>
#include <signal.h>
}

bool TimeInit(void);
bool TimeExit(void);
/*Just waits for uSec ms to pass*/
bool TimeWait(unsigned long uSec);
/* TimeSetStart sets a point in time t0 and
 * the counterpart TimeWaitTill waits
 * the number of µs since t0 or returns
 * immediately if we are past t0 + uSec.
 */
bool TimeSetStart(void);
unsigned long TimeWaitTill(unsigned long uSec);
/* These four functions perform same, for
 * timer 1 and timer 2 respectively:
 * TimeSetStop sets the number of ms to elapse (ulOffset),
 * counted on from call of that functions.
 * and TimeIsStop just polls if ulOffset ms have passed.
 */
bool TimeSetStop(unsigned long ulOffset);
bool TimeIsStop(void);
bool TimeSetStop2(unsigned long ulOffset);
bool TimeIsStop2(void);
#endif //Timing_h__

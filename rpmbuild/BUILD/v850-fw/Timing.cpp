/**
 * @file	Timing.cpp
 * @copyright 	(c) 2013 Robert Bosch GmbH
 * @brief 	Handles the Timing Functionality
 * @version 	0.2
 * @author	kuk5cob<Kumar.Kandasamy@in.bosch.com>
 * @date  	16.12.2015
 * @history	
 * 		0.1 -Initial version
 * 		0.2 -Code cleanup work
 */

#include <time.h>
#include "globals.h"
#include "Flash850.h"
#include "flash.h"
#include "Timing.h"
#include "v850_Macro.h"
/**Max allowed timers struct variable*/
#define MAX_TIMER	2

/** timers is a structure which is used to
 *  clock operations of the Timing.cpp*/ 
struct timers
{
	/**This is obj for timer_t*/
	timer_t id;
	/**Used to track timer creation*/
	bool valid;

} timer[MAX_TIMER] = { { 0 } };

/**Gobal structure variable used to hold the system clock(CLOCK_REALTIME)*/
struct timespec timeT0 = { 0 };



//-----------------------------------------------------------------------------//
//------------------------ begin of Modul routines ----------------------------//
//-----------------------------------------------------------------------------//


/**
 * @brief	To create the timers.
 * @param	void
 * @return 	True if timer is created successfully else False is returned
 * @author	Vigneshwaran Karunanithi(RBEI/ECA5)
 * @history	
 * 		0.1 -Initial version
 * 		0.2 -Code cleanup work
 */
bool TimeInit(void)
{
	bool ret = true;

	for(timers *t = timer; t <  
		(timer +  (sizeof(timer)/sizeof(timer[0]))); t++)
	{
		struct sigevent se = { { 0 } };

		se.sigev_notify = SIGEV_NONE;

		if (false != timer_create(CLOCK_REALTIME, &se, &t->id))
		{
			ret = false;
		}

		t->valid = ret;
	}

	return ret;
}

/**
 * @brief 	To delete the timers
 * @param	void
 * @return 	True if timer is deleted successfully else False is returned
 * @author	Vigneshwaran Karunanithi(RBEI/ECA5)
 * @history	
 * 		0.1 -Initial version
 * 		0.2 -Code cleanup work
 */
bool TimeExit(void)
{
	for(timers *t = timer;
		t <  (timer +  (sizeof(timer)/sizeof(timer[0]))); t++)
	{
		if (t->valid)
		{
			timer_delete(t->id);
		}
		t->valid = false;
	}

	return true;
}

/**
 * @brief 	To wait for micro sec.
 * @param 	uSec - Amount of macro-seconds to slepp
 * @return 	True after time wait is completed
 * @author	Vigneshwaran Karunanithi(RBEI/ECA5)
 * @history	
 * 		0.1 -Initial version
 * 		0.2 -Code cleanup work
 */
bool TimeWait(unsigned long uSec)
{
	usleep(uSec);
	return true;
}

/**
 * @brief	General function shall return the timer expiration overrun count for
 * 		the given timer id (with help of timer_getoverrun).
 * @param 	void
 * @return	True of success/False for failure.
 * @author	Vigneshwaran Karunanithi(RBEI/ECA5)
 * @history	
 * 		0.1 -Initial version
 * 		0.2 -Code cleanup work
 */
bool _TimeIsStop(timer_t id)
{
	return (timer_getoverrun(id) != false);
}

/**
 * @brief	Function shall return the timer expiration overrun count for
 * 		struct timer's 1st obj.
 * @param 	void
 * @return	True for success/False for failure.
 * @author	Vigneshwaran Karunanithi(RBEI/ECA5) 
 * @history	
 * 		0.1 -Initial version
 * 		0.2 -Code cleanup work
 */
bool TimeIsStop()
{
	return _TimeIsStop(timer[0].id);
}

/**
 * @brief	Function shall return the timer expiration overrun count for
 * 		struct timer's 2nd obj.
 * @param 	void
 * @return	True for success/False for failure.
 * @author	Vigneshwaran Karunanithi(RBEI/ECA5) 
 * @history	
 * 		0.1 -Initial version
 * 		0.2 -Code cleanup work
 */
bool TimeIsStop2()
{
	return _TimeIsStop(timer[1].id);
}

/**
 * @brief	General function  shall set the time until the next expiration
 * 		(with help of timer_settime).
 * @param 	ulOffset
 * @return	0 for success/neg for failure.
 * @author	Vigneshwaran Karunanithi(RBEI/ECA5) 
 * @history	
 * 		0.1 -Initial version
 * 		0.2 -Code cleanup work
 */
bool _TimeSetStop(timer_t id, unsigned long ulOffset)
{
	itimerspec ts = { { 0 } };

	ts.it_value.tv_nsec = ulOffset * (int)1E3;

	return (timer_settime(id, 0, &ts, 0) == 0);
}

/**
 * @brief	Function  shall set the time until the next expiration
 * 		(with help of _TimeSetStop) for struct timer's 1st obj.
 * @param 	ulOffset
 * @return	0 for success/neg for failure.
 * @author	Vigneshwaran Karunanithi(RBEI/ECA5) 
 * @history	
 * 		0.1 -Initial version
 * 		0.2 -Code cleanup work
 */
bool TimeSetStop(unsigned long ulOffset)
{
	return _TimeSetStop(timer[0].id, ulOffset);
}

/**
 * @brief	Function  shall set the time until the next expiration
 * 		(with help of _TimeSetStop) for struct timer's 2nd obj.
 * @param 	ulOffset
 * @return	0 for success/neg for failure.
 * @author	Vigneshwaran Karunanithi(RBEI/ECA5) 
 * @history	
 * 		0.1 -Initial version
 * 		0.2 -Code cleanup work
 */
bool TimeSetStop2(unsigned long ulOffset)
{
	return _TimeSetStop(timer[1].id, ulOffset);
}

/**
 * @brief	Function will get the current time value in timeT0
 * 		variable(for CLOCL_REALTIME). 
 * @param 	void.
 * @return	void
 * @author	Vigneshwaran Karunanithi(RBEI/ECA5) 
 * @history	
 * 		0.1 -Initial version
 * 		0.2 -Code cleanup work
 */
bool TimeSetStart(void)
{
	return (clock_gettime(CLOCK_REALTIME, &timeT0) == 0);
}

/**
 * @brief	Function used to provide nano sec sleep 
 * 		if some conditions satisfied.
 * @param 	uSec	- wait time.
 * @return 	1 for success/ return 0.
 * @author	Vigneshwaran Karunanithi(RBEI/ECA5) 
 * @history	
 * 		0.1 -Initial version
 * 		0.2 -Code cleanup work
 */
unsigned long TimeWaitTill(unsigned long uSec)
{
	struct timespec timeT1;
	struct timespec timeDelta = timeT0;

	if (!(timeT0.tv_sec) && !(timeT0.tv_nsec))
	{
		return false;
	}

	/*used to retrive current time for real time clock*/
	if (false != clock_gettime(CLOCK_REALTIME, &timeT1))
	{
		return false;
	}

	timeDelta.tv_nsec = (uSec * (long)1E4) - 
				(timeT1.tv_nsec - timeT0.tv_nsec);
	timeDelta.tv_sec  = (uSec / (time_t)1E6) - 
				(timeT1.tv_sec - timeT0.tv_sec);
	
	if (timeDelta.tv_nsec > INIT_INDEX)
	{
		nanosleep(&timeDelta, 0);
	}
	
	return true;
}

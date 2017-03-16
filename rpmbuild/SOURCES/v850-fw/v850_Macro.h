/**
 * @file 	v850_Macro.h
 * @brief 	Header file which is providing macros to avoid magic numbers
 * @author 	vrk5cob<Vigneshwaran.Karunanithi@in.bosch.com>
 * @copyright 	(c) 2015 Robert Bosch GmbH
 * @version 	0.1
 * @history
 * 		0.1 - initial version
 */

#ifndef __V850_MACRO__
#define __V850_MACRO__

/**Indicate success */
#define SUCCESS		0
/** Indicate zero/initial index*/
#define INIT_INDEX	0
/** Indicate value is 1/next from current position*/
#define NEXT_INDEX	1
/**Indicate value 1*/
#define VAL_1		1
/**Indicate value 2*/
#define VAL_2		2
/**Indicate value 3*/
#define VAL_3		3
/**Indicate value 4*/
#define VAL_4		4
/**Indicate value 6*/
#define VAL_6		6
/**Indicate value 7*/
#define VAL_7		7
/**Indicate value 8*/
#define VAL_8		8
/**Indicate value 9*/
#define VAL_9		9
/**Indicate value 10*/
#define VAL_10		10
/**Indicate value 14*/
#define VAL_14		14
/**Indicate value 15*/
#define VAL_15		15
/**Indicate value 16*/
#define VAL_16		16
/**Indicate value 24*/
#define VAL_24		24
/**Indicate value 32*/
#define VAL_32		32
/**Indicate value 100*/
#define VAL_100		100

/**Macro for array size is 4*/
#define TMP_ARR_SIZE0	4
/**Macro for array size is 5*/
#define TMP_ARR_SIZE1	5	
/**Macro for array size is 50*/
#define TMP_ARR_SIZE2	50	
/**Macro for array size is 180*/
#define TMP_ARR_SIZE3	180
/**Macro for array size is 256*/
#define TMP_ARR_SIZE4	256
/**Macro for array size is 1020*/
#define TMP_ARR_SIZE5	1020
/**Macro for array size is 3072*/
#define TMP_ARR_SIZE6   3072
/**Macro for array size is 25000*/
#define TMP_ARR_SIZE7	25000
/**Macro for array size is 512*/
#define TMP_ARR_SIZE8	512
/**Macro for array size is 20*/
#define TMP_ARR_SIZE9	20
/**Macro for array size is 3500*/
#define TMP_ARR_SIZE10	3500
/**Macro for array size is 500*/
#define TMP_ARR_SIZE11	500
/**Macro for array size is 255*/
#define TMP_ARR_SIZE12	255
/**Macro for array size is 128*/
#define TMP_ARR_SIZE13	128
/**Macro for array size is 1024*/
#define TMP_ARR_SIZE_KB	1024

/**Virgin flash flag*/
#define MODE_VIRG	0x7bFFFFFF
/**Update flash flag*/
#define MODE_UPDATE	0x5bFFFFFF
/**Fixed  length*/
#define DEF_LEN		32

/** Telegram timeout */
#define TELE_TIMEOUT	500
/** Telegram Len */
#define TELE_LEN	0x70
/** Telegram retry */
#define TELE_RETRY	5
/** Erase timeout */
#define ERASE_TIMEOUT	20
/** Write timeout*/
#define WRITE_TIMEOUT	5
/** 100 delay */
#define PRE_DNL_DELAY	100
/** FWL timeout*/
#define FWL_TIMEOUT	250
/** Program retry */
#define PRG_RETRY	2

#endif

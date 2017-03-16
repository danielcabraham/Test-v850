/**
 * @file        parser.h
 * @brief       Act as header file for parser.cpp.
 * @author      ShashiKiran HS (RBEI/ECG2)
 * 		kuk5cob<Kumar.Kandasamy@in.bosch.com>
 * @version     0.2
 * @copyright   (c) 2015 Robert Bosch GmbH
 * @history     
 *      0.1     -initial version
 *      0.2     -code cleaning (16/12/2015,vrk5cob).
 */

#ifndef __PARSER_H__
#define __PARSER_H__

#include "v850_Macro.h"

void cfg_free(int argc, char **argv);
int cfg_get(const char *file, int * const argc, char *** const argv);

#endif

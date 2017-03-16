/** 
 * @file 	parser.cpp
 * @brief 	Provides API's to extract certain words from a file
 * @version 	0.2
 * @copyright	(c) 2015 Robert Bosch GmbH and its subsidiaries
 * @author	ShashiKiran HS (RBEI/ECG2)
 * 		kuk5cob<Kumar.Kandasamy@in.bosch.com>
 * 		vrk5cob<Vigneshwaran Karunanith@in.bosch.com>
 * @history     
 * 	0.1     -initial version
 * 	0.2     -code cleaning (16/12/2015,vrk5cob).
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#ifndef UTEST
#include <fcntl.h>
#endif

#include <string.h>
#include <errno.h>

#ifndef LINUX
#include "strndup.h"
#endif

#include "v850_Macro.h"

/**
 * @brief 	Return the memory used for a argv structure obtained
 * 		through cfg_get.
 * @param 	argc
 * @param 	argv
 * @return 	0 on success/neg error value.
 * @author	Vigneshwaran Karunanithi(RBEI/ECA5)
 * @history 
 * 		0.1	-initial version
 * 		0.2	-code cleanup(16/12/2015,vrk5cob)
 */
void cfg_free(int argc, char **argv)
{
	while(argc--)
	{
		if (argv[argc]) 
			free(argv[argc]);

		argv[argc] = NULL;
	}

	free(argv);
}

/**
 * @brief	create duplicate of hast_ptr-line or line and assign to
 * 		str.
 * @param 	hash_ptr - char ptr
 * @param 	line 	 - char ptr
 * @param 	str 	 - char ptr
 * @return 	0 if duplicate is created / return 1. 
 * @author	Vigneshwaran Karunanithi(RBEI/ECA5)
 * @history 
 * 		0.1	-initial version
 * 		0.2	-code cleanup(16/12/2015,vrk5cob)
 */
unsigned char check_and_dup(char *hash_ptr, char *line,char *str)
{
	unsigned char ret = false;

	if(hash_ptr > line)
	{
		str = strndup(line, hash_ptr - line);
	}
	else if (hash_ptr != line)
	{
		str = strdup(line);
	}
	else
	{
		ret = true;
	}

	return ret;
}

/**
 * @brief	Open and read the given filename, parse the lines that 
 * 		do no start with '#' as single words on the command line.
 * @param 	Filename File name from which words are extracted.
 * @param 	argc is updated to the number of lines read.
 * @param 	argv space is allocated to hold argc pointers plus
             	the strings. The pointer to such structure is stored here in.
 * @return 	0 on success/neg error value. 
 * @author	Vigneshwaran Karunanithi(RBEI/ECA5)
 * @history 
 * 		0.1	-initial version
 * 		0.2	-code cleanup(16/12/2015,vrk5cob)
 */
int cfg_get(const char *filename, int * const argc, char *** const argv)
{
	FILE	*cfg	= fopen(filename, "r");
	char line[TMP_ARR_SIZE3/*180*/];

	if(!cfg)
	{
		return errno;
	}

	/* The argc 1 / argv[0] is always the program name!
	 * we provide an empty string here instead
	 */
	*argc = true;

	*argv=(char**)calloc(1, sizeof(char *));
	if(!(*argv))
		return errno;

	(*argv)[INIT_INDEX] = NULL;

	while(fgets(line, sizeof(line), cfg))
	{
		char *hash_ptr = strstr(line,"#");
		char *str = 0;
	
		/* consider characters after a # as comments*/
		if (hash_ptr)
		{
			if (check_and_dup(hash_ptr, line, str))
				continue;
		}
		else
		{ 
			str = strdup(line);/*creating duplication*/
		}

		if (str)
		{
			char *mystr	= str;
			char *endstr	= NULL;
		
			while (*mystr == ' ')
			{
				mystr++;
			}
			
			endstr	= mystr;

			while ((*endstr != '\n') && 
				(*endstr != '\0') &&
				(*endstr > ' '))
			{
				endstr++;
			}
			
			/*set to null in last addr*/
			*endstr = '\0';
			(*argc)++;

			/*breaked for 80 char in a line*/
			*argv = (char**)realloc(*argv, 
					(*argc) * sizeof(char*));
			if (!(*argv))
			{
				cfg_free(*argc, *argv);
				return -ENOMEM;
			}

			(*argv)[(*argc)-NEXT_INDEX] = strdup(mystr);
			free(str);
		}
		else
		{
			cfg_free(*argc, *argv);
			return -ENOMEM;
		}
	}
	return fclose(cfg);
}

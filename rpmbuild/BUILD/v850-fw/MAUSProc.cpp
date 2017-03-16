#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>

#include <errno.h>
#include "MAUSProc.h"
#ifndef LINUX
  #include "strndup.h"
#endif

int ctr;                         // counter for number of lines of tx/rx msgs
char **lines;                    // lines for rx/tx msg-data
char buffer[510];                //


//---------------------------------------------------------------------------

char *strupper(char *s)          // write own function to have functionality in
{                                // LINUX as well
  char *ret = s;
  while (*s)
  {  *s = toupper (*s);
     s++;
  }
  return ret;
}
//---------------------------------------------------------------------------

char * NormalizeMsg(char *Msg,int &len)
{ len = 0;
  char *ptr,*endptr;
  char Copy[1000] =  { 0 };
  bool StringStart = false;

  if (Msg == NULL) return NULL;

  for (size_t i=0;i<strlen(Msg);i++)         // alle x aus String entfernen
      if ((Msg[i] == 'x') || (Msg[i] == 'X'))
      {  if ((i>0) && (Msg[i-1]) == '0')
            len -=1;;
      }
      else Copy[len++] = Msg[i];

  endptr = Copy + strlen(Copy);
  ptr = &Copy[0];
  len = 0;

  while (ptr < endptr)
  {  if (*ptr == '"')                     // Stringformat erkannt ?
        if (!StringStart) StringStart = true;   // dann
        else StringStart = false;         // Start oder Ende setzen
     else
     {  if (StringStart)                  // if Stringformat aktiv
        {
           buffer[len++] = *ptr;          // 1 Zeichen in Buffer holen
        }
        else                              // sonst
        {  char help[6] = { "0x0" };      // max. 2 Zeichen
           while (*ptr == ' ') ptr++;     // in Wert wandeln und in Buffer
                                          // kopieren
           if (*ptr == ';') break;        // raus falls Kommentar
           strncat(help,ptr,1);
           if ((*(ptr+1) != ' ') && ((*ptr+1) != ';'))  // Space = Trennzeichen
              strncat(help,(++ptr),1);

           unsigned char val = (char)(strtoul(help,NULL,16));
           buffer[len++] = val;

           while (*(ptr+1) == ' ') ptr++; // in Wert wandeln und in Buffer
        }
     }
     ptr++;
  }
  return len ? buffer : NULL;
}
//---------------------------------------------------------------------------

/*
   Return the memory used for a argv structure optained
   through cfg_get.
 */
void list_free(int count, char **lines)
{
  while(count--)
    {
      if (lines[count]) free(lines[count]);
      lines[count] = NULL;
    }

  free(lines);
}
//---------------------------------------------------------------------------

bool AddNewLine(char *mystr)
{ char **help = NULL;

  ctr++;
  if (lines == NULL) help = (char**)malloc((ctr) * sizeof(char*));
  else help = (char**)realloc(lines, (ctr) * sizeof(char*));

  if (!help) return false;

  lines = help;
  (lines)[(ctr)-1] = strdup(mystr);
  return true;
}
//---------------------------------------------------------------------------
/*
  Open and read the given filename, parse the lines that
  do no start with '#' as single words on the command line.

  In case of error the errno will be returned.

  returns: argc - is updated to the number of lines read.
           argv - space is allocated to hold argc pointers plus
                  the strings. The pointer to such structure
		  is stored herein.
 */
int MAUSProcessing(const char *filename, XmVect xv, RxVect rv)
{
  printf("\nDo MAUS processing by script: %s",filename);

  if (filename != NULL)            // data must be read from given file only
  {                                // if filename is passed
    FILE* cfg = fopen(filename, "r");
    char line[512];

    if(!cfg)
      return errno;                // file open error

    ctr = 0;

    // read vaild data from file into string array

    while(fgets(line, sizeof(line), cfg))
    {
      char* comment_ptr = strstr(line,"#");
      if (comment_ptr == NULL) comment_ptr = strstr(line,";");
      if (comment_ptr == NULL) comment_ptr = strstr(line,"//");

      char *str = NULL;
      /* consider characters after a #, ; or // as comments. */
      if (comment_ptr)
      {
         if(comment_ptr > line)
	       str = strndup(line, comment_ptr - line);
     	 else if(comment_ptr != line)
	            str = strdup(line);
              else continue;
      }
      else str = strdup(line);


      if (str)
      {
         char *mystr = str;
         while (*mystr==' ') mystr++;
         char *endstr = mystr+strlen(mystr);
         while (((*endstr == '\n') || (*endstr == '\r') || (*endstr == '\0') || (*endstr <= ' ')) && (mystr<endstr)) endstr--;
         *(endstr+1) = '\0';
         if (mystr < endstr)
         {  if (!AddNewLine(mystr))
            {
               free(str);
               list_free(ctr, lines);
               return ENOMEM;
            }
         }
         free(str);
      }
      else
      {
         list_free(ctr, lines);
         return ENOMEM;
      }
    }

    int ex = fclose(cfg);					// Close file
	if (ex != 0) return ex;					// exit if error
  }


  /* Now process the lines !!! */

  bool err = false;
  char *TxAct = NULL;
  char *RxAct = NULL;
  char *MsgPtr = NULL;
  int i;

  if (lines != NULL)
  {
    for (i=0; i<ctr && !err;i++)               // process for number of lines
    {   TxAct = strstr(strupper(lines[i]),"TX:");
        RxAct = strstr(strupper(lines[i]),"RX:");
        MsgPtr = TxAct ? TxAct : RxAct;
        int len;

        if (MsgPtr != NULL)
        {  char *Msg = NormalizeMsg(MsgPtr+3,len); // convert ASCII to bin-format

           if (Msg)
           {
              if (TxAct)
              {
                 (xv) ((ubyte)*Msg,(ubyte*)(Msg+2),(ubyte)*(Msg+1));
              }
              else if (RxAct)
              {
                 ubyte RxBuf[512] = { 0 };
                 ubyte MsgLen = 255;

                 if ((rv)(Msg[0],RxBuf,(ubyte*)&MsgLen,5) == FLASH_SUCCESS)
                 {  for (int i=0; i<len && !err; i++)
                        err = (RxBuf[i] != Msg[i]);  // expected data received ?
                 }
                 else err = true;                    // Reception failed
              }
           } // Msg != NULL
        } // TX: or Rx:
    } // for lines[]

    list_free(ctr, lines);
    lines = NULL;
  } // if (lines != NULL)

  if (err) printf("\nMAUS script processing not successful !!\nError in line %d : %s\n",i,MsgPtr);
  else printf("\nMAUS script processing successful !!\n");

  return err ? -1 : 0;
}
//---------------------------------------------------------------------------



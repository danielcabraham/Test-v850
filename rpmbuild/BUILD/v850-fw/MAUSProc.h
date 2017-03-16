#include "Flash850.h"

typedef ulword __stdcall (*XmVect)(ubyte ubCmd, ubyte *pubBuf, ubyte ubLen);
typedef ulword __stdcall (*RxVect)(ubyte ubCmd, ubyte *pubBuf, ubyte *ubLen, ulword ulwRetries);

int MAUSProcessing(const char *filename, XmVect x, RxVect r);



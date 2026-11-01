#ifndef __LOG_H__
#define __LOG_H__

#include "log.h"
#include "fsl_debug_console.h"
#include <stdio.h>

void _debuglog(const char* msg)
{
    DbgConsole_Printf("%s", msg);
    DbgConsole_Printf("\n\r");
}

void _debuglogf(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    DbgConsole_Vprintf(format, args);
    va_end(args);
    DbgConsole_Printf("\n\r");
}

#endif

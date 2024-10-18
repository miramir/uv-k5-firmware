#include "version.h"

#ifdef VERSION_STRING
    #define VER     " "VERSION_STRING
#else
    #define VER     ""
#endif

#ifndef AUTHOR_STRING
    #define AUTHOR_STRING "noname"
#endif

const char Version[]      = AUTHOR_STRING VER;
const char UART_Version[] = "UV-K5 Firmware, " AUTHOR_STRING VER "\r\n";

/*  Copyright (C) 2012  Adam Green (https://github.com/adamgreen)

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version 2
    of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
*/
#ifndef _LINE_INFO_H_
#define _LINE_INFO_H_

#include "try_catch.h"

typedef struct LineInfo
{
    const char*    pLineText;
    size_t         machineCodeSize;
    int            validSymbol;
    unsigned int   lineNumber;
    unsigned short address;
    unsigned short symbolValue;
    unsigned char  machineCode[3];
} LineInfo;

#endif /* _LINE_INFO_H_ */
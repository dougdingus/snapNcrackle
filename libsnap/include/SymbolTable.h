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
#ifndef _SYMBOL_TABLE_H_
#define _SYMBOL_TABLE_H_

#include "try_catch.h"
#include "Symbol.h"


typedef struct SymbolTable SymbolTable;


__throws SymbolTable* SymbolTable_Create(size_t bucketCount);
         void         SymbolTable_Free(SymbolTable* pThis);
         
         size_t       SymbolTable_GetSymbolCount(SymbolTable* pThis);
__throws Symbol*      SymbolTable_Add(SymbolTable* pThis, const char* pKey);
         Symbol*      SymbolTable_Find(SymbolTable* pThis, const char* pKey);
         
         void         SymbolTable_EnumStart(SymbolTable* pThis);
         Symbol*      SymbolTable_EnumNext(SymbolTable* pThis);

#endif /* _SYMBOL_TABLE_H_ */

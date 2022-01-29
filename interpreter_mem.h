#ifndef INTERPRETER_MEM_MANAGE_H_
#define INTERPRETER_MEM_MANAGE_H_
#include <stdbool.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdarg.h>
#include "interpreter_console.h"

/* Return true If memory is allocated successfully */
bool IsMemAlloc(void *ptr);
void FreeString(size_t free_num, char *str, ...);
void FreeCmdList(CmdElementPtr head);
#endif

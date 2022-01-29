#ifndef INTERPRETER_COMMAND_LINE_H_
#define INTERPRETER_COMMAND_LINE_H_
#include "interpreter_console.h"
#include <stdbool.h>
#include <sys/types.h>

void CmdLineInit();
char *CmdLine();
bool AddHistoryCmd(const char *cmd);
bool SaveHistoryCmd(const char *file_name);
void FreeHistory();
bool LoadHistory(const char *file_name);

#endif

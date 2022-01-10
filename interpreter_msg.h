#ifndef INTERPRETER_MSG_H_
#define INTERPRETER_MSG_H_
#include <stdbool.h>

extern bool g_is_visible; /* Shows messages if it's true */
extern char *g_log_file;  /* Path of log file */

void SetMsgVisible(bool is_visible);
void SetLogFile(char *log_file);
void ShowMsg(const char *format, ...);
#endif

#include "interpreter_msg.h"
#include <stdarg.h>
#include <stdio.h>

bool g_is_visible = false;
char *g_log_file = NULL;

void SetMsgVisible(bool is_visible) { g_is_visible = is_visible; }

void SetLogFile(char *log_file) { g_log_file = log_file; }

void LogMsg(const char *format, va_list args) {
  FILE *fp;

  if (!(fp = fopen(g_log_file, "a"))) {
    printf("open file %s failed\n", g_log_file);
    return;
  }
  vfprintf(fp, format, args);
  if (fp) {
    fclose(fp);
  }
}

void ShowMsg(char *format, ...) {
  va_list args;
  FILE *fp;

  if(g_is_visible){
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
  }
  if(g_log_file){
    va_start(args, format);
    LogMsg(format, args);
    va_end(args);
  }
}

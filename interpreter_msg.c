#include "interpreter_msg.h"
#include <stdarg.h>
#include <stdio.h>

bool g_is_visible = false;
char *g_log_file = NULL;

void SetMsgVisible(bool is_visible) { g_is_visible = is_visible; }

void SetLogFile(char *log_file) { g_log_file = log_file; }

void LogMsg(const char *format, va_list var_list) {
  FILE *fp;

  if (!(fp = fopen(g_log_file, "a"))) {
    printf("open file %s failed\n", g_log_file);
  }
  vfprintf(fp, format, var_list);
  if (fp) {
    fclose(fp);
  }
}

void ShowMsg(const char *format, ...) {
  va_list var_list;
  FILE *fp;

  va_start(var_list, format);
  if (g_is_visible) {
    vprintf(format, var_list);
  }
  if (g_log_file) {
    LogMsg(format, var_list);
  }
  va_end(var_list);
}

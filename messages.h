#ifndef __SHOW_MESSAGE__
#define __SHOW_MESSAGE__
#include <stdbool.h>

extern bool is_visible;
extern char *log_file;

void message_init(bool);
void do_log_init(char *);
void show_message(const char *, ...);

#endif

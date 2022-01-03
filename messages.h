#ifndef __SHOW_MESSAGE__
#define __SHOW_MESSAGE__
extern bool is_visible;
extern char *log_file;

void message_init();
void show_message(const char *, ...);

#endif

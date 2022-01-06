#ifndef __CONSOLE_H__
#define __CONSOLE_H__
#include <stdbool.h>
#include <sys/types.h>

typedef bool (*cmd_func)(int, char **);

typedef struct cmd_ele{
    char *cmd;
    char *doc;
    cmd_func op;
    struct cmd_ele *next;
}cmd_ele, *cmd_ptr;

typedef struct cmd_line_state{
    char *buf;
    char *prompt;
    size_t len;
    size_t pos;
    size_t col;
    int history_idx;
    size_t completion_idx;
}cmd_line_state;

typedef struct abuf {
    char *b;
    int len;
}abuf;

void console_init();
bool add_cmd(char *, char *, cmd_func);
bool run_console(char *, char *, bool);

#endif

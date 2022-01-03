#ifndef __CONSOLE_H__
#define __CONSOLE_H__

typedef bool (*cmd_func)(int, char **);

typedef struct cmd_ele{
    char *cmd;
    char *doc;
    cmd_func op;
    struct cmd_ele *next;
}cmd_ele, *cmd_ptr;

void console_init();
bool add_cmd(char *, char *, cmd_func);
bool run_console(char *, char *, bool);

#endif

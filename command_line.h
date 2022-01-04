#ifndef __COMMAND_LINE__
#define __COMMAND_LINE__

char *cmd_line();
int cmd_line_edit(char *, char *, size_t);
bool add_history_cmd(const char *);
int save_history_cmd(const char *);
void free_history();
int load_history(const char *);

#endif

#ifndef INTERPRETER_CONSOLE_H_
#define INTERPRETER_CONSOLE_H_
#include <stdbool.h>
#include <sys/types.h>

typedef bool (*CmdFunction)(int, char **);

typedef struct CmdElement {
  char *cmd;
  char *doc;
  CmdFunction op;
  struct CmdElement *next;
} CmdElement, *CmdElementPtr;

typedef struct CmdLineState {
  /* Content of command line */
  char *buf;
  /* Prompt of command line, e.g., cmd> */
  char *prompt;
  /* Length of command line */
  size_t len;
  /* Position of cursor of command line */
  size_t pos;
  /* Width of terminal */
  size_t col;
  /* For looking for command of history */
  int history_idx;
} CmdLineState;

typedef struct Buffer {
  char *val;
  int len;
} Buffer;

bool ConsoleInit();
/* Sets what command users can operate on command line */
bool AddCmd(char *cmd, char *doc, CmdFunction op);
bool RunConsole(char *input_file, char *log_file, bool is_visible);
#endif

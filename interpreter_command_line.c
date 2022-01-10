#include "interpreter_command_line.h"
#include "interpreter_mem.h"
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

size_t g_max_line = 4096;
static int g_history_max_len = 200;
static int g_history_len = 0;
static char **g_history = NULL;
CmdElementPtr g_cmd_list = NULL;
CmdElementPtr g_cmd_list_ptr = NULL;

enum Action {
  CTRL_B = 2,
  CTRL_C = 3,
  CTRL_F = 6,
  TAB = 9,
  ENTER = 13,
  ESC = 27,
  BACKSPACE = 127
};

enum Direction { DOWN = 0, UP = 1 };

void HistoryCmd(CmdLineState *, int);

void CmdLineInit(CmdElementPtr cmd_list) {
  g_cmd_list = cmd_list;
  g_cmd_list_ptr = g_cmd_list;
}

static void BufferInit(Buffer *buf) {
  buf->val = NULL;
  buf->len = 0;
}

static void BufferAppend(Buffer *buf, const char *str, int str_len) {
  char *new_buf_val = realloc(buf->val, (buf->len + str_len + 1) * sizeof(char));

  if (!IsMemAlloc(new_buf_val)){
    return;
  }
  strncpy(new_buf_val + buf->len, str, str_len);
  new_buf_val[buf->len + str_len] = '\0';
  buf->val = new_buf_val;
  buf->len += str_len;
}

static void CmdLineBeep() {
  fprintf(stderr, "\x7");
  fflush(stderr);
}

/* CmdLine() is called in pipe or with a file redirected 
   to its standard input 
 * On success, return a pointer to a command 
 * On error, return NULL
 * Memory is freed by caller */
static char *CmdLineNoTTY() {
  size_t max_len = 256;
  size_t current_len = 0;
  char *cmd = NULL;
  char *cmd_ptr = NULL;
  char c = '\0';

  cmd = malloc((max_len + 1) * sizeof(char));
  if (!IsMemAlloc(cmd)) {
    return cmd;
  }
  memset(cmd, 0, (max_len + 1) * sizeof(char));
  while (true) {
    /* Make space of cmd double */
    if (current_len >= max_len) {
      max_len *= 2;
      cmd_ptr = realloc(cmd, (max_len + 1) * sizeof(char));
      if (!IsMemAlloc(cmd_ptr)) {
        return cmd;
      }
      cmd = cmd_ptr;
      memset(cmd + max_len, 0, (max_len + 1) * sizeof(char));
    }
    c = fgetc(stdin);
    if (c == EOF || c == '\n') {
      if (current_len > 0){
          cmd[current_len] = '\0';
      }
      return cmd;
    } else {
      cmd[current_len] = c;
      current_len++;
    }
  }
}

/* Sets attributes of terminal 
 * On success, return true */
static bool EnableRawMode(struct termios *orig) {
  struct termios raw = *orig;

  /* input modes: no break, no CR to NL, no parity check, no strip char,
   * no start/stop output control. */
  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
  /* output modes - disable post processing */
  raw.c_oflag &= ~(OPOST);
  /* control modes - set 8 bit chars */
  raw.c_cflag |= (CS8);
  /* local modes - choing off, canonical off, no extended functions,
   * no signal chars (^Z,^C) */
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
  /* control chars - set return condition: min number of bytes and timer.
   * We want read to return every single byte, without timeout. */
  raw.c_cc[VMIN] = 1;
  raw.c_cc[VTIME] = 0; /* 1 byte, no timer */
  /* put terminal in raw mode after flushing */
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) < 0)
    return false;
  return true;
}

/* Sets default attributes of terminal */
static bool DisableRawMode(struct termios *orig) {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, orig) < 0)
    return false;
  return true;
}

/* Gets width of terminal 
 * On success, return width of termial
 * On error, return 0 */
static int GetColumns() {
  struct winsize ws;

  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) < 0)
    return 0;
  return ws.ws_col;
}

/* Refresh command line */
static void Refresh(CmdLineState *cls) {
  char seq[64];
  size_t prompt_len = strlen(cls->prompt);
  char *cls_buf_ptr = cls->buf;
  size_t len = cls->len;
  size_t pos = cls->pos;
  Buffer buf;

  while ((prompt_len + pos) >= cls->col) {
    cls_buf_ptr++;
    len--;
    pos--;
  }
  while (prompt_len + len > cls->col) {
    len--;
  }
  BufferInit(&buf);
  /* Cursor to left edge */
  snprintf(seq, 64, "\r");
  BufferAppend(&buf, seq, strlen(seq));
  /* Write the prompt and the current buffer content */
  BufferAppend(&buf, cls->prompt, prompt_len);
  BufferAppend(&buf, cls_buf_ptr, len);
  /* Erase to right */
  snprintf(seq, 64, "\x1b[0K");
  BufferAppend(&buf, seq, strlen(seq));
  /* Move cursor to original position. */
  snprintf(seq, 64, "\r\x1b[%dC", (int)(pos + prompt_len));
  BufferAppend(&buf, seq, strlen(seq));
  if (write(STDOUT_FILENO, buf.val, buf.len) == -1) {
  } /* Can't recover from write error. */
  free(buf.val);
}

/* Inserts character c to command line cls->buf */
static void CmdLineIns(CmdLineState *cls, char c) {
  if (cls->len < g_max_line) {
    /* Inserts c at tail of command line */
    if (cls->len == cls->pos) {
      cls->buf[cls->pos] = c;
      cls->pos++;
      cls->len++;
      cls->buf[cls->len] = '\0';
    } else { /* Inserts c at other places of command line */
      memmove(cls->buf + cls->pos + 1, cls->buf + cls->pos,
              cls->len - cls->pos);
      cls->buf[cls->pos] = c;
      cls->len++;
      cls->pos++;
      cls->buf[cls->len] = '\0';
    }
    Refresh(cls);
  }
}

static void CmdLineMoveLeft(CmdLineState *cls) {
  if (cls->pos > 0) {
    cls->pos--;
    Refresh(cls);
  } else {
    CmdLineBeep();
  }
}

static void CmdLineMoveRight(CmdLineState *cls) {
  if (cls->pos < cls->len) {
    cls->pos++;
    Refresh(cls);
  } else {
    CmdLineBeep();
  }
}

static void CmdLineBacksapce(CmdLineState *cls) {
  if (cls->pos > 0) {
    memmove(cls->buf + cls->pos - 1, cls->buf + cls->pos, cls->len - cls->pos);
    cls->buf[cls->len - 1] = '\0';
    cls->pos--;
    cls->len--;
    Refresh(cls);
  } else {
    CmdLineBeep();
  }
}

static void CmdLineDel(CmdLineState *cls) {
  if (cls->pos > 0 && cls->pos < cls->len) {
    memmove(cls->buf + cls->pos, cls->buf + cls->pos + 1,
            cls->len - cls->pos - 1);
    cls->len--;
    cls->buf[cls->len] = '\0';
    Refresh(cls);
  }
  if (cls->pos == 0 || cls->pos == cls->len) {
    CmdLineBeep();
  }
}

static void CmdLineMoveHome(CmdLineState *cls) {
  if (cls->pos != 0) {
    cls->pos = 0;
    Refresh(cls);
  }
}

static void CmdLineMoveEnd(CmdLineState *cls) {
  if (cls->pos != 0) {
    cls->pos = cls->len;
    Refresh(cls);
  }
}

static bool CompleteCmdLine(CmdLineState *cls) {
  char c = '\0';
  char seq[3];
  bool press_tab = false;
  size_t cmd_len = 0;
  CmdLineState cls_var = *cls;

  while (isalpha(cls->buf[0]) && g_cmd_list_ptr) {
    cmd_len = strlen(g_cmd_list_ptr->cmd);
    if (cls->len <= cmd_len && strncmp(cls->buf, g_cmd_list_ptr->cmd, cls->len) == 0) {
      strncpy(cls_var.buf, g_cmd_list_ptr->cmd, cmd_len);
      cls_var.buf[cmd_len] = '\0';
      cls_var.len = cls_var.pos = cmd_len;
      Refresh(&cls_var);

      while (true) {
        press_tab = false;
        read(STDIN_FILENO, &c, 1);
        switch (c) {
        case CTRL_B:
          CmdLineMoveLeft(&cls_var);
          break;
        case CTRL_C:
          errno = EAGAIN;
          return false;
        case CTRL_F:
          CmdLineMoveRight(&cls_var);
          break;
        case BACKSPACE:
          CmdLineBacksapce(&cls_var);
          strncpy(cls->buf, cls_var.buf, cls_var.len);
          cls->buf[cls_var.len] = '\0';
          cls->len = cls->pos = cls_var.len;
          g_cmd_list_ptr = g_cmd_list;
          break;
        case TAB:
          press_tab = true;
          break;
        case ENTER:
          strncpy(cls->buf, cls_var.buf, cls_var.len);
          cls->buf[cls_var.len] = '\0';
          cls->len = cls->pos = cls_var.len;
          Refresh(cls);
          return true;
        case ESC:
          if (read(STDIN_FILENO, seq, 1) == -1)
            break;
          if (read(STDIN_FILENO, seq + 1, 1) == -1)
            break;
          /* ESC [ sequences. */
          if (seq[0] == '[') {
            if (seq[1] >= '0' && seq[1] <= '9') {
              /* Extended escape, read additional byte. */
              if (read(STDIN_FILENO, seq + 2, 1) == -1)
                break;
              if (seq[2] == '~') {
                switch (seq[1]) {
                case '3': /* Delete key. */
                  CmdLineDel(&cls_var);
                  break;
                }
              }
            } else {
              switch (seq[1]) {
              case 'A': /* Up */
                HistoryCmd(&cls_var, UP);
                break;
              case 'B': /* Down */
                HistoryCmd(&cls_var, DOWN);
                break;
              case 'C': /* Right */
                CmdLineMoveRight(&cls_var);
                break;
              case 'D': /* Left */
                CmdLineMoveLeft(&cls_var);
                break;
              case 'H': /* Home */
                CmdLineMoveHome(&cls_var);
                break;
              case 'F': /* End*/
                CmdLineMoveEnd(&cls_var);
                break;
              }
            }
          } else if (seq[0] == 'O') { /* ESC O sequences. */
            switch (seq[1]) {
            case 'H': /* Home */
              CmdLineMoveHome(&cls_var);
              break;
            case 'F': /* End*/
              CmdLineMoveEnd(&cls_var);
              break;
            }
          } else {
            Refresh(cls);
            return false;
          }
          break;
        default:
          CmdLineIns(&cls_var, c);
          strncpy(cls->buf, cls_var.buf, cls_var.len);
          cls->buf[cls_var.len] = '\0';
          cls->len = cls->pos = cls_var.len;
          g_cmd_list_ptr = g_cmd_list;
          break;
        }
        if (c == TAB) {
          break;
        }
      }
    }
    g_cmd_list_ptr = g_cmd_list_ptr->next;
  }
  g_cmd_list_ptr = g_cmd_list;
  Refresh(cls);
  return false;
}

/* Edit command line 
 * On success, return a pointer to command line
 * On error, return NULL 
 * The returned pointer is freed by caller */
char *CmdLineEdit() {
  char c = '\0';
  CmdLineState cls;
  char seq[3];
  size_t cmd_line_len = 0;
  char *buf = NULL;
  char prompt[] = "cmd> ";

  buf = malloc((g_max_line + 1) * sizeof(char));
  if (!IsMemAlloc(buf)) {
    return NULL;
  }
  memset(buf, 0, (g_max_line + 1) * sizeof(char));

  cls.buf = buf;
  cls.prompt = prompt;
  cls.len = 0;
  cls.pos = 0;
  cls.col = GetColumns();
  cls.history_idx = 0;

  /* command line begins with cmd> */
  write(STDOUT_FILENO, cls.prompt, strlen(cls.prompt));
  while (true) {
    read(STDIN_FILENO, &c, 1);

    switch (c) {
    case CTRL_B:
      CmdLineMoveLeft(&cls);
      break;
    case CTRL_C:
      errno = EAGAIN;
      free(buf);
      return NULL;
    case CTRL_F:
      CmdLineMoveRight(&cls);
      break;
    case BACKSPACE:
      CmdLineBacksapce(&cls);
      break;
    case TAB:
      if(CompleteCmdLine(&cls))
        return cls.buf;
      break;
    case ENTER:
      return cls.buf;
    case ESC:
      if (read(STDIN_FILENO, seq, 1) == -1)
        break;
      if (read(STDIN_FILENO, seq + 1, 1) == -1)
        break;
      /* ESC [ sequences. */
      if (seq[0] == '[') {
        if (seq[1] >= '0' && seq[1] <= '9') {
          /* Extended escape, read additional byte. */
          if (read(STDIN_FILENO, seq + 2, 1) == -1)
            break;
          if (seq[2] == '~') {
            switch (seq[1]) {
            case '3': /* Delete key. */
              CmdLineDel(&cls);
              break;
            }
          }
        } else {
          switch (seq[1]) {
          case 'A': /* Up */
            HistoryCmd(&cls, UP);
            break;
          case 'B': /* Down */
            HistoryCmd(&cls, DOWN);
            break;
          case 'C': /* Right */
            CmdLineMoveRight(&cls);
            break;
          case 'D': /* Left */
            CmdLineMoveLeft(&cls);
            break;
          case 'H': /* Home */
            CmdLineMoveHome(&cls);
            break;
          case 'F': /* End */
            CmdLineMoveEnd(&cls);
            break;
          }
        }
      } else if (seq[0] == 'O') { /* ESC O sequences. */
        switch (seq[1]) {
        case 'H': /* Home */
          CmdLineMoveHome(&cls);
          break;
        case 'F': /* End */
          CmdLineMoveEnd(&cls);
          break;
        }
      }
      break;
    default: /* Inserts c into command line */
      CmdLineIns(&cls, c);
      break;
    }
  }
}

/* Edits command line in raw mode 
 * On success, return a pointer to command line
 * On error, return NULL */
static char *CmdLineRaw() {
  char *buf = NULL;
  struct termios orig;

  /* Gets default attributes of terminal */
  if (tcgetattr(STDIN_FILENO, &orig) < 0)
    return NULL;
  if (!EnableRawMode(&orig))
    return NULL;
  buf = CmdLineEdit();
  if (!DisableRawMode(&orig))
    return NULL;
  printf("\n");
  return buf;
}

/* On success, return a pointer to command line
 * On error, return NULL
 * The returned pointer is freed by caller */
char *CmdLine() {
  char *buf = NULL;

  /* CmdLine() is called in pipe or with a file redirected 
     to its standard input */
  if (!isatty(STDIN_FILENO)) {
    buf = CmdLineNoTTY();
  }else{
    buf = CmdLineRaw();
  }
  return buf;
}

/* Add a command into history list 
 * On success, return true */
bool AddHistoryCmd(const char *cmd) {
  size_t cmd_len = 0;

  if (!g_history) {
    g_history = malloc((g_history_max_len + 1) * sizeof(char *));
    if (!IsMemAlloc(g_history)) {
      return false;
    }
    memset(g_history, 0, g_history_max_len * sizeof(char *));
  }
  cmd_len = strlen(cmd);
  /* Do not add duplicated command */
  if (g_history_len > 0 && cmd_len == strlen(g_history[g_history_len - 1]) &&
      strncmp(g_history[g_history_len - 1], cmd, cmd_len) == 0) {
    return false;
  }
  if (!g_history[g_history_len]) {
    g_history[g_history_len] = malloc((cmd_len + 1) * sizeof(char));
    if (!IsMemAlloc(g_history[g_history_len])) {
      return false;
    }
    memset(g_history[g_history_len], 0, (cmd_len + 1) * sizeof(char));
  }
  /* Number of history command reaches the maximum */
  if (g_history_len == g_history_max_len) {
    /* Remove the oldest history command */
    free(g_history[0]);
    memmove(g_history, g_history + 1, g_history_max_len - 1);
    g_history_len--;
  }
  strncpy(g_history[g_history_len], cmd, cmd_len);
  g_history[g_history_len][cmd_len] = '\0';
  g_history_len++;
  return true;
}

/* Saves commands of history in file_name 
 * On success, return true */
bool SaveHistoryCmd(const char *file_name) {
  FILE *file_ptr = NULL;
  int j = 0;

  /* Sets file mode creation mask 
   * Only the owner can read or write file_name */
  umask(S_IXUSR | S_IRWXG | S_IRWXO);
  file_ptr = fopen(file_name, "w");
  if (file_ptr == NULL)
    return false;
  for (j = 0; j < g_history_len; j++)
    fprintf(file_ptr, "%s\n", g_history[j]);
  fclose(file_ptr);
  return true;
}

void FreeHistory() {
  if (g_history) {
    for (int i = 0; i < g_history_len; i++)
      free(g_history[i]);
    free(g_history);
  }
}

/* Loads commands of history 
 * On success, return true */
bool LoadHistory(const char *file_name) {
  char buf[g_max_line];
  char *buf_ptr = NULL;
  FILE *file_ptr = NULL;

  if ((file_ptr = fopen(file_name, "r")) == NULL)
    return false;
  while (fgets(buf, g_max_line, file_ptr) != NULL) {
    buf_ptr = strchr(buf, '\r');
    if (!buf_ptr)
      buf_ptr = strchr(buf, '\n');
    if (buf_ptr)
      *buf_ptr = '\0';
    AddHistoryCmd(buf);
  }
  fclose(file_ptr);
  return true;
}

/* Puts previous/next command on command line if d = 1/0 */
void HistoryCmd(CmdLineState *cls, int d) {
  size_t cmd_len = 0;

  if (g_history_len > 0) {
    if (d == 0) {
      cls->history_idx--;
      if (cls->history_idx == 0) {
        strcpy(cls->buf, "");
        cls->len = cls->pos = 0;
        Refresh(cls);
        return;
      } else if (cls->history_idx < 0) {
        cls->history_idx = 0;
        CmdLineBeep();
        return;
      }
    } else if (d == 1) {
      cls->history_idx++;
      if (cls->history_idx >= g_history_len) {
        CmdLineBeep();
        cls->len = cls->pos = 0;
        cls->history_idx = g_history_len - 1;
        return;
      }
    } else {
      return;
    }
    cmd_len = strlen(g_history[g_history_len - cls->history_idx]);
    strncpy(cls->buf, g_history[g_history_len - cls->history_idx], cmd_len);
    cls->buf[cmd_len] = '\0';
    cls->len = cls->pos = cmd_len;
    Refresh(cls);
  } else {
    CmdLineBeep();
  }
}


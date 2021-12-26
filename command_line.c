#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <termios.h>
#include <errno.h>
#include <sys/ioctl.h>
#include "command_line.h"
#include "mem_manage.h"

unsigned int max_line = 4096;
static int history_max_len = 200;
static int history_len = 0;
static char **history = NULL;

enum action{
    CTRL_B = 2,
    CTRL_C = 3,
    CTRL_F = 6,
    TAB = 9,
    ENTER = 13,
    ESC = 27,
    BACKSPACE = 127
};

typedef struct cmd_line_state{
    char *buf;
    char *prompt;
    size_t len;
    size_t pos;
    size_t col;
}cmd_line_state;

typedef struct abuf {
    char *b;
    int len;
}abuf;

static void cmd_line_beep(){
    fprintf(stderr, "\x7");
    fflush(stderr);
}

static void abInit(struct abuf *ab)
{
    ab->b = NULL;
    ab->len = 0;
}

static void abAppend(struct abuf *ab, const char *s, int len)
{
    char *new = realloc(ab->b, ab->len + len);

    if (!mem_alloc_succ(new))
        return;
    memcpy(new + ab->len, s, len);
    ab->b = new;
    ab->len += len;
}

static char *cmd_line_noTTY(){
    size_t max_len = 256;
    size_t curr_len = 0;
    char *new_input = NULL;
    char *input = calloc(max_len, sizeof(char));
    if(!mem_alloc_succ(input)){
        return NULL;
    }

    while(true){
        if(curr_len >= max_len){
		max_len *= 2;
            new_input = realloc(input, max_len);
	    if(!mem_alloc_succ(new_input)){
                if(input)
                    free(input);
		return NULL;
	    }
	    input = new_input;
	}
	int c = fgetc(stdin);
	if (c == EOF || c == '\n') {
            if (c == EOF && curr_len == 0) {
                free(input);
                return NULL;
            } else {
                input[curr_len] = '\0';
                return input;
            }
        } else {
            input[curr_len] = c;
            curr_len++;
        }
    }
}

static bool enable_raw_mode(){
    struct termios raw;

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

static bool disable_raw_mode(struct termios *orig){
    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, orig) < 0)
	    return false;
    return true;
}

// On success, return column
// On error, return 0
static int get_columns(){
    struct winsize ws;

    if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) < 0)
	    return 0;
    return ws.ws_col;
}

static void refresh(cmd_line_state *cls){
    char seq[64];
    size_t plen = strlen(cls->prompt);
    char *buf = cls->buf;
    size_t len = cls->len;
    size_t pos = cls->pos;
    struct abuf ab;

    while ((plen + pos) >= cls->col) {
        buf++;
        len--;
        pos--;
    }
    while (plen + len > cls->col) {
        len--;
    }
    abInit(&ab);
    /* Cursor to left edge */
    snprintf(seq, 64, "\r");
    abAppend(&ab, seq, strlen(seq));
    /* Write the prompt and the current buffer content */
    abAppend(&ab, cls->prompt, strlen(cls->prompt));
    abAppend(&ab, buf, len);
    /* Erase to right */
    snprintf(seq, 64, "\x1b[0K");
    abAppend(&ab, seq, strlen(seq));
    /* Move cursor to original position. */
    snprintf(seq, 64, "\r\x1b[%dC", (int) (pos + plen));
    abAppend(&ab, seq, strlen(seq));
    if (write(STDOUT_FILENO, ab.b, ab.len) == -1) {
    } /* Can't recover from write error. */
    free(ab.b);
}

static int cmd_line_ins(cmd_line_state *cls, char c){
    if(cls->len < max_line){
        if (cls->len == cls->pos) {
            cls->buf[cls->pos] = c;
            cls->pos++;
            cls->len++;
            cls->buf[cls->len] = '\0';
	}else{
            memmove(cls->buf + cls->pos + 1, cls->buf + cls->pos, cls->len - cls->pos);
            cls->buf[cls->pos] = c;
            cls->len++;
            cls->pos++;
            cls->buf[cls->len] = '\0';
	}
	refresh(cls);
    }

}


void cmd_line_move_left(cmd_line_state *cls){
    if(cls->pos > 0){
        cls->pos--;
	refresh(cls);
    }
}

void cmd_line_move_right(cmd_line_state *cls){
    if(cls->pos < cls->len){
        cls->pos++;
	refresh(cls);
    }
}

void cmd_line_backspace(cmd_line_state *cls){
    if(cls->pos > 0){
        memmove(cls->buf + cls->pos - 1, cls->buf + cls->pos, cls->len - cls->pos);
        cls->pos--;
	cls->len--;
	refresh(cls);
    }else{
        cmd_line_beep();
    }
}

void cmd_line_del(cmd_line_state *cls){
    if(cls->pos > 0 && cls->pos < cls->len){
        memmove(cls->buf + cls->pos, cls->buf + cls->pos + 1, cls->len - cls->pos - 1);
	cls->len--;
	cls->buf[cls->len] = '\0';
	refresh(cls);
    }
    if(cls->pos == 0 || cls->pos == cls->len){
        cmd_line_beep();
    }
}

void cmd_line_move_home(cmd_line_state *cls){
    if(cls->pos != 0){
        cls->pos = 0;
	refresh(cls);
    }
}

void cmd_line_move_end(cmd_line_state *cls){
    if(cls->pos != 0){
        cls->pos = cls->len;
	refresh(cls);
    }
}

// On success, return length of the current buffer
// On error, return -1
int cmd_line_edit(char* buf, char* prompt, size_t len){
    char c = '\0';
    int n = 0;
    cmd_line_state cls;
    char seq[3];

    cls.buf = buf;
    cls.prompt = prompt;
    cls.len = 0;
    cls.pos = 0;
    cls.col = get_columns();

    while(true){
        n = read(STDIN_FILENO, &c, 1);

        switch(c){
            case CTRL_B:
                cmd_line_move_left(&cls);
		break;
            case CTRL_C:
		errno = EAGAIN;
                return -1;
            case CTRL_F:
                cmd_line_move_right(&cls);
		break;
            case BACKSPACE:
		cmd_line_backspace(&cls);
		break;
            case TAB:
		printf("TAB\n");
		return 1;
		break;
            case ENTER:
		return (int)cls.len;
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
                                    cmd_line_del(&cls);
                                    break;
                            }
                        }
                    }else {
		        switch (seq[1]) {
			    case 'A': /* Up */
                                //linenoiseEditHistoryNext(&l, LINENOISE_HISTORY_PREV);
                                break;
                            case 'B': /* Down */
                                //linenoiseEditHistoryNext(&l, LINENOISE_HISTORY_NEXT);
                                break;
                            case 'C': /* Right */
                                cmd_line_move_right(&cls);
                                break;
                            case 'D': /* Left */
                                cmd_line_move_left(&cls);
                                break;
                            case 'H': /* Home */
                                cmd_line_move_home(&cls);
                                break;
                            case 'F': /* End*/
                                cmd_line_move_end(&cls);
                                break;
			}
		    }
	        }else if (seq[0] == 'O') {/* ESC O sequences. */
                    switch (seq[1]) {
                        case 'H': /* Home */
                            cmd_line_move_home(&cls);
                            break;
                        case 'F': /* End*/
                            cmd_line_move_end(&cls);
                            break;
                    }
                }
		break;
	    default:
		cmd_line_ins(&cls, c);
		break;
	}
    }
}

static int cmd_line_raw(char *buf, char *prompt){
    int n = 0;
    struct termios orig;

    if(tcgetattr(STDIN_FILENO, &orig) < 0)
	return -1;
    if(!enable_raw_mode())
	return -1;
    n = cmd_line_edit(buf, prompt, max_line);
    if(!disable_raw_mode(&orig))
        return -1;
    return n;
}

char *cmd_line(){
    char *prompt = "cmd> ";
    size_t len;
    char *buf = calloc(max_line, sizeof(char));
    mem_alloc_succ(buf);

    if(!isatty(STDIN_FILENO)){
	return cmd_line_noTTY();
    }

    /*printf("%s", prompt);
    fflush(stdout);
    if(fgets(buf, max_line, stdin) == NULL)
        return NULL;
    len = strlen(buf);
    while(len && buf[len - 1] == '\n' || buf[len - 1] == '\r'){
        len--;
	buf[len] = '\0';
    }*/
    cmd_line_raw(buf, prompt);
    return buf;
}

int add_history_cmd(const char *file_name)
{
    mode_t old_umask = umask(S_IXUSR | S_IRWXG | S_IRWXO);
    FILE *fp;
    int j;

    fp = fopen(file_name, "w");
    umask(old_umask);
    if (fp == NULL)
        return -1;
    chmod(file_name, S_IRUSR | S_IWUSR);
    for (j = 0; j < history_len; j++)
        fprintf(fp, "%s\n", history[j]);
    fclose(fp);
    return 0;
}
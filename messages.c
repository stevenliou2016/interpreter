#include <stdio.h>
#include <stdbool.h>
#include<stdarg.h>

bool is_visible = false;
char *log_file = NULL;

void message_init(bool is_v, char *l_file){
    is_visible = is_v;
    log_file = l_file;
}

void do_log(const char *format, va_list vlist){
    FILE *fp;

    if(!(fp = fopen(log_file, "a"))){
        printf("open file %s failed\n", log_file);
    }
    vfprintf(fp, format, vlist);
    if(fp){
        fclose(fp);
    }
}

void show_message(const char *format, ...){
    va_list vlist;
    FILE *fp;

    va_start(vlist, format);
    if(is_visible){
        vprintf(format, vlist);
    }
    if(log_file){
        do_log(format, vlist);
    }
    va_end(vlist);
}

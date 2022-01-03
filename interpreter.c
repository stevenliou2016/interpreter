#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>
#include "command_line.h"
#include "console.h"
#include "mem_manage.h"
#include "messages.h"

void usage(char *name){
    printf("Usage: %s [options] [args]\n", name);
    printf("\t-h			#Usage\n");
    printf("\t-f INPUT_FILE		#Read command from file\n");
    printf("\t-v 			#Enable print messages\n");
    printf("\t-l LOG_FILE		#Log print messages\n");
    exit(0);
}

int main(int argc, char **argv){
    char *input_file = NULL;
    char *l_file = NULL; // log file
    size_t len = 0;
    bool is_v = false; // the messages are visible if it's true
    char c = '\0';
    time_t seconds = 0;
    struct tm *today;

    while((c = getopt(argc, argv, "hf:vl")) != -1){
        switch(c){
            case 'h':
                usage(argv[0]);
		exit(0);
	    case 'f':
		len = strlen(optarg);
		input_file = malloc(len + 1);
		if(!mem_alloc_succ(input_file)){
                    exit(-1);
		}
                strncpy(input_file, optarg, len);
                input_file[len] = '\0';
		break;
            case 'v':
                is_v = true;
                break;
	    case 'l':
		l_file = malloc(20);
                if(!mem_alloc_succ(l_file)){
                    exit(-1);
                }
		memset(l_file, 0, 20);
		time(&seconds);
		today = localtime(&seconds);
		sprintf(l_file, "log%04d-%02d-%02d", today->tm_year + 1900, today->tm_mon + 1, today->tm_mday);
		break;
            default:
                printf("Unknown option %c\n", c);
		usage(argv[0]);
		exit(0);
	}
    }
    console_init();
    message_init(is_v, l_file);
    if(!run_console(input_file, l_file, is_v)){
        return -1;
    }
    return 0; 
}

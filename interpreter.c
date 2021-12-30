#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include "command_line.h"
#include "console.h"
#include "mem_manage.h"

void usage(char *name){
    printf("Usage: %s [options] [args]\n", name);
    printf("\t-h			#Usage\n");
    printf("\t-f INPUT_FILE		#Read command from file\n");
    printf("\t-v 			#Enable print messages\n");
    printf("\t-l LOG_FILE		#Log print messages\n");
    exit(0);
}

int main(int argc, char **argv){
    unsigned int file_size = 256;
    char *input_file = calloc(file_size, sizeof(char));
    char *log_file = calloc(file_size, sizeof(char));
    bool is_visible = false;
    char c = '\0';

    while((c = getopt(argc, argv, "hf:vl:")) != -1){
        switch(c){
            case 'h':
                usage(argv[0]);
		break;
	    case 'f':
                strncpy(input_file, optarg, file_size);
		break;
            case 'v':
                is_visible = true;
                break;
	    case 'l':
                strncpy(log_file, optarg, file_size);
		break;
            default:
                printf("Unknown option %c \n", c);
		usage(argv[0]);
		break;
	}
    }
    console_init();
    if(!run_console()){
        return -1;
    }
    return 0; 
}

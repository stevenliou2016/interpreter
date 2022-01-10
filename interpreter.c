#include "interpreter_command_line.h"
#include "interpreter_console.h"
#include "interpreter_mem.h"
#include "interpreter_msg.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

void Usage(char *program_name) {
  printf("Usage: %s [options] [args]\n", program_name);
  printf("\t-h			#Usage\n");
  printf("\t-f INPUT_FILE	#Read command from file\n");
  printf("\t-v 			#Enable print messages\n");
  printf("\t-l LOG_FILE		#Log print messages\n");
  exit(0);
}

int main(int argc, char **argv) {
  char *input_file = NULL; /* Path of command file */
  char *log_file = NULL; 
  size_t input_file_len = 0;
  size_t log_file_len = 20;
  bool is_visible = false; /* the messages are visible if it's true */
  char c = '\0';
  time_t seconds = 0;
  struct tm *today;

  while ((c = getopt(argc, argv, "hf:vl")) != -1) {
    switch (c) {
    case 'h': 
      Usage(argv[0]);
      exit(0);
    case 'f': 
      input_file_len = strlen(optarg);
      input_file = malloc((input_file_len + 1) * sizeof(char));
      if (!IsMemAlloc(input_file)) {
        exit(-1);
      }
      memset(input_file, 0, (input_file_len + 1) * sizeof(char));
      strncpy(input_file, optarg, input_file_len);
      input_file[input_file_len] = '\0';
      break;
    case 'v':
      is_visible = true;
      break;
    case 'l':
      log_file = malloc((log_file_len + 1) * sizeof(char));
      if (!IsMemAlloc(log_file)) {
        exit(-1);
      }
      memset(log_file, 0, (log_file_len + 1) * sizeof(char));
      /* Generates log name which is "log + date of today"
       * Format YYYY-MM-DD, e.g., log2022-01-01*/
      time(&seconds);
      today = localtime(&seconds);
      sprintf(log_file, "log%04d-%02d-%02d", today->tm_year + 1900,
              today->tm_mon + 1, today->tm_mday);
      break;
    default:
      printf("Unknown option %c\n", c);
      Usage(argv[0]);
      exit(0);
    }
  }
  if(!ConsoleInit())
    return -1;
  SetMsgVisible(is_visible);
  SetLogFile(log_file);
  if (!RunConsole(input_file, log_file, is_visible)) {
    return -1;
  }
  return 0;
}

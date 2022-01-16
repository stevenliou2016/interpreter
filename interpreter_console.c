#include "interpreter_console.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <sys/wait.h>
#include <time.h>
#include "client/interpreter_client.h"
#include "interpreter_cmd_line.h"
#include "interpreter_mem.h"
#include "interpreter_msg.h"
#include "interpreter_queue.h"
#include "interpreter_server.h"

const char g_history_file_name[] = ".history_cmd";
bool g_quit = false;
Queue *g_queue = NULL;
pid_t g_pid = -2; /* Server process ID; -2 is default value */
CmdElementPtr g_cmd_list = NULL;

/* Internal functions */
static bool HelpOperation(int argc, char **argv);
static bool QueueNewOperation(int argc, char **argv);
static bool QueueFreeOperation(int argc, char **argv);
static bool QueueInsertHeadOperation(int argc, char **argv);
static bool QueueInsertTailOperation(int argc, char **argv);
static bool QueueRemoveHeadOperation(int argc, char **argv);
static bool QueueSizeOperation(int argc, char **argv);
static bool QueueReverseOperation(int argc, char **argv);
static bool QueueSortOperation(int argc, char **argv);
static bool QueueShowOperation(int argc, char **argv);
static bool ServerOperation(int argc, char **argv);
static bool ClientOperation(int argc, char **argv);
static bool QuitOperation(int argc, char **argv);
static bool AddCmd(char *cmd, char *doc, CmdFunction op);

bool ConsoleInit() {
  g_cmd_list = NULL;
  srand(time(NULL)); /* For random string */
  /* Adds commands into command list */
  if(!AddCmd("help", "\t#Show documents", HelpOperation)){
    QuitOperation(0, NULL);
    return false;
  }
  if(!AddCmd("new", "\t#Create a queue", QueueNewOperation)){
    QuitOperation(0, NULL);
    return false;
  }
  if(!AddCmd("free", "\t#Delete a queue", QueueFreeOperation)){
    QuitOperation(0, NULL);
    return false;
  }
  if(!AddCmd("ih",
          " str [n]\t#Insert n times of str at head, n>=1. Generate a string "
          "if str is RAND",
          QueueInsertHeadOperation)){
    QuitOperation(0, NULL);
    return false;
  }
  if(!AddCmd("it",
          " str [n]\t#Insert n times of str at tail, n>=1. Generate a string "
          "if str is RAND",
          QueueInsertTailOperation)){
    QuitOperation(0, NULL);
    return false;
  }
  if(!AddCmd("rh", "\t#Remove the first element", QueueRemoveHeadOperation)){
    QuitOperation(0, NULL);
    return false;
  }
  if(!AddCmd("size", "\t#Show the size of queue", QueueSizeOperation)){
    QuitOperation(0, NULL);
    return false;
  }
  if(!AddCmd("reverse", "\t#Reverse the queue", QueueReverseOperation)){
    QuitOperation(0, NULL);
    return false;
  }
  if(!AddCmd("sort", "\t#Sort the queue", QueueSortOperation)){
    QuitOperation(0, NULL);
    return false;
  }
  if(!AddCmd("show", "\t#Show the queue", QueueShowOperation)){
    QuitOperation(0, NULL);
    return false;
  }
  if(!AddCmd("server", "\t#Activate server", ServerOperation)){
    QuitOperation(0, NULL);
    return false;
  }
  if(!AddCmd("client", "\t#Activate client", ClientOperation)){
    QuitOperation(0, NULL);
    return false;
  }
  if(!AddCmd("quit", "\t#Exit program", QuitOperation)){
    QuitOperation(0, NULL);
    return false;
  }
  /* Loads history command from a file .history.cmd */
  if(!LoadHistory(g_history_file_name)){
    QuitOperation(0, NULL);
    return false;
  }
  return true;
}

/* Trims leading and tailing spaces 
 * On success, returns a pointer to a copy string
 * Returns NULL if the input is NULL
 * The returned pointer needs to be freed by caller */
char *TrimSpace(char *str) {
  char *end = NULL;
  char *new_str = NULL;
  char *str_ptr = str;
  size_t str_len = 0;

  if (!str_ptr) {
    return str_ptr;
  }
  /* Trim leading whitespace */
  while (isspace(*str_ptr)) {
    str_ptr++;
  }
  if (*str_ptr == '\0') {
    /* Allocates new memory spaces to avoid possibly lost */
    new_str = malloc(sizeof(char));
    if(!IsMemAlloc(new_str)){
      return NULL;
    }
    memset(new_str, 0, sizeof(char));
    return new_str;
  }
  /* Trim tailing whitespace */
  str_len = strlen(str_ptr);
  end = str_ptr + str_len - 1;
  while (end > str_ptr && isspace(*end)) {
    end--;
  }
  *(end + 1) = '\0';

  str_len = strlen(str_ptr);
  new_str = malloc((str_len + 1) * sizeof(char));
  if(!IsMemAlloc(new_str)){
    return NULL;
  }
  memset(new_str, 0, (str_len + 1) * sizeof(char));
  strncpy(new_str, str_ptr, str_len);
  new_str[str_len] = '\0';

  return new_str;
}

char *TrimNewLine(char *str) {
  char *str_ptr = str;

  if (!str) {
    return str;
  }
  while (*str_ptr != '\n' && *str_ptr != '\0') {
    str_ptr++;
  }
  *str_ptr = '\0';

  return str;
}

/* Split cmd into strings by space 
 * For example, cmd = "server -p 9999", 
   argv will be ["server", "-p", "9999"]
 * On success, returns a pointer of pointer to strings 
 * On error, returns NULL */
static char **SplitCmd(int *argc, char *cmd) {
  size_t args_max_num = 8;
  const char delim[] = " ";
  char **argv = NULL;
  char **argv_ptr = NULL;

  if (!cmd || !argc){
    return NULL;
  }

  argv = malloc((args_max_num + 1) * sizeof(char *));
  if(!IsMemAlloc(argv)){
    return NULL;
  }
  memset(argv, 0, (args_max_num + 1) * sizeof(char *));
  argv_ptr = argv;

  (*argc)++;
  *argv_ptr = strtok(cmd, delim);
  /*if (strncmp("server", cmd, 6) != 0 && strncmp("client", cmd, 6) != 0) {
    *argv_ptr = TrimNewLine(*argv_ptr);
  }*/
  while ((*argc) < args_max_num && *argv_ptr++) {
    *argv_ptr = strtok(NULL, delim);
    if (strncmp("server", cmd, 6) != 0 && strncmp("client", cmd, 6) != 0) {
      *argv_ptr = TrimNewLine(*argv_ptr);
    }
    if (*argv_ptr) {
      (*argc)++;
    }
  }

  return argv;
}

static bool IsQueueNULL() {
  if (!g_queue) {
    ShowMsg("the queue is NULL\n");
    return true;
  }

  return false;
}

/* Show all commands and document of these commands */
static bool HelpOperation(int argc, char **argv) {
  CmdElementPtr cmd_list = g_cmd_list;

  printf("\tCommand\tDescription\n");
  fflush(stdout);

  while (cmd_list) {
    printf("\t%s%s\n", cmd_list->cmd, cmd_list->doc);
    fflush(stdout);
    cmd_list = cmd_list->next;
  }

  return true;
}

/* Shows elements of queue 
 * On success, returns true*/
static bool QueueShowOperation(int argc, char **argv) {
  ListElement *head = g_queue->head;
  bool is_visible = g_is_visible;
  size_t show_len = 4;
  bool is_show_cmd = false;

  if (IsQueueNULL() || !argv || !*argv) {
    return false;
  }

  if(strlen(*argv) == show_len && strncmp(*argv, "show", show_len) == 0){
    is_show_cmd = true;
    g_is_visible = true;
  }

  if (g_is_visible || g_log_file) {
    if (head && head->value) {
      ShowMsg("queue = [%s", head->value);
      head = head->next;
    } else {
      ShowMsg("queue = [");
    }
    while (head) {
      ShowMsg(", %s", head->value);
      head = head->next;
    }
    ShowMsg("]\n");
  }

  if(is_show_cmd){
    g_is_visible = is_visible;
  }

  return true;
}

static bool QueueNewOperation(int argc, char **argv) {
  if (g_queue) {
    free(g_queue);
    g_queue = NULL;
  }

  g_queue = QueueNew();
  if (!IsMemAlloc(g_queue)) {
    return false;
  }
  QueueShowOperation(argc, argv);

  return true;
}

static bool QueueFreeOperation(int argc, char **argv) {
  if (IsQueueNULL()) {
    return false;
  }

  QueueFree(g_queue);
  g_queue = NULL;
  ShowMsg("the queue is freed\n");

  return true;
}

/* Generates a random string whose length is between 5 and 10 
 * On success, return the string
 * On error, return NULL 
 * The returned pointer needs to be freed by caller */
char *RandomString() {
  const char alphabets[] = "abcdefghijklmnopqrstuvwxyz";
  size_t max_str_len = 10;
  size_t min_str_len = 5;
  size_t str_len = 0;
  char *str = NULL;

  while (str_len < min_str_len) {
    str_len = rand() % (max_str_len + 1);
  }

  str = malloc((str_len + 1) * sizeof(char));
  if (!IsMemAlloc(str)) {
    return NULL;
  }
  memset(str, 0, (str_len + 1) * sizeof(char));

  for (int i = 0; i < str_len; i++) {
    str[i] = alphabets[rand() % 26];
  }
  str[str_len] = '\0';

  return str;
}

/* Inserts str at head of queue num times
 * str is random string if argv[1] is "RAND"
 * On success, return true */
static bool QueueInsertHeadOperation(int argc, char **argv) {
  int num = 1;
  char *str = NULL;
  bool is_random = false;

  if (IsQueueNULL() || argc < 2 || !argv) {
    return false;
  }
  if (argc > 2 && argv[2]) {
    num = atoi(argv[2]);
    if (num < 1) {
      ShowMsg("number must be greater than 0\n");
      return false;
    }
  }
  if(*(argv + 1)){
    if (strlen(*(argv + 1)) == 4 && strncmp("RAND", *(argv + 1), 4) == 0) {
      is_random = true;
    } else {
      str = *(argv + 1);
    }
  }else{
    return false;
  }
  for (int i = 0; i < num; i++) {
    if (is_random) {
      str = RandomString();
    }
    if (!QueueInsertHead(g_queue, str)) {
      ShowMsg("insert a string at the head of queue failed\n");
      if (is_random) {
        free(str);
      }
      return false;
    }
    if (is_random) {
      free(str);
    }
  }
  QueueShowOperation(argc, argv);

  return true;
}

/* Inserts str at tail of queue 
 * str is random string if argv[1] is "RAND" 
 * On success, return true */
static bool QueueInsertTailOperation(int argc, char **argv) {
  int num = 1;
  char *str = NULL;
  bool is_random = false;

  if (IsQueueNULL() || argc < 2 || !argv) {
    return false;
  }
  if (argc > 2 && argv[2]) {
    num = atoi(argv[2]);
    if (num < 1) {
      ShowMsg("number must be greater than 0\n");
      return false;
    }
  }
  if(*(argv + 1)){
    if (strlen(*(argv + 1)) == 4 && strncmp("RAND", *(argv + 1), 4) == 0) {
      is_random = true;
    } else {
      str = *(argv + 1);
    }
  }else{
    return false;
  }
  for (int i = 0; i < num; i++) {
    if (is_random) {
      str = RandomString();
    }
    if (!QueueInsertTail(g_queue, str)) {
      ShowMsg("insert a string at the tail of queue failed\n");
      if (is_random) {
        free(str);
      }
      return false;
    }
    if (is_random) {
      free(str);
    }
  }
  QueueShowOperation(argc, argv);

  return true;
}

/* Removes the first element of queue 
 * On success, return true */
static bool QueueRemoveHeadOperation(int argc, char **argv) {
  if (IsQueueNULL()) {
    return false;
  }

  if (!g_queue->head) {
    QueueShowOperation(argc, argv);
    return true;
  }
  if (!QueueRemoveHead(g_queue)) {
    ShowMsg("remove the first element failed\n");
    return false;
  }
  QueueShowOperation(argc, argv);

  return true;
}

/* Shows size of queue 
 * On success, return true */
static bool QueueSizeOperation(int argc, char **argv) {
  if (IsQueueNULL()) {
    return false;
  }

  ShowMsg("the size of queue is %d\n", QueueSize(g_queue));

  return true;
}

/* Reverses the queue 
 * On success, return true */
static bool QueueReverseOperation(int argc, char **argv) {
  if (IsQueueNULL()) {
    return false;
  }

  QueueReverse(g_queue);
  QueueShowOperation(argc, argv);

  return true;
}

/* Sorts the queue
 * On success, return true */
static bool QueueSortOperation(int argc, char **argv) {
  if (IsQueueNULL()) {
    return false;
  }

  QueueSort(g_queue);
  QueueShowOperation(argc, argv);

  return true;
}

/* For signal SIGCLD */
static void sig_cld() {
  g_pid = -2; /* Resets g_pid */
}

/* Runs operation of server 
 * On success, return true */
static bool ServerOperation(int argc, char **argv) {
  if (argc > 1 && argv[1] && strncmp(argv[1], "-s", 2) == 0) {
    if (g_pid != -2) {
      kill(g_pid, SIGTERM);
      g_pid = -2;
      return true;
    } else {
      ShowMsg("there is no server running\n");
      return false;
    }
  }
  if (g_pid > 0) {
    ShowMsg("the server is running\n");
    return true;
  }
  signal(SIGCLD, sig_cld);
  g_pid = fork();
  if (g_pid == -1) {
    ShowMsg("fork failed\n");
    return false;
  } else if (g_pid == 0) { /* Child process*/
    if (!RunServer(argc, argv)) {
      QuitOperation(argc, argv);
      exit(-1);
    }
    QuitOperation(argc, argv);
    exit(0);
  }
  /* For showing order of message correctly */
  if (argc > 1 && argv[1] && strncmp(argv[1], "-h", 2) == 0) {
    wait(NULL);
    g_pid = -2;
  }

  return true;
}

/* Runs operation of client 
 * On success, return true */
static bool ClientOperation(int argc, char **argv) {
  pid_t client_pid = fork();

  if(client_pid == 0){ /* Child process */
    if (!RunClient(argc, argv)) {
      ShowMsg("running client failed\n");
      QuitOperation(argc, argv);
      exit(-1);
    }
    QuitOperation(argc, argv);
    exit(0);
  }

  return true;
}

/* Quits console
 * On success, return true */
static bool QuitOperation(int argc, char **argv) {
  g_quit = true;
  if (g_queue) {
    QueueFreeOperation(argc, argv);
  }
  /* Inactivates server if it's running */
  if (g_pid != -2) {
    kill(g_pid, SIGTERM);
  }
  FreeCmdList(g_cmd_list);

  return true;
}

/* Adds a command into command list 
 * On success, return true */
bool AddCmd(char *cmd, char *doc, CmdFunction op) {
  int cmd_len = 0;
  int doc_len = 0;
  CmdElementPtr element = g_cmd_list;
  CmdElementPtr *last = &g_cmd_list; 
  CmdElementPtr new_cmd_element = NULL;

  if(!cmd || !doc || !op){
    return false;
  }
  cmd_len = strlen(cmd);
  doc_len = strlen(doc);
  /* Gets the address of last element of command list */
  while (element) {
    last = &(element->next);
    element = element->next;
  }

  new_cmd_element = malloc(sizeof(CmdElement));
  if (!IsMemAlloc(new_cmd_element)) {
    return false;
  }
  memset(new_cmd_element, 0, sizeof(CmdElement));

  new_cmd_element->cmd = malloc((cmd_len + 1) * sizeof(char));
  if (!IsMemAlloc(new_cmd_element->cmd)) {
    FreeCmdList(new_cmd_element);
    return false;
  }
  memset(new_cmd_element->cmd, 0, (cmd_len + 1) * sizeof(char));
  strncpy(new_cmd_element->cmd, cmd, cmd_len);
  new_cmd_element->cmd[cmd_len] = '\0';

  new_cmd_element->doc = malloc((doc_len + 1) * sizeof(char));
  if (!IsMemAlloc(new_cmd_element->doc)) {
    FreeCmdList(new_cmd_element);
    return false;
  }
  memset(new_cmd_element->doc, 0, (doc_len + 1) * sizeof(char));
  strncpy(new_cmd_element->doc, doc, doc_len);
  new_cmd_element->doc[doc_len] = '\0';

  new_cmd_element->op = op;
  new_cmd_element->next = NULL;
  *last = new_cmd_element;

  return true;
}

bool RunConsole(char *input_file, char *log_file, bool is_visible) {
  char *cmd = NULL;
  char *trim_cmd = NULL;
  char **argv = NULL;
  bool ret = false;
  int argc = 0;
  size_t cmd_line_len = 0;
  ssize_t num_read = 0;
  FILE *input_file_ptr = NULL;
  CmdElementPtr cmd_list;

  g_is_visible = is_visible;
  g_log_file = log_file;
  CmdLineInit();

  if (g_log_file) {
    g_is_visible = false;
    ShowMsg("=====start running =====\n");
    g_is_visible = is_visible;
  }
  /* Checks existence of file */
  if (input_file && access(input_file, F_OK) == -1) {
    ShowMsg("%s does not exist\n", input_file);
    QuitOperation(0, NULL);
    return false;
  }
  if (input_file) {
    if (!(input_file_ptr = fopen(input_file, "r"))) {
      ShowMsg("open file %s failed\n", input_file);
      QuitOperation(0, NULL);
      return false;
    }
  }
  while (!g_quit) {
    argc = 0;
    cmd = NULL;
    trim_cmd = NULL;
    cmd_list = g_cmd_list;

    if (input_file) { /* Inputs from input file */
      /* On success, returns the number of character read 
       * On error or EOF, returns -1 */
      if ((num_read = getline(&cmd, &cmd_line_len, input_file_ptr)) == -1) {
	FreeString(1, cmd);
	QuitOperation(0, NULL);
	fclose(input_file_ptr);
        if(errno == 0){ /* EOF */
	  return true;
	}
        return false; /* Error */
      }
    } else { /* Inputs from console */
      if((cmd = CmdLine()) == NULL){
	FreeString(1, cmd);
        QuitOperation(0, NULL);
        return false;
      }
    }

    trim_cmd = TrimSpace(cmd);
    FreeString(1, cmd);
    if (*trim_cmd == '#') {
      /* Shows the description of test case */
      printf("%s\n", trim_cmd);
      FreeString(1, trim_cmd);
      continue;
    } else if(trim_cmd == NULL || *trim_cmd == '\0') {
      FreeString(1, trim_cmd);
      continue;
    }

    ShowMsg("%s\n", trim_cmd);
    if (!input_file) {
      /* Adds a new command into command list */
      if(!AddHistoryCmd(trim_cmd)) {
        FreeString(1, trim_cmd);
        QuitOperation(0, NULL);
        return false;
      }
      /* Saves command list in g_history_file_name */
      if(!SaveHistoryCmd(g_history_file_name)) {
        FreeString(1, trim_cmd);
        QuitOperation(0, NULL);
        return false;
      }
    }
    if((argv = SplitCmd(&argc, trim_cmd)) == NULL){
      FreeString(1, trim_cmd);
      continue;
    }
    while (cmd_list &&strncmp(*argv, cmd_list->cmd, strlen(*argv)) != 0) {
      cmd_list = cmd_list->next;
    }
    if (cmd_list) {
      ret = cmd_list->op(argc, argv);
      if (input_file){
        FreeString(1, trim_cmd);
	free(argv);
        QuitOperation(0, NULL);
        fclose(input_file_ptr);
        return ret;
      }
    } else {
      ShowMsg("unknown command:%s\n", *argv);
      fflush(stdout);
      if (input_file){
        FreeString(1, trim_cmd);
	free(argv);
        QuitOperation(0, NULL);
        fclose(input_file_ptr);
        return false;
      }
    }
    free(argv);
    FreeString(1, trim_cmd);
  }
  QuitOperation(0, NULL);
  fclose(input_file_ptr);

  return true;
}

#include "interpreter_mem.h"
#include "interpreter_msg.h"
#include <stdio.h>

bool IsMemAlloc(void *ptr) {
  if (!ptr) {
    ShowMsg("memory allocation failed\n");
    return false;
  }
  return true;
}

void FreeString(size_t free_num, char *str, ...){
  va_list args;
  char *next_str = NULL;

  if(str){
    free(str);
  }
  va_start(args, str);
  for(int i = 1; i < free_num; i++){
    if((next_str = va_arg(args, char *)) != NULL){
      free(next_str);
    }
  }
  va_end(args);
}

void FreeCmdList(CmdElementPtr cmd_head){
  CmdElementPtr cmd_next = cmd_head;

  while(cmd_next){
    if(cmd_next->cmd){
      free(cmd_next->cmd);
    }
    if(cmd_next->doc){
      free(cmd_next->doc);
    }
    cmd_next = cmd_next->next;
    free(cmd_head);
    cmd_head = cmd_next;
  }
}

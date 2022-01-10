#include "interpreter_mem.h"
#include "interpreter_msg.h"

bool IsMemAlloc(void *ptr) {
  if (!ptr) {
    ShowMsg("memory allocation failed\n");
    return false;
  }
  return true;
}

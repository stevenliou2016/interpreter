#ifndef INTERPRETER_SERVER_H_
#define INTERPRETER_SERVER_H_
#include <stdbool.h>
#include <sys/types.h>

typedef struct HttpRequest {
  char *file_name;
  off_t offset;
  size_t end;
} HttpRequest;

bool RunServer(int argc, char **argv);
#endif

#ifndef __SERVER_H__
#define __SERVER_H__

typedef struct http_request{
    char *file_name;
    off_t offset;
    size_t end;
}http_request;

bool server(int, char **);
#endif

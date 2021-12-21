#ifndef _RIO_H
#define _RIO_H
#include <unistd.h>

#define MAXLINE 1024 /* max length of a line */
#define WEB_RIO_BUFSIZE 1024

typedef struct {
    int rio_fd;                    /* descriptor for this buf */
    int rio_cnt;                   /* unread byte in this buf */
    char *rio_bufptr;              /* next unread byte in this buf */
    char rio_buf[WEB_RIO_BUFSIZE]; /* internal buffer */
} web_rio_t;

ssize_t writen(int, void *, size_t);
void rio_read_init(web_rio_t *, int);
ssize_t rio_read_line(web_rio_t *, void *, size_t);
#endif

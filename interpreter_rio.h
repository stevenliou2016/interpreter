#ifndef INTERPRETER_RIO_H_
#define INTERPRETER_RIO_H_
#include <sys/types.h>

#define MAXLINE 1024 /* max length of a line */
#define RIO_BUFSIZE 1024

typedef struct RIO{
  /* a file descriptor for this buf */
  int rio_fd;
  /* unread bytes in this buf */
  int rio_cnt;
  /* next unread byte in this buf */
  char *rio_bufptr;
  /* internal buffer */
  char rio_buf[RIO_BUFSIZE];
} RIO;

/* Sends data of usr_buf to a file descriptor fd
 * On success, the number of bytes written is returned
 * On error, return -1 and errno is set to indicate the error */
ssize_t WriteNum(int fd, void *usr_buf, size_t usr_buf_len);
/* Initializes RIO */
void RioReadInit(RIO *rp, int fd);
/* Reads usrbuf of maxlen from a file descriptor rp->rio_fd
 * On success, the number of bytes read is returned(zero 
 * indicates end of file)
 * On error, return -1 and errno is set to indicate the error */
ssize_t RioReadLine(RIO *rp, void *usr_buf, size_t max_len);
#endif

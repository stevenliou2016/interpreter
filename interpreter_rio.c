#include "interpreter_rio.h"
#include <errno.h>
#include <string.h>
#include <unistd.h>

ssize_t WriteNum(int fd, void *usr_buf, size_t usr_buf_len) {
  size_t usr_buf_len_left = usr_buf_len;
  ssize_t usr_buf_len_written;
  char *buf = usr_buf;

  while (usr_buf_len_left > 0) {
    if ((usr_buf_len_written = write(fd, buf, usr_buf_len_left)) <= 0) {
      /* Interrupted by sig handler return */
      if (errno == EINTR) { 
        usr_buf_len_written = 0; /* Calls write() again */
      } else {
        return -1; /* Errno set by write() */
      }
    }
    usr_buf_len_left -= usr_buf_len_written;
    buf += usr_buf_len_written;
  }
  return usr_buf_len;
}


/* rio_read - This is a wrapper for the Unix read() function that
   transfers min(n, rio_cnt) bytes from an internal buffer to a user
   buffer, where n is the number of bytes requested by the user and
   rio_cnt is the number of unread bytes in the internal buffer. On
   entry, rio_read() refills the internal buffer via a call to
   read() if the internal buffer is empty */
static ssize_t RioRead(RIO *rp, char *usr_buf, size_t num) {
  int cnt = 0;

  while (rp->rio_cnt <= 0) { /* Refills if buf is empty */

    rp->rio_cnt = read(rp->rio_fd, rp->rio_buf, sizeof(rp->rio_buf));
    if (rp->rio_cnt < 0) {
      if (errno != EINTR) { /* Interrupted by sig handler return */
        return -1;
      }
    } else if (rp->rio_cnt == 0) { /* EOF */
      return 0;
    } else
      rp->rio_bufptr = rp->rio_buf; /* Resets buffer ptr */
  }

  /* Copys min(n, rp->rio_cnt) bytes from internal buf to user buf */
  cnt = num;
  if (rp->rio_cnt < num) {
    cnt = rp->rio_cnt;
  }
  memcpy(usr_buf, rp->rio_bufptr, cnt);
  rp->rio_bufptr += cnt;
  rp->rio_cnt -= cnt;
  return cnt;
}

void RioReadInit(RIO *rp, int fd) {
  rp->rio_fd = fd;
  rp->rio_cnt = 0;
  rp->rio_bufptr = rp->rio_buf;
}

ssize_t RioReadLine(RIO *rp, void *usr_buf, size_t max_len) {
  int num = 0;
  int read_cnt = 0;
  char c, *buf = usr_buf;

  for (num = 1; num < max_len; num++) {
    if ((read_cnt = RioRead(rp, &c, 1)) == 1) {
      *buf++ = c;
      if (c == '\n') {
        break;
      }
    } else if (read_cnt == 0) {
      if (num == 1) {
        return 0; /* EOF, no data read */
      } else {
        break; /* EOF, some data was read */
      }
    } else {
      return -1; /* Error */
    }
  }
  *buf = 0;
  return num;
}

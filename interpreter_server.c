#include "interpreter_server.h"
#include "interpreter_mem.h"
#include "interpreter_msg.h"
#include "interpreter_rio.h"
#include <dirent.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static void PrintUsage() {
  printf("\tCommand\t\tDescription\n");
  printf("\tserver\t\t#Use default port(9999), serve current directory\n");
  printf("\tserver -h\t#Usage\n");
  printf("\tserver -s \tInactivate server\n");
  printf("\tserver -d dir\t#Use default port(9999), ");
  printf("serve given directory\n");
  printf("\tserver -p port\t#Use given port, serve current directory\n");
}

static int ServerSocketToListen(size_t port) {
  int server_sock_fd = 0;
  int opt_val = 1;
  struct sockaddr_in server_addr;

  /* Create an endpoint for communication
   * Domain:AF_INET stands for IPv4
   * SOCK_STREAM provides sequenced, reliable, two-way,
   * connection-based byte streams
   * Choose a protocol automatically if third argument is 0
   * On success, return a file descriptor
   * On error, return -1 */
  if ((server_sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    return -1;
  }
  /* Eliminates "Address already in use" error from bind. */
  if (setsockopt(server_sock_fd, SOL_SOCKET, SO_REUSEADDR,
                 (const void *)&opt_val, sizeof(int)) < 0) {
    return -1;
  }
  /* 6 is TCP's protocol number
   * Enable this, much faster : 4000 req/s -> 17000 req/s */
  if (setsockopt(server_sock_fd, 6, TCP_CORK, (const void *)&opt_val,
                 sizeof(int)) < 0) {
    return -1;
  }
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET; /* IPv4 */
  /* Convert the unsigned integer host‐long from host byte
   * order to network byte order
   * INADDR_ANY:accept connection from every place */
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  /* Convert the unsigned short integer
     hostshort from host byte order to network byte order */
  server_addr.sin_port = htons(port);
  if (bind(server_sock_fd, (struct sockaddr *)&server_addr,
           sizeof(server_addr)) < 0) {
    return -1;
  }
  /* Make it a listening socket ready to accept connection requests.
   * 1024 is the maximum number of incoming requests */
  if (listen(server_sock_fd, 1024) < 0) {
    return -1;
  }
  return server_sock_fd;
}

/* Converts Bytes to K/M/GBytes */
void FormatSize(char *buf, struct stat *stat) {
  off_t size = 0;

  /* Directory */
  if (S_ISDIR(stat->st_mode)) {
    sprintf(buf, "%s", "[DIR]");
  } else { /* File */
    size = stat->st_size; /* Size of file */
    if (size < 1024) {
      sprintf(buf, "%lu", size);
    } else if (size < 1024 * 1024) {
      /* Converts Bytes to KBytes */
      sprintf(buf, "%.1fK", (double)size / 1024);
    } else if (size < 1024 * 1024 * 1024) {
      /* Converts Bytes to MBytes */
      sprintf(buf, "%.1fM", (double)size / 1024 / 1024);
    } else {
      /* Converts Bytes to GBytes */
      sprintf(buf, "%.1fG", (double)size / 1024 / 1024 / 1024);
    }
  }
}

HttpRequest *HttpRequestInit() {
  size_t file_name_size = 1024;
  HttpRequest *req = NULL;

  req = malloc(sizeof(HttpRequest));
  if (!IsMemAlloc(req)) {
    return NULL;
  }
  req->file_name = malloc(file_name_size);
  if (!IsMemAlloc(req->file_name)) {
    return NULL;
  }
  memset(req->file_name, 0, file_name_size);
  req->offset = 0;
  req->end = 0;
  return req;
}

HttpRequest *ParseRequest(int client_fd) {
  int length = 0;
  int i = 0;
  char *file_name = NULL;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE];
  HttpRequest *req = NULL;
  RIO rio;

  req = HttpRequestInit();
  if (req == NULL) {
    return NULL;
  }
  RioReadInit(&rio, client_fd);
  RioReadLine(&rio, buf, MAXLINE);
  sscanf(buf, "%s %s", method, uri);
  /* Read all */
  while (buf[0] != '\n' && buf[1] != '\n') { /* \n || \r\n */
    RioReadLine(&rio, buf, MAXLINE);
    if (buf[0] == 'R' && buf[1] == 'a' && buf[2] == 'n') {
      sscanf(buf, "Range: bytes=%lu-%lu", &req->offset, &req->end);
      /* Range: [start, end] */
      if (req->end != 0) {
        req->end++;
      }
    }
  }
  file_name = uri;
  /* Get file name */
  if (uri[0] == '/') {
    file_name = uri + 1;
    length = strlen(file_name);
    if (length == 0) {
      file_name = ".";
    } else {
      i = 0;
      for (; i < length; ++i) {
        if (file_name[i] == '?') {
          file_name[i] = '\0';
          break;
        }
      }
    }
  }
  strncpy(req->file_name, file_name, MAXLINE);
  return req;
}

void SendErrorToClient(int client_fd, int status, char *msg, char *longmsg) {
  char buf[MAXLINE];

  sprintf(buf, "HTTP/1.1 %d %s\r\n", status, msg);
  sprintf(buf + strlen(buf), "Content-length: %lu\r\n\r\n", strlen(longmsg));
  sprintf(buf + strlen(buf), "%s", longmsg);
  WriteNum(client_fd, buf, strlen(buf));
}

void SendFileToClient(int client_fd, int file_fd, HttpRequest *req,
                      size_t total_size) {
  char buf[256];
  off_t offset = 0;

  /* Sends partial file to client */
  if (req->offset > 0) {
    sprintf(buf, "HTTP/1.1 206 Partial\r\n");
    sprintf(buf + strlen(buf), "Content-Range: bytes %lu-%lu/%lu\r\n",
            req->offset, req->end, total_size);
  } else { /* Sends whole file to client */
    sprintf(buf, "HTTP/1.1 200 OK\r\nAccept-Ranges: bytes\r\n");
  }
  sprintf(buf + strlen(buf), "Cache-Control: no-cache\r\n");
  sprintf(buf + strlen(buf), "Content-length: %lu\r\n", req->end - req->offset);
  sprintf(buf + strlen(buf), "Content-type: text/plain\r\n\r\n");
  WriteNum(client_fd, buf, strlen(buf));
  offset = req->offset; 
  while (offset < req->end) {
    /* Sends data which start from offset of file to client */
    if (sendfile(client_fd, file_fd, &offset, req->end - req->offset) <= 0) {
      break;
    }
    close(client_fd);
    break;
  }
}

void SendDirectoryToClient(int client_fd, int dir_fd) {
  char buf[MAXLINE], m_time[32], size[16];
  char *d = NULL;
  int file_fd = -1;
  DIR *dir = NULL;
  struct stat stat_buf;
  struct dirent *dp = NULL;

  /* Send messages to client */
  sprintf(buf, "HTTP/1.1 200 OK\r\n%s%s%s%s%s",
          "Content-Type: text/html\r\n\r\n", "<html><head><style>",
          "body{font-family: monospace; font-size: 13px;}",
          "td {padding: 1.5px 6px;}", "</style></head><body><table>\n");
  WriteNum(client_fd, buf, strlen(buf));
  /* Open a directory.
   * On success, return a pointer to the directory stream
   * On error, return NULL */
  dir = fdopendir(dir_fd);
  /* Read a directory
   * On success, return a pointer to dirent structure
   * On error, return NULL */
  while ((dp = readdir(dir)) != NULL) {
    if (!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, "..")) {
      continue;
    }
    /* Open and possibly create a file
     * On success, return a file descriptor
     * On error, return -1 */
    if ((file_fd = openat(dir_fd, dp->d_name, O_RDONLY)) == -1) {
      perror(dp->d_name);
      continue;
    }
    /* Get file status
     * On success, return 0
     * On error, return -1  */
    fstat(file_fd, &stat_buf);
    /* Format date and time 
     * size_t strftime(char *restrict s, size_t max,
                       const char *restrict format,
                       const struct tm *restrict tm);
     * The strftime() function formats the broken-down time tm according
       to the format specification format and places the result in the
       character array s of size max. */
    /* The localtime() function shall convert the time in seconds */
    /* st_mtime is the time of last modification of file data. */
    strftime(m_time, sizeof(m_time), "%Y-%m-%d %H:%M",
             localtime(&stat_buf.st_mtime));
    FormatSize(size, &stat_buf);
    /* File or Directory */
    if (S_ISREG(stat_buf.st_mode) || S_ISDIR(stat_buf.st_mode)) {
      d = S_ISDIR(stat_buf.st_mode) ? "/" : "";
      sprintf(
          buf,
          "<tr><td><a href=\"%s%s\">%s%s</a></td><td>%s</td><td>%s</td></tr>\n",
          dp->d_name, d, dp->d_name, d, m_time, size);
      WriteNum(client_fd, buf, strlen(buf));
    }
    close(file_fd);
  }
  sprintf(buf, "</table></body></html>");
  WriteNum(client_fd, buf, strlen(buf));
  closedir(dir);
}

void Process(int client_fd) {
  int status = 200;
  int file_fd = -1;
  char *msg = NULL;
  struct stat stat_buf;
  HttpRequest *req = ParseRequest(client_fd);
  
  file_fd = open(req->file_name, O_RDONLY, 0);
  if (file_fd <= 0) {
    status = 404;
    msg = "File not found";
    SendErrorToClient(client_fd, status, "Not found", msg);
  } else {
    fstat(file_fd, &stat_buf);
    if (S_ISREG(stat_buf.st_mode)) {
      if (req->end == 0) {
        req->end = stat_buf.st_size;
      }
      if (req->offset > 0) {
        status = 206;
      }
      SendFileToClient(client_fd, file_fd, req, stat_buf.st_size);
    } else if (S_ISDIR(stat_buf.st_mode)) {
      status = 200;
      SendDirectoryToClient(client_fd, file_fd);
    } else {
      status = 400;
      msg = "Unknow Error";
      SendErrorToClient(client_fd, status, "Error", msg);
    }
    close(file_fd);
  }
}

bool RunServer(int argc, char **argv) {
  char c = 'h';
  int client_fd = -1;
  int server_fd = -1;
  char *dir = NULL;
  char *new_dir = NULL;
  size_t dir_max_len = 1024;
  size_t dir_len = 0;
  size_t port = 9999;
  struct sockaddr_in client_addr;
  socklen_t client_len = sizeof client_addr;

  dir = malloc((dir_max_len + 1) * sizeof(char));
  if (!IsMemAlloc(dir)) {
    return false;
  }
  memset(dir, 0, (dir_max_len + 1) * sizeof(char));
  /* Get current working directory */
  if (!getcwd(dir, dir_max_len)) {
    return false;
  }

  while ((c = getopt(argc, argv, "hd:p:s")) != -1) {
    switch (c) {
    case 'h': /* Usage */
      PrintUsage();
      return true;
      break;
    case 'd': /* Set working directory */
      dir_len = strlen(optarg);
      /* Increase size of directory  */
      if (dir_len > dir_max_len) {
        new_dir = realloc(dir, (dir_len + 1) * sizeof(char));
        if (!IsMemAlloc(new_dir)) {
          return false;
        }
	memset(new_dir, 0, (dir_len + 1) * sizeof(char));
        dir = new_dir;
      }
      strncpy(dir, optarg, dir_len);
      dir[dir_len] = '\0';
      /* Changes working directroy 
       * On success, reutrns 0
       * On error , returns -1*/
      if (chdir(dir) == -1) {
        return false;
      }
      break;
    case 's': /* Shut down server */
      return true;
    case 'p': /* Set port */
      port = atoi(optarg);
      if (port < 0 || port > 65535) {
        ShowMsg("range of port is 0~65535\n");
        exit(0);
      } else if (port == 0 && optarg[0] != '0') {
        ShowMsg("%s is not in the range 0~65535\n", optarg);
      }
      break;
    default:
      ShowMsg("unknown option:%c", c);
      break;
    }
  }
  server_fd = ServerSocketToListen(port);
  if (server_fd < 0) {
    exit(server_fd);
  }

  /* Ignore SIGPIPE signal, so if browser cancels the request, it
   * won't kill the whole process. */
  signal(SIGPIPE, SIG_IGN);
  while (true) {
    /* Accept a connection
     * On success, return file descriptor of client
     * On error, return -1 */
    client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
    Process(client_fd);
    close(client_fd);
  }
  return true;
}
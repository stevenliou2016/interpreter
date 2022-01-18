#include "interpreter_client.h"
#include "../interpreter_mem.h"
#include "../interpreter_msg.h"
#include "../interpreter_rio.h"
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

size_t g_buf_max_size = 1024;

/* Makes size of list double
 * On success, return a pointer of pointer to list
 * On error, return NULL */
static char **DoubleSize(char **list) {
  int list_len = sizeof(list) / sizeof(char *);
  char **new_list = realloc(list, (2 * list_len + 1) * sizeof(char *));
  if (IsMemAlloc(new_list)) {
    return NULL;
  }
  /* Initializes new spaces */
  memset(list + list_len, 0, (list_len + 1) * sizeof(char *));

  return new_list;
}

static void Usage(char *program_name) {
  printf("\nUsage: %s [options] [args]\n", program_name);
  printf("\tclient -h		#usage\n");
  printf("\tclient -c serverIP	#connect to server(IPv4)\n");
  printf("\tclient -f fileName	#download file\n");
  printf("\tclient -p port\t	#port range 0~65353\n");
  printf("\tclient -d directory	#download directory\n");
}

/* Connects to server
 * On success, return a file descriptor of client socket
 * On error, return -1
 * The returned file descriptor needs to be closed by caller */
static int ConnectToServer(const char *ip, size_t port) {
  int client_fd = -1;
  int ret = 0;
  struct sockaddr_in server_addr;

  memset(&server_addr, 0, sizeof(server_addr));
  /* Establishes client socket */
  if ((client_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    return -1;
  }
  /* Sets version of internet protocol which is IPv4 */
  server_addr.sin_family = AF_INET;
  /* Converts the unsigned short integer port
     from host byte order to network byte order */
  server_addr.sin_port = htons(port);
  /* int inet_pton(int af, const char *src, void *dst)
   * Converts address of af(address format) from src to dst
   * AF_INET is IPv4
   * Returns 1 on success
   * Returns 0 if second parameter is not a valid network address
   * Returns -1 if first parameter is not a valid address family */
  ret = inet_pton(AF_INET, ip, &server_addr.sin_addr);
  if (ret < 0) {
    ShowMsg("it is not a valid address family\n");
    close(client_fd);
    return -1;
  } else if (ret == 0) {
    ShowMsg("it is not a valid network address\n");
    close(client_fd);
    return -1;
  }
  if (connect(client_fd, (const struct sockaddr *)&server_addr,
              sizeof(struct sockaddr_in)) == -1) {
    ShowMsg("connection failed\n");
    close(client_fd);
    return -1;
  }
  return client_fd;
}

/* Sends a request to get a file */
static void GetFile(const char *file_name, int client_fd) {
  size_t msg_len = 20;
  size_t buf_len = 0;
  char *buf = NULL;

  if(!file_name || client_fd == -1){
    return;
  }
  buf_len = msg_len + strlen(file_name);
  buf = malloc((buf_len + 1) * sizeof(char));
  if (!IsMemAlloc(buf)) {
    ShowMsg("sending request failed\n");
  }
  memset(buf, 0, buf_len * sizeof(char));

  sprintf(buf, "GET %s HTTP/1.1\r\n\r\n", file_name);
  WriteNum(client_fd, buf, strlen(buf));
  free(buf);
}

/* Returns true if path is a directory */
static bool IsDir(char *path) {
  char *path_ptr = NULL;

  if(!path){
    return NULL;
  }
  path_ptr = path;
  while (*path_ptr != '\0') {
    path_ptr++;
  }
  if (*(--path_ptr) == '/') {
    return true;
  }
  return false;
}

/* Sends a request to get a directory */
static void GetDir(char *dir_name, int client_fd) {
  size_t msg_len = 20;
  size_t buf_len = 0;
  char *buf = NULL;

  if(!dir_name || client_fd == -1){
    return;
  }
  buf_len = msg_len + strlen(dir_name);
  buf = malloc(buf_len * sizeof(char));
  if (!IsMemAlloc(buf)) {
    ShowMsg("send request failed\n");
  }
  memset(buf, 0, buf_len * sizeof(char));

  if (!IsDir(dir_name)) {
    strcat(dir_name, "/");
  }

  sprintf(buf, "GET %s HTTP/1.1\r\n\r\n", dir_name);
  WriteNum(client_fd, buf, strlen(buf));
  free(buf);
}

/* Returns true if it downloads file successfully */
static bool DownloadFile(const char *file_name, const char *ip, size_t port) {
  char *server_buf = NULL;
  bool is_writing = false;
  int read_num = 1;
  int client_fd = -1;
  FILE *file_ptr = NULL;
  RIO rio;

  if(!file_name || !ip){
    return false;
  }
  server_buf = malloc((g_buf_max_size + 1) * sizeof(char));
  if (!IsMemAlloc(server_buf)) {
    return false;
  }
  memset(server_buf, 0, (g_buf_max_size + 1) * sizeof(char));
  if ((client_fd = ConnectToServer(ip, port)) == -1) {
    free(server_buf);
    return false;
  }
  GetFile(file_name, client_fd);
  RioReadInit(&rio, client_fd);
  file_ptr = fopen(file_name, "a");
  if (file_ptr == NULL) {
    ShowMsg("opening file failed\n");
    free(server_buf);
    return false;
  }
  while (read_num > 0) {
    read_num = RioReadLine(&rio, server_buf, MAXLINE);
    /* Skips file state */
    if (strncmp(server_buf, "Content-type", 12) == 0) {
      RioReadLine(&rio, server_buf, MAXLINE);
      is_writing = true;
      continue;
    }
    /* Writes contents of file in file_ptr */
    if (is_writing) {
      fwrite(server_buf, 1, read_num, file_ptr);
    }
  }
  fclose(file_ptr);
  close(client_fd);
  free(server_buf);

  return true;
}

/* Builds directory if it does not exist
 * On success, returns true */
static bool BuildDir(const char *dir_name) {
  if(!dir_name){
    return false;
  }
  /* Checks existence of file */
  if (access(dir_name, F_OK) != 0) {
    /* Builds a directory dir_name
     * Only owner can read or write or execute */
    if (mkdir(dir_name, S_IRWXU) == -1) {
      ShowMsg("makeing a directory failed\n");
      return false;
    }
  }
  return true;
}

/* Gets name from request
 * On success, returns a pointer to a file name
 * On error, returns NULL */
static char *GetName(char *buf) {
  char *buf_ptr = NULL;

  if(!buf){
    return NULL;
  }

  if (strncmp(buf, "<tr><td><a href=", 16) == 0) {
    /* Skips "<tr><td><a href=" */
    buf += 17;
    buf_ptr = buf;
    while (*buf_ptr != '"') {
      buf_ptr++;
    }
    if ((buf_ptr - buf) <= g_buf_max_size) {
      *buf_ptr = '\0';
    } else {
      ShowMsg("file name is too long\n");
      return NULL;
    }
    return buf;
  }

  return NULL;
}

/* Concatenates dir_name with file_name
 * On success, return a pointer to a concatenated string
 * On error, return NULL
 * The returned pointer needs to be freed by caller */
static char *CompletePath(const char *dir_name, char *file_name) {
  size_t path_len = 0;
  char *path = NULL;

  if(!dir_name || !file_name){
    return NULL;
  }
  path_len = strlen(dir_name) + strlen(file_name);

  path = malloc((path_len + 1) * sizeof(char));
  if (!IsMemAlloc(path)) {
    return NULL;
  }
  memset(path, 0, (path_len + 1) * sizeof(char));

  strcat(path, dir_name);
  strcat(path, file_name);
  path[path_len] = '\0';

  return path;
}

static void FreeList(char **list) {
  char **list_ptr = list;

  while (list_ptr && *list_ptr) {
    free(*list_ptr);
    list_ptr++;
  }
  if (list_ptr) {
    free(list);
  }
}

/* Adds a file/directory to a file/directory list
 * On success, returns true */
static bool AddNameToList(char **list, size_t list_idx, char *name) {
  size_t name_len = 0;

  if (!list || !name || *name == '\0') {
    return false;
  }
  name_len = strlen(name);

  list[list_idx] = malloc((name_len + 1) * sizeof(char));
  if (!IsMemAlloc(list[list_idx])) {
    return false;
  }
  memset(list[list_idx], 0, (name_len + 1) * sizeof(char));
  strncpy(list[list_idx], name, name_len);
  list[list_idx][name_len] = '\0';

  return true;
}

/* Returns true if it download directory successfully */
static bool DownloadDir(char *dir_name, const char *ip, size_t port) {
  int read_num = 1;
  int client_fd = -1;
  size_t file_list_len = 100;
  size_t dir_list_len = 100;
  size_t file_list_idx = 0;
  size_t dir_list_idx = 0;
  char *server_buf = NULL;
  char *name = NULL;
  char **file_list = NULL;
  char **file_list_ptr = NULL;
  char **dir_list = NULL;
  char **dir_list_ptr = NULL;
  char **new_list = NULL;
  RIO rio;

  if(!dir_name || !ip){
    return false;
  }

  if (!BuildDir(dir_name)) {
    return false;
  }

  server_buf = malloc((g_buf_max_size + 1) * sizeof(char));
  if (!IsMemAlloc(server_buf)) {
    return false;
  }
  memset(server_buf, 0, (g_buf_max_size + 1) * sizeof(char));

  file_list = malloc((file_list_len + 1) * sizeof(char *));
  if (!IsMemAlloc(file_list)) {
    free(server_buf);
    return false;
  }
  memset(file_list, 0, (file_list_len + 1) * sizeof(char *));

  dir_list = malloc((dir_list_len + 1) * sizeof(char *));
  if (!IsMemAlloc(dir_list)) {
    free(server_buf);
    free(file_list);
    return false;
  }
  memset(dir_list, 0, (dir_list_len + 1) * sizeof(char *));

  if ((client_fd = ConnectToServer(ip, port)) == -1) {
    free(server_buf);
    free(file_list);
    free(dir_list);
    return false;
  }

  GetDir(dir_name, client_fd);

  RioReadInit(&rio, client_fd);
  while (read_num > 0) {
    memset(server_buf, 0, (g_buf_max_size + 1) * sizeof(char));
    read_num = RioReadLine(&rio, server_buf, MAXLINE);
    if ((name = GetName(server_buf)) == NULL) {
      continue;
    }
    if (IsDir(name)) {
      if (dir_list_idx > dir_list_len) {
        if ((new_list = DoubleSize(dir_list)) == NULL) {
	  free(server_buf);
          free(file_list);
          free(dir_list);
          return false;
        }
        dir_list_len *= 2;
        dir_list = new_list;
      }
      if (AddNameToList(dir_list, dir_list_idx, name) == false) {
	free(server_buf);
        free(file_list);
        free(dir_list);
        return false;
      }
      dir_list_idx++;
    } else {
      if ((name = CompletePath(dir_name, name)) == NULL) {
	free(server_buf);
        free(file_list);
        free(dir_list);
        return false;
      }
      if (file_list_idx > file_list_len) {
        if ((new_list = DoubleSize(file_list)) == NULL) {
          free(server_buf);
          free(file_list);
          free(dir_list);
	  if(name){
            free(name);
	  }
          return false;
        }
        file_list_len *= 2;
        file_list = new_list;
      }
      if (AddNameToList(file_list, file_list_idx, name) == false) {
        free(server_buf);
        free(file_list);
        free(dir_list);
        if(name){
          free(name);
	}
        return false;
      }
      file_list_idx++;
      if (name) {
        free(name);
      }
    }
  }
  close(client_fd);
  file_list_ptr = file_list;
  while (file_list_ptr && *file_list_ptr) {
    DownloadFile(*file_list_ptr, ip, port);
    file_list_ptr++;
  }
  dir_list_ptr = dir_list;
  while (dir_list_ptr && *dir_list_ptr) {
    DownloadDir(*dir_list_ptr, ip, port);
    dir_list_ptr++;
  }
  free(server_buf);
  FreeList(file_list);
  FreeList(dir_list);

  return true;
}

bool RunClient(int argc, char **argv) {
  int ch = 'h';
  int port = 9999;
  size_t max_port = 65353;
  size_t file_name_len = 0;
  size_t dir_name_len = 0;
  size_t server_ip_max_len = 15;
  size_t server_ip_len = 0;
  char *file_name = NULL;
  char *dir_name = NULL;
  char *server_ip = NULL;
  const char default_server_ip[] = "140.118.155.192";

  optind = 0; /* Initialize getopt() */
  while ((ch = getopt(argc, argv, "hf:c:p:d:")) != -1) {
    switch (ch) {
    case 'h': /* Usage */
      Usage(argv[0]);
      if(file_name){
        free(file_name);
      }
      if(dir_name){
        free(dir_name);
      }
      if(server_ip){
        free(server_ip);
      }
      return true;
    case 'f': /* Sets which file you wanna download */
      file_name_len = strlen(argv[optind - 1]);
      file_name = malloc((file_name_len + 1) * sizeof(char));
      if (!IsMemAlloc(file_name)) {
        if(dir_name){
          free(dir_name);
        }
        if(server_ip){
          free(server_ip);
        }
        return false;
      }
      memset(file_name, 0, (file_name_len + 1) * sizeof(char));
      strncpy(file_name, argv[optind - 1], file_name_len);
      file_name[file_name_len] = '\0';
      break;
    case 'c': /* Sets what IP you wanna connect */
      server_ip_len = strlen(argv[optind - 1]);
      if (server_ip_len > server_ip_max_len && server_ip_len < 7) {
        ShowMsg("It is a invalid IPv4 address\n");
        if(file_name){
          free(file_name);
        }
        if(dir_name){
          free(dir_name);
        }
        return false;
      }
      server_ip = malloc((server_ip_max_len + 1) * sizeof(char));
      if (!IsMemAlloc(server_ip)) {
        if(file_name){
          free(file_name);
        }
        if(dir_name){
          free(dir_name);
        }
        return false;
      }
      memset(server_ip, 0, (server_ip_max_len + 1) * sizeof(char));
      strncpy(server_ip, argv[optind - 1], server_ip_len);
      server_ip[server_ip_len] = '\0';
      break;
    case 'p': /* Sets what port you wanna connect */
      port = atoi(argv[optind - 1]);
      if (port > max_port || port < 0) {
        ShowMsg("It is not a valid port\n");
        if(file_name){
          free(file_name);
        }
        if(dir_name){
          free(dir_name);
        }
        if(server_ip){
          free(server_ip);
        }
        return false;
      }
      break;
    case 'd': /* Sets which directory you wanna download */
      dir_name_len = strlen(argv[optind - 1]);
      dir_name = malloc((dir_name_len + 1) * sizeof(char));
      if (!IsMemAlloc(dir_name)) {
        if(file_name){
          free(file_name);
        }
        if(server_ip){
          free(server_ip);
        }
        return false;
      }
      memset(dir_name, 0, (dir_name_len + 1) * sizeof(char));
      strncpy(dir_name, argv[optind - 1], dir_name_len);
      dir_name[dir_name_len] = '\0';
      break;
    default:
      ShowMsg("Unknown option %c\n", ch);
      if(file_name){
        free(file_name);
      }
      if(dir_name){
        free(dir_name);
      }
      if(server_ip){
        free(server_ip);
      }
      return false;
    }
  }

  if (file_name) {
    if (server_ip) {
      if (!DownloadFile(file_name, server_ip, port)) {
        ShowMsg("download %s failed\n", file_name);
        if(file_name){
          free(file_name);
        }
        if(dir_name){
          free(dir_name);
        }
        if(server_ip){
          free(server_ip);
        }
        return false;
      }
    } else {
      if (!DownloadFile(file_name, default_server_ip, port)) {
        ShowMsg("download %s failed\n", file_name);
        if(file_name){
          free(file_name);
        }
        if(dir_name){
          free(dir_name);
        }
        return false;
      }
    }
    ShowMsg("download %s sucessfully\n", file_name);
  }
  if (dir_name) {
    if (server_ip) {
      if (!DownloadDir(dir_name, server_ip, port)) {
        ShowMsg("download directory %s failed\n", dir_name);
        if(file_name){
          free(file_name);
        }
        if(dir_name){
          free(dir_name);
        }
        if(server_ip){
          free(server_ip);
        }
        return false;
      }
    } else {
      if (!DownloadDir(dir_name, default_server_ip, port)) {
        ShowMsg("download directory %s failed\n", dir_name);
        if(file_name){
          free(file_name);
        }
        if(dir_name){
          free(dir_name);
        }
        return false;
      }
    }
    ShowMsg("download directory %s sucessfully\n", dir_name);
  }
  if (file_name) {
    free(file_name);
  }
  if (dir_name) {
    free(dir_name);
  }
  if (server_ip) {
    free(server_ip);
  }

  return true;
}

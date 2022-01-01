#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/sendfile.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <dirent.h>
#include <errno.h>
#include "mem_manage.h"
#include "rio.h"
#include "server.h"

static void print_help(){
    printf("\tCommand\t\tDescription\n");
    printf("\tserver\t\t#Use default port(9999), serve current directory\n");
    printf("\tserver -h\t#Usage\n");
    printf("\tserver -s \tInactivate server\n");
    printf("\tserver -d dir\t#Use default port(9999), serve given directory\n");
    printf("\tserver -p port\t#Use given port, serve current directory\n");
}

int open_listen_fd(size_t port){
    int fd = 0;
    int optval = 1;
    struct sockaddr_in serveraddr;
  
    /* Create a socket */
    if((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        return -1;
    }

    /* Eliminates "Address already in use" error from bind. */
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const void *) &optval, sizeof(int)) < 0) {
        return -1;
    }

    // 6 is TCP's protocol number
    // enable this, much faster : 4000 req/s -> 17000 req/s
    if (setsockopt(fd, 6, TCP_CORK, (const void *) &optval, sizeof(int)) <
        0) {
        return -1;
    }

    memset(&serveraddr, 0, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons(port);
    if (bind(fd, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0) {
        return -1;
    }

    /* Make it a listening socket ready to accept connection requests; 1024 is the number of incoming requests */
    if (listen(fd, 1024) < 0) {
        return -1;
    }
    return fd;

}

void format_size(char *buf, struct stat *stat)
{
    if (S_ISDIR(stat->st_mode)) {
        sprintf(buf, "%s", "[DIR]");
    } else {
        off_t size = stat->st_size;
        if (size < 1024) {
            sprintf(buf, "%lu", size);
        } else if (size < 1024 * 1024) {
            sprintf(buf, "%.1fK", (double) size / 1024);
        } else if (size < 1024 * 1024 * 1024) {
            sprintf(buf, "%.1fM", (double) size / 1024 / 1024);
        } else {
            sprintf(buf, "%.1fG", (double) size / 1024 / 1024 / 1024);
        }
    }
}

http_request * http_request_init(){
    http_request *req = malloc(sizeof(http_request));
    if(!mem_alloc_succ(req)){
        return NULL;
    }
    size_t file_name_size = 1024;
    req->file_name = malloc(file_name_size);
    if(!mem_alloc_succ(req->file_name)){
        return NULL;
    }
    memset(req->file_name, 0, file_name_size);
    req->offset = 0;
    req->end = 0;
    
    return req;
}

http_request *parse_request(int fd){
    http_request *req = http_request_init();
    web_rio_t rio;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE];

    if(req == NULL){
        return NULL;
    }
    rio_read_init(&rio, fd);
    rio_read_line(&rio, buf, MAXLINE);
    sscanf(buf, "%s %s", method, uri);
    /* read all */
    while (buf[0] != '\n' && buf[1] != '\n') { /* \n || \r\n */
        rio_read_line(&rio, buf, MAXLINE);
        if (buf[0] == 'R' && buf[1] == 'a' && buf[2] == 'n') {
            sscanf(buf, "Range: bytes=%lu-%lu", &req->offset, &req->end);
            // Range: [start, end]
            if (req->end != 0) {
                req->end++;
            }
        }
    }
    char *file_name = uri;
    if (uri[0] == '/') {
        file_name = uri + 1;
        int length = strlen(file_name);
        if (length == 0) {
            file_name = ".";
        } else {
            int i = 0;
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

void client_error(int fd, int status, char *msg, char *longmsg)
{
    char buf[MAXLINE];
    sprintf(buf, "HTTP/1.1 %d %s\r\n", status, msg);
    sprintf(buf + strlen(buf), "Content-length: %lu\r\n\r\n", strlen(longmsg));
    sprintf(buf + strlen(buf), "%s", longmsg);
    writen(fd, buf, strlen(buf));
}

void serve_static(int out_fd, int in_fd, http_request *req, size_t total_size)
{
    char buf[256];
    if (req->offset > 0) {
        sprintf(buf, "HTTP/1.1 206 Partial\r\n");
        sprintf(buf + strlen(buf), "Content-Range: bytes %lu-%lu/%lu\r\n",
                req->offset, req->end, total_size);
    } else {
        sprintf(buf, "HTTP/1.1 200 OK\r\nAccept-Ranges: bytes\r\n");
    }
    sprintf(buf + strlen(buf), "Cache-Control: no-cache\r\n");
    sprintf(buf + strlen(buf), "Content-length: %lu\r\n",
            req->end - req->offset);
    sprintf(buf + strlen(buf), "Content-type: text/plain\r\n\r\n");

    writen(out_fd, buf, strlen(buf));
    off_t offset = req->offset; /* copy */
    while (offset < req->end) {
        if (sendfile(out_fd, in_fd, &offset, req->end - req->offset) <= 0) {
            break;
        }
        close(out_fd);
        break;
    }
}

void handle_directory_request(int out_fd, int dir_fd, char *filename)
{
    char buf[MAXLINE], m_time[32], size[16];
    struct stat statbuf;
    sprintf(buf, "HTTP/1.1 200 OK\r\n%s%s%s%s%s",
            "Content-Type: text/html\r\n\r\n", "<html><head><style>",
            "body{font-family: monospace; font-size: 13px;}",
            "td {padding: 1.5px 6px;}", "</style></head><body><table>\n");
    writen(out_fd, buf, strlen(buf));
    DIR *d = fdopendir(dir_fd);
    struct dirent *dp;
    int ffd;
    while ((dp = readdir(d)) != NULL) {
        if (!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, "..")) {
            continue;
        }
        if ((ffd = openat(dir_fd, dp->d_name, O_RDONLY)) == -1) {
            perror(dp->d_name);
            continue;
        }
        fstat(ffd, &statbuf);
        strftime(m_time, sizeof(m_time), "%Y-%m-%d %H:%M",
                 localtime(&statbuf.st_mtime));
        format_size(size, &statbuf);
        if (S_ISREG(statbuf.st_mode) || S_ISDIR(statbuf.st_mode)) {
            char *d = S_ISDIR(statbuf.st_mode) ? "/" : "";
            sprintf(buf,
                    "<tr><td><a "
                    "href=\"%s%s\">%s%s</a></td><td>%s</td><td>%s</td></tr>\n",
                    dp->d_name, d, dp->d_name, d, m_time, size);
            writen(out_fd, buf, strlen(buf));
        }
        close(ffd);
    }
    sprintf(buf, "</table></body></html>");
    writen(out_fd, buf, strlen(buf));
    closedir(d);
}

void process(int fd, struct sockaddr_in *clientaddr){
    http_request *req = parse_request(fd);
    struct stat sbuf;
    int status = 200, ffd = open(req->file_name, O_RDONLY, 0);
    if (ffd <= 0) {
        status = 404;
        char *msg = "File not found";
        client_error(fd, status, "Not found", msg);
    } else {
        fstat(ffd, &sbuf);
        if (S_ISREG(sbuf.st_mode)) {
            if (req->end == 0) {
                req->end = sbuf.st_size;
            }
            if (req->offset > 0) {
                status = 206;
            }
            serve_static(fd, ffd, req, sbuf.st_size);
        } else if (S_ISDIR(sbuf.st_mode)) {
            status = 200;
            handle_directory_request(fd, ffd, req->file_name);
        } else {
            status = 400;
            char *msg = "Unknow Error";
            client_error(fd, status, "Error", msg);
        }
        close(ffd);
    }
}

bool server(int argc, char **argv){
    int c = 'h';
    size_t dir_max_len = 256;
    size_t len = 0;
    char *dir = malloc(dir_max_len);
    if(!mem_alloc_succ(dir)){
        return false;
    }
    memset(dir, 0, dir_max_len);
    if(!getcwd(dir, dir_max_len)){
        return false;
    }
    size_t port = 9999;
    struct sockaddr_in clientaddr;
    socklen_t clientlen = sizeof clientaddr;
    int connfd = 0;

    while((c = getopt(argc, argv, "hd:p:s")) != -1){
        switch(c){
            case 'h':
                print_help();
                return false;
		break;
	    case 'd':
		len = strlen(optarg);
                while(len >= dir_max_len){
                    dir_max_len *= 2;
                }
		if(dir_max_len != 256){
	            char *new_dir = realloc(dir, dir_max_len);
	            if(new_dir == NULL){
                        printf("memory alloction failed\n");
	    	        return false;
		    }
	            dir = new_dir;
		}
	        strncpy(dir, optarg, len);
	        dir[len] = '\0';
		if(chdir(dir) == -1){
                    return false;
		}
		break;
	    case 's':
		break;
	    case 'p':
		port = atoi(optarg);
		if(port < 0 || port > 65535){
                    printf("range of port is 0~65535\n");
	            exit(0);
		}else if(port == 0 && optarg[0] != '0'){
                    printf("%s is not in the range 0~65535\n", optarg);
		}
		break;
	    default:
		printf("unknown option:%c", c);
		break;
	}
    }
    int fd = open_listen_fd(port);
    if (fd < 0) {
        exit(fd);
    }
    
    // Ignore SIGPIPE signal, so if browser cancels the request, it
    // won't kill the whole process.
    signal(SIGPIPE, SIG_IGN);
    while(true){
        connfd = accept(fd, (struct sockaddr *) &clientaddr, &clientlen);
	process(connfd, &clientaddr);
        close(connfd);
    }
    return true;
}

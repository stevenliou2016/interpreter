#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "rio.h"

typedef struct{
    char **ele;
    int idx;
}data_array;    

unsigned int buf_max_size = 1024;

static void print_help()
{
    printf("\nWEB CLIENT HELP\n");
    printf("\tclient -h		#usage\n");
    printf("\tclient -c serverIP	#connect to server(IPv4)\n");
    printf("\tclient -f fileName	#download file\n");
    printf("\tclient -p port\t	#port range 0~65353\n");
    printf("\tclient -d directory	#download directory\n");
}

/* If memory is allocated successfully, return true
 * Otherwise return false 
 */ 
bool is_mem_suc(char *s)
{
    if (s == NULL) {
        printf("malloc failed\n");
        return false;
    }
    return true;
}

/* If the connection succeeds, return file descriptor
 * On error, return -1 
 * On success, close fd by caller 
 */
int connect_to_server(const char* ip, const char* port){
    int fd = -1;
    struct sockaddr_in server_addr;
    socklen_t server_len = sizeof server_addr;

    // Establish client socket
    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        return -1;

    memset(&server_addr, 0, sizeof(server_addr));
    // IPv4
    server_addr.sin_family = AF_INET;
    uint32_t u_port = strtoul(port, NULL, 10);
    server_addr.sin_port = htons(u_port);
    /* Convert IPv4 address from text to binary form.
     * Return 1 on success,
     * Return 0 if second parameter is not a valid network address
     * Return -1 if first parameter is not a valid address family
     */ 
    int res = inet_pton(AF_INET, ip, &server_addr.sin_addr);
    if (res < 0) {
        printf("error: first parameter is not a valid address family\n");
	close(fd);
        return -1;
    } else if (res == 0) {
        printf(
            "char string (second parameter does not contain valid ipaddress\n");
	close(fd);
        return -1;
    }
    if (connect(fd, (const struct sockaddr *) &server_addr,
                sizeof(struct sockaddr_in)) == -1) {
        printf("connect failed\n");
	close(fd);
        return -1;
    }
    return fd;
}
// Send request to get file
void get_file(const char *file_name, int fd){
    char *buf = calloc(20 + strlen(file_name), sizeof(char));

    if (!is_mem_suc(buf)) {
        printf("send request failed\n");
    }

    sprintf(buf, "GET %s HTTP/1.1\r\n\r\n", file_name);
    writen(fd, buf, strlen(buf));
    free(buf);
}

// Send request to get dir
void get_dir(char *dir_name, int fd){
    char *buf = calloc(20 + strlen(dir_name), sizeof(char));
    char *p = dir_name;

    if (!is_mem_suc(buf)) {
        printf("send request failed\n");
    }

    while(*p != '\0'){
        p++;
    }
    if(*(--p) != '/'){
        strcat(dir_name, "/");
    }
    sprintf(buf, "GET %s HTTP/1.1\r\n\r\n", dir_name);
    writen(fd, buf, strlen(buf));
    free(buf);
}

static bool download_file(const char *file_name, char *ip, char *port)
{
    web_rio_t rio;
    int n = 1; // Number of character from server
    int fd = -1; // Socket file descriptor
    FILE *p_file;
    bool is_writing = false;
    char *server_buf = calloc(buf_max_size, sizeof(char));

    if (!is_mem_suc(server_buf)) {
        return false;
    }

    if((fd = connect_to_server(ip, port)) == -1){
        return false;
    }
    get_file(file_name, fd);
    rio_read_init(&rio, fd);
    p_file = fopen(file_name, "w");
    if (p_file == NULL) {
        printf("open failure\n");
        return false;
    }
    while (n > 0) {
        n = rio_read_line(&rio, server_buf, MAXLINE);

	// Skip request part
        if (strncmp(server_buf, "Content-type", 12) == 0) {
            rio_read_line(&rio, server_buf, MAXLINE);
            is_writing = true;
            continue;
        }

        if (is_writing) {
            fwrite(server_buf, 1, n, p_file);
        }
    }
    fclose(p_file);
    close(fd);
    free(server_buf);

    return true;
}

// Build directory if it does not exist
bool build_dir(const char *dir_name){
    if (access(dir_name, F_OK) != 0) {
        if (mkdir(dir_name, S_IRWXU) == -1) {
            printf("make a directory failed\n");
            return false;
        }
    }
    return true;
}

/* Get name from request
 * On success, return true
 * On error, return false
 */ 
bool get_name(char* buf){
    char *p = buf;
    int i = 0;

    if (strncmp(buf, "<tr><td><a href=", 16) == 0) {
        p += 17;

        while (*p != '"') {
            buf[i] = *p;
            i++;
            p++;
        }

        buf[i] = '\0';

	return true;
    }
    
    return false;
}

bool is_dir(char *path){
	char *p = path;

	while(*p != '\0'){
            p++;
	}
	if(*(--p) == '/'){
          return true;
	}
	return false;
}

/* Concatenation
 * On success, return true
 * On error, return false
 */ 
bool complete_path(const char *dir_name, char *buf){
    char *p = buf;
    int len = strlen(dir_name) + strlen(buf) + 1;
    char *path = calloc(buf_max_size, sizeof(char));

    if(!is_mem_suc(path)){
        return false;
    }

    strcat(path, dir_name);

    while(*p != '\0'){
        p++;
    }

    if(*(--p) != '/'){
        strcat(path, "/");
	len++;
    }

    strcat(path, buf);
    path[len - 1] = '\0';
    strncpy(buf, path, buf_max_size);
    free(path);

    return true;
}

/* On success, return true
 * On error, return false
 */ 
bool resize(char **s, unsigned int max){
    int len = sizeof(s) / sizeof(char*);
    if(len >= max){
        char **new = realloc(s, 2 * len);

	if(new == NULL){
            printf("malloc failed\n");
            return false;
	}
	s = new;
    }
    return true;
}

/* Make and initialize a data_array 
 * On error, return NULL
 * On success, return data_array
 */ 
data_array* make_data_array(){
    data_array* temp = malloc(sizeof(data_array));
    if(temp == NULL){
        printf("malloc failed\n");
	return NULL;
    }
    temp->ele = calloc(buf_max_size, sizeof(char*));
    if(temp->ele == NULL){
        printf("malloc failed\n");
	return NULL;
    }
    temp->idx = 0;

    return temp;
}

/* Add data into data_array
 * On success, return true
 * On error, return false
 */ 
bool add_data(data_array *da, const char *data){
    da->ele[da->idx] = calloc(buf_max_size, sizeof(char));

    if(!is_mem_suc(da->ele[da->idx])){
        return false;
    }
    memcpy(da->ele[da->idx++], data, strlen(data) + 1);

    return true;
}

void free_data_array(data_array *da){
    for(int i = 0; i < da->idx; i++){
        free(da->ele[i]);
    }
    free(da->ele);
    free(da);
}

bool download_dir(char *dir_name, char *ip, char *port)
{
    if(!build_dir(dir_name)){
        return false;
    }
    web_rio_t rio;
    int n = 1; // Number of character from server
    int fd = -1; // Client file descriptor
    char *server_buf = calloc(buf_max_size, sizeof(char));
    data_array* file_array = make_data_array();
    data_array* dir_array = make_data_array();

    if (!is_mem_suc(server_buf)) {
        return false;
    }

    if((fd = connect_to_server(ip, port)) == -1){
        return false;
    }
    get_dir(dir_name, fd);
    rio_read_init(&rio, fd);

    while (n > 0) {
        memset(server_buf, 0, sizeof(server_buf));
        n = rio_read_line(&rio, server_buf, MAXLINE);
        if(!get_name(server_buf)){
            continue;
	}

        if(!complete_path(dir_name, server_buf)){
            return false;
        }

        if(is_dir(server_buf)){
            if (dir_array->idx < buf_max_size) {
		if(!add_data(dir_array, server_buf)){
                    return false;
		}
            } else {
                if(!resize(dir_array->ele, buf_max_size)){
                    return false;
                }
	    }
	} else {
            if (file_array->idx < buf_max_size) {
                if(!add_data(file_array, server_buf)){
                    return false;
                }
            } else {
                if(!resize(file_array->ele, buf_max_size)){
                    return false;
                }
            }
        }
    }
    close(fd);
    for(int i = 0; i < file_array->idx; i++){
        download_file(file_array->ele[i], ip, port);
    }
    for(int i = 0; i < dir_array->idx; i++){
        download_dir(dir_array->ele[i], ip, port);
    }
    free(server_buf);
    free_data_array(file_array);
    free_data_array(dir_array);

    return true;
}


int main(int argc, char **argv)
{
    int ch = 'h';
    unsigned int max_size = 256;

    char *file_name = calloc(max_size, sizeof(char));
    if (!is_mem_suc(file_name)) {
        return -1;
    }

    char *dir_name = calloc(max_size, sizeof(char));
    if (!is_mem_suc(dir_name)) {
        return -1;
    }

    char *server_ip = calloc(16, sizeof(char));
    if (!is_mem_suc(server_ip)) {
        return -1;
    }
    strncpy(server_ip, "140.118.155.192", 16);

    char *port = calloc(6, sizeof(char)); 
    if (!is_mem_suc(port)) {
        return -1;
    }
    strncpy(port, "9999", 5);

    while ((ch = getopt(argc, argv, "hf:c:p:d:")) != -1) {
        switch (ch) {
        case 'h':
            print_help();
            break;
        case 'f':
            if (strlen(argv[optind - 1]) > max_size) {
                max_size *= 2;
                char *f = realloc(file_name, max_size);
                if (!is_mem_suc(f)) {
                    return -1;
                }
                file_name = f;
            }
            memcpy(file_name, argv[optind - 1], strlen(argv[optind - 1]));
            break;
        case 'c':
	    if(strlen(argv[optind - 1]) > 15){
                printf("It is not a valid IPv4 address\n");
            }
            memcpy(server_ip, argv[optind - 1], strlen(argv[optind - 1]));
	    server_ip[strlen(argv[optind - 1])] = '\0';
            break;
        case 'p':
	    if(atoi(argv[optind - 1]) > 65353 || atoi(argv[optind - 1]) < 0){
                printf("It is not a valid port\n");
                return -1;
            }
            memcpy(port, argv[optind - 1], strlen(argv[optind - 1]));
	    port[strlen(argv[optind - 1])] = '\0';
	    
            break;
        case 'd':
            if (strlen(argv[optind - 1]) > max_size) {
                char *dp = realloc(dir_name, max_size);
                if (!is_mem_suc(dp)) {
                    return -1;
                }
		dir_name = dp;
            }
            memcpy(dir_name, argv[optind - 1], strlen(argv[optind - 1]));
            break;
        default:
            printf("Unknown option\n");
            break;
        }
    }

    if (file_name[0] != '\0') {
        if (!download_file(file_name, server_ip, port)) {
            printf("download %s failed\n", file_name);
            return -1;
        } else
            printf("download %s sucessfully\n", file_name);
    }
    if (dir_name[0] != '\0') {
        if (!download_dir(dir_name, server_ip, port)) {
            printf("download directory %s failed\n", dir_name);
            return -1;
        } else
            printf("download directory %s sucessfully\n", dir_name);
    }
    free(file_name);
    free(dir_name);
    free(server_ip);
    free(port);
    return 0;
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "console.h"
#include "mem_manage.h"
#include "command_line.h"
#include "queue.h"
#include "client.h"
#include "server.h"

static cmd_ptr cmd_list = NULL;
static bool help_operation(int, char **);
static bool q_new_operation(int, char **);
static bool q_free_operation(int, char **);
static bool q_insert_head_operation(int, char **);
static bool q_insert_tail_operation(int, char **);
static bool q_remove_head_operation(int, char **);
static bool q_size_operation(int, char **);
static bool q_reverse_operation(int, char **);
static bool q_sort_operation(int, char **);
static bool q_show_operation(int, char **);
static bool server_operation(int, char **);
static bool client_operation(int, char **);
static bool quit_operation(int, char **);
queue_t *q = NULL;
bool quit_flag = false;
pid_t pid = -2; // id of server process

void console_init(){
    cmd_list = NULL;
    add_cmd("help", "\t#Show documents", help_operation);
    add_cmd("new", "\t#Create a queue", q_new_operation);
    add_cmd("free", "\t#Delete a queue", q_free_operation);
    add_cmd("ih", " str \t#Insert str at head", q_insert_head_operation);
    add_cmd("it", " str \t#Insert str at tail", q_insert_tail_operation);
    add_cmd("rh", "\t#Remove the first element", q_remove_head_operation);
    add_cmd("size", "\t#Show the size of queue", q_size_operation);
    add_cmd("reverse", "\t#Reverse the queue", q_reverse_operation);
    add_cmd("sort", "\t#Sort the queue", q_sort_operation);
    add_cmd("show", "\t#Show the queue", q_show_operation);
    add_cmd("server", "\t#Activate server", server_operation);
    add_cmd("client", "\t#Activate client", client_operation);
    add_cmd("quit", "\t#Exit program", quit_operation);
}

char **parse_cmd(int *argc, char *cmd){
    if(!cmd)
        return NULL;
    size_t n_args = 10;
    size_t string_size = 256;
    const char delim = ' ';
    char **argv = calloc(n_args, sizeof(char *));
    if(argv == NULL){
        printf("malloc failed\n");
        return NULL;
    }
    char **argv_p = argv;
    *argv_p = strtok(cmd, &delim);
    (*argc)++;

    while(*argv_p++){
        *argv_p = strtok(NULL, &delim);
	if(*argv_p){
            (*argc)++;

	}
    }
    return argv;
}

static bool help_operation(int argc, char **argv){
    cmd_ptr clist = cmd_list;

    printf("\tCommand\tDescription\n");
    fflush(stdout);
    while(clist){
        printf("\t%s%s\n", clist->cmd, clist->doc);
        fflush(stdout);
	clist = clist->next;
    }
    return true;
}

static bool q_show_operation(int argc, char **argv){
    list_ele_t *head = q->head;
    if(head){
        printf("q = [%s", head->value);
        head = head->next;
    }else{
        printf("q = [");
    }
    while(head){
        printf(", %s", head->value);
	head = head->next;
    }
    printf("]\n");

    return true;
}

static bool q_new_operation(int argc, char **argv){
    q = q_new();
    if(q == NULL){
        printf("malloc failed\n");
        return false;
    }
    q_show_operation(argc, argv);
    return true;
}

static bool q_free_operation(int argc, char **argv){
    if(q == NULL){
        printf("please create a queue first\n");
	return false;
    }
    q_free(q);
    printf("q is freed\n");
    return true;
}

static bool q_insert_head_operation(int argc, char **argv){
    if(q == NULL){
        printf("please create a queue first\n");
	return false;
    }
    if(!q_insert_head(q, *(argv + 1))){
        printf("insert a string at the head of queue failed\n");
        return false;
    }
    q_show_operation(argc, argv);
    return true;
}

static bool q_insert_tail_operation(int argc, char **argv){
    if(q == NULL){
        printf("please create a queue first\n");
	return false;
    }
    if(!q_insert_tail(q, *(argv + 1))){
        printf("insert an element at the tail of queue failed\n");
        return false;
    }
    q_show_operation(argc, argv);

    return true;
}

static bool q_remove_head_operation(int argc, char **argv){
    if(q == NULL){
        printf("please create a queue first\n");
	return false;
    }
    size_t len = strlen(q->head->value);
    char *head = malloc(len + 1);
    if(!mem_alloc_succ(head)){
        return false;
    }
    memset(head, 0, len + 1);
    if(!q_remove_head(q, head, len + 1)){
        printf("remove the first element failed\n");
	return false;
    }
    q_show_operation(argc, argv);

    return true;
}

static bool q_size_operation(int argc, char **argv){
    if(q == NULL){
        printf("please create a queue first\n");
	return false;
    }
    printf("the size of queue is %d\n", q_size(q));
    return true;
}

static bool q_reverse_operation(int argc, char **argv){
    if(q == NULL){
        printf("please create a queue first\n");
	return false;
    }
    q_reverse(q);
    q_show_operation(argc, argv);
    
    return true;
}

static bool q_sort_operation(int argc, char **argv){
    if(q == NULL){
        printf("please create a queue first\n");
        return false;
    }
    q_sort(q);
    q_show_operation(argc, argv);

    return true;
}

// for signal SIGCLD
static void sig_cld(){
    pid = -2; // reset pid
}

static bool server_operation(int argc, char **argv){
    if(argc > 1 && strncmp(argv[1], "-s", 2) == 0 && pid != -2){
        kill(pid, SIGTERM);
	pid = -2;
	return true;
    }
    if(pid > 0){
        printf("server is running\n");
	return true;
    }
    signal(SIGCLD, sig_cld);
    pid = fork();
    if(pid == -1){ 
	printf("fork failed\n");
	return false;
    }else if(pid == 0){ // child
        if(!server(argc, argv)){
            pid = -2;
            exit(-1);
        }
	pid = -2;
	exit(0);
    }
    if(argc > 1 && strncmp(argv[1], "-h", 2) == 0){
        wait(NULL);
        pid = -2;
    }

    return true;
}

static bool client_operation(int argc, char **argv){
    if(!client(argc, argv)){
        return false;
    }
    return true;
}

static bool quit_operation(int argc, char **argv){
    quit_flag = true;
    /* inactivate server if it's running */
    if(pid != -2){
        kill(pid, SIGTERM);
    }
    return true;
}

bool add_cmd(char *cmd, char *doc, cmd_func op){
    int cmd_len = strlen(cmd);
    int doc_len = strlen(doc);
    cmd_ptr ele = cmd_list;
    cmd_ptr *last = &cmd_list;
    while(ele){
        last = &(ele->next);
        ele = ele->next;
    }

    cmd_ptr new_cmd = malloc(sizeof(cmd_ptr));
    if(!mem_alloc_succ(new_cmd)){
        return false;
    }

    new_cmd->cmd = malloc(cmd_len + 1);
    if(!mem_alloc_succ(new_cmd->cmd)){
        return false;
    }
    strncpy(new_cmd->cmd, cmd, cmd_len);
    new_cmd->cmd[cmd_len] = '\0';

    new_cmd->doc = malloc(doc_len + 1);
    if(!mem_alloc_succ(new_cmd->doc)){
        return false;
    }
    strncpy(new_cmd->doc, doc, doc_len);
    new_cmd->doc[doc_len] = '\0';

    new_cmd->op = malloc(sizeof(op));
    if(!mem_alloc_succ(new_cmd->op)){
        return false;
    }
    new_cmd->op = op;
    new_cmd->next = NULL;
    *last = new_cmd;

    return true;
}

bool run_console(){
    while(!quit_flag){
        char *cmd = cmd_line();
	if(cmd == NULL)
            continue;
        int arg_cnt = 0;
        char **arg_val = parse_cmd(&arg_cnt, cmd);
        size_t cmd_name_len = strlen(*arg_val);
        char *cmd_name = malloc(cmd_name_len + 1);
        if(!mem_alloc_succ(cmd_name)){
            return false;
        }
        memset(cmd_name, 0, cmd_name_len + 1);
        strncpy(cmd_name, *arg_val, cmd_name_len);
        cmd_ptr clist = cmd_list;
        while(clist && strcmp(cmd_name, clist->cmd) != 0){
            clist = clist->next;
        }
	if(clist){
            clist->op(arg_cnt, arg_val);
	}else{
            printf("unknown command:%s\n", cmd_name);
	    fflush(stdout);
	}
        free(cmd);
    }
    return true;
}

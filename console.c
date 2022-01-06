#include "console.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//#include <stdbool.h>
#include <unistd.h>
//#include <signal.h>
//#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <errno.h>
#include <ctype.h>
//#include <stdarg.h>
#include "mem_manage.h"
#include "command_line.h"
#include "queue.h"
#include "client/client.h"
#include "server.h"
#include "messages.h"

queue_t *q = NULL;
bool quit_flag = false;
pid_t pid = -2; // Id of server process
const char history_file_name[] = ".history_cmd";

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

void console_init(){
    cmd_list = NULL;
    srand(time(NULL));
    add_cmd("help", "\t#Show documents", help_operation);
    add_cmd("new", "\t#Create a queue", q_new_operation);
    add_cmd("free", "\t#Delete a queue", q_free_operation);
    add_cmd("ih", " str [n]\t#Insert n times of str at head, n>=1. Generate a string if str is RAND", q_insert_head_operation);
    add_cmd("it", " str [n]\t#Insert n times of str at tail, n>=1. Generate a string if str is RAND", q_insert_tail_operation);
    add_cmd("rh", "\t#Remove the first element", q_remove_head_operation);
    add_cmd("size", "\t#Show the size of queue", q_size_operation);
    add_cmd("reverse", "\t#Reverse the queue", q_reverse_operation);
    add_cmd("sort", "\t#Sort the queue", q_sort_operation);
    add_cmd("show", "\t#Show the queue", q_show_operation);
    add_cmd("server", "\t#Activate server", server_operation);
    add_cmd("client", "\t#Activate client", client_operation);
    add_cmd("quit", "\t#Exit program", quit_operation);
    load_history(history_file_name);
}

/* Trim leading and tailing whitespace, return a pointer to a copy string. It will be free by caller */
char *trim_whitespace(char *s){
    if(!s){
        return s;
    }
    char *end = NULL;

    /* Trim leading whitespace */
    while(isspace(*s)){
        s++;
    }

    if(*s == '\0'){
        return strdup(s);
    }

    /* Trim tailing whitespace */
    end = s + strlen(s) - 1;
    while(end > s && isspace(*end)){
        end--;
    }
    *(end + 1) = '\0';

    return strdup(s);
}

char *trim_newline(char *s){
    char *temp = s;
    if(!temp){
        return temp;
    }
    while(*temp != '\n' && *temp != '\0'){
        temp++;
    }
    *temp = '\0';

    return s;
}

char **parse_cmd(int *argc, char *cmd){
    if(!cmd)
        return NULL;
    size_t n_args = 10;
    size_t string_size = 256;
    const char delim[] = " ";
    char **argv = calloc(n_args, sizeof(char *));
    if(argv == NULL){
        show_message("memory allocation failed\n");
        return NULL;
    }
    char **argv_p = argv;
    *argv_p = strtok(cmd, delim);
    if(strncmp("server", cmd, 6) != 0 && strncmp("client", cmd, 6) != 0){
        *argv_p = trim_newline(*argv_p);
    }
    (*argc)++;

    while(*argv_p++){
        *argv_p = strtok(NULL, delim);
        if(strncmp("server", cmd, 6) != 0 && strncmp("client", cmd, 6) != 0){
            *argv_p = trim_newline(*argv_p);
        }
	if(*argv_p){
            (*argc)++;

	}
    }
    return argv;
}

static bool q_is_NULL(){
   if(!q){
       show_message("the queue is NULL\n");
       return true;
   }
   return false;
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
    if(q_is_NULL()){
        return false;
    }
    list_ele_t *head = q->head;
    if(is_visible || log_file){
        if(head){
	    show_message("q = [%s", head->value);
            head = head->next;
        }else{
	    show_message("q = [");
        }
        while(head){
	    show_message(", %s", head->value);
            head = head->next;
        }
	show_message("]\n");
    }

    return true;
}

static bool q_new_operation(int argc, char **argv){
    if(q){
        free(q);
    }
    q = q_new();
    if(!mem_alloc_succ(q)){
        return false;
    }
    q_show_operation(argc, argv);
    return true;
}

static bool q_free_operation(int argc, char **argv){
    if(q_is_NULL()){
        return false;
    }
    q_free(q);
    q = NULL;
    show_message("the queue is freed\n");
    return true;
}

char* random_string(){
    char alphabets[] = "abcdefghijklmnopqrstuvwxyz";
    size_t max_len = 10;
    size_t min_len = 5;
    size_t len = 0;

    while(len < min_len){
        len = rand() % (max_len + 1);
    }
    char *s = malloc(len + 1);
    if(!mem_alloc_succ(s)){
        return NULL;
    }
    memset(s, 0, len + 1);
    for(int i = 0; i < len; i++){
        s[i] = alphabets[rand() % 26];
    }
    s[len] = '\0';

    return s;
}

static bool q_insert_head_operation(int argc, char **argv){
    if(q_is_NULL() || argc < 2){
        return false;
    }
    int n = 1;
    if(argc > 2){
        n = atoi(argv[2]);
        if(n < 1){
	    show_message("n must be greater than 0\n");
            return false;
        }
    }
    char *s = NULL;
    bool is_random = false;
    if(strlen(*(argv + 1)) == 4 && strncmp("RAND", *(argv + 1), 4) == 0){
        is_random = true;
    }else{
        s = *(argv + 1);
    }
    for(int i = 0; i < n; i++){
        if(is_random){
            s = random_string();
	}
        if(!q_insert_head(q, s)){
	    show_message("insert a string at the head of queue failed\n");
	    if(is_random){
	        free(s);
	    }
            return false;
        }
	if(is_random){
	    free(s);
	}
    }
    q_show_operation(argc, argv);
    return true;
}

static bool q_insert_tail_operation(int argc, char **argv){
    if(q_is_NULL() || argc < 2){
        return false;
    }
    int n = 1;
    if(argc > 2){
        n = atoi(argv[2]);
        if(n < 1){
	    show_message("n must be greater than 0\n");
            return false;
        }
    }
    char *s = NULL;
    bool is_random = false;
    if(strlen(*(argv + 1)) == 4 && strncmp("RAND", *(argv + 1), 4) == 0){
        is_random = true;
    }else{
        s = *(argv + 1);
    }
    for(int i = 0; i < n; i++){
        if(is_random){
            s = random_string();
	}
        if(!q_insert_tail(q, s)){
	    show_message("insert a string at the tail of queue failed\n");
	    if(is_random){
	        free(s);
	    }
            return false;
        }
	if(is_random){
	    free(s);
	}
    }
    q_show_operation(argc, argv);

    return true;
}

static bool q_remove_head_operation(int argc, char **argv){
    if(q_is_NULL()){
        return false;
    }
    if(!q->head){
        q_show_operation(argc, argv);
	return true;
    }
    size_t len = strlen(q->head->value);
    char *head = malloc(len + 1);
    if(!mem_alloc_succ(head)){
        return false;
    }
    memset(head, 0, len + 1);
    if(!q_remove_head(q, head, len + 1)){
	show_message("remove the first element failed\n");
	return false;
    }
    q_show_operation(argc, argv);

    return true;
}

static bool q_size_operation(int argc, char **argv){
    if(q_is_NULL()){
        return false;
    }
    show_message("the size of queue is %d\n", q_size(q));
    return true;
}

static bool q_reverse_operation(int argc, char **argv){
    if(q_is_NULL()){
        return false;
    }
    q_reverse(q);
    q_show_operation(argc, argv);
    
    return true;
}

static bool q_sort_operation(int argc, char **argv){
    if(q_is_NULL()){
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
        if(pid != -2){
            kill(pid, SIGTERM);
	    pid = -2;
	    return true;
	}else{
            show_message("there is no server running\n");
	    return false;
	}
    }
    if(pid > 0){
	show_message("the server is running\n");
	return true;
    }
    signal(SIGCLD, sig_cld);
    pid = fork();
    if(pid == -1){ 
	show_message("fork failed\n");
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
    if(q){
        q_free_operation(argc, argv);
    }
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

bool run_console(char *input_file, char *l_file, bool is_v){
    FILE *fp, *lfp;
    char *cmd = NULL;
    char *trim_cmd = NULL;
    size_t len = 0;
    ssize_t nread = 0;
    is_visible = is_v;
    log_file = l_file;

    cmd_line_init(cmd_list);
    if(log_file){
        is_visible = false;
	show_message("=====start running =====\n");
	is_visible = is_v;
    }
    // check if file exists
    if(input_file && access(input_file, F_OK) == -1){
	show_message("%s does not exist\n", input_file);

        return false;
    }
    if(input_file){
        if(!(fp = fopen(input_file, "r"))){
            show_message("open file %s failed\n", input_file);

            return false;
	}
    }
    while(!quit_flag){
        cmd = NULL;
	trim_cmd = NULL;
	if(input_file){
            if((nread = getline(&cmd, &len, fp)) == -1){
                if(errno != 0 && cmd){
		    free(cmd);
		    exit(-1);
		}else{
                    exit(0);
		}
	    }else{
                char *p = cmd;
		while(isspace(*p)){
                    p++;
		}
		if(*p == '#'){
                    printf("%s", cmd);
		    continue;
		}
	    }
	}else{
	    cmd = cmd_line();
	}
        trim_cmd = trim_whitespace(cmd);
	if(trim_cmd == NULL)
            continue;
        if(input_file){
	    show_message("%s", trim_cmd);
	}
        else{
	    show_message("%s\n", trim_cmd);
            add_history_cmd(trim_cmd);
            save_history_cmd(history_file_name);
	}
            
        int arg_cnt = 0;
        char **arg_val = parse_cmd(&arg_cnt, trim_cmd);
        cmd_ptr clist = cmd_list;
        while(clist && strncmp(trim_cmd, clist->cmd, strlen(trim_cmd)) != 0){
            clist = clist->next;
        }
	if(clist){
            bool ret = clist->op(arg_cnt, arg_val);
	    if(input_file && trim_cmd == "")
                return ret;
	}else{
	    show_message("unknown command:%s\n", *arg_val);
	    fflush(stdout);
	    if(input_file)
                return false;
	}
        free(cmd);
	free(trim_cmd);
    }
    free_history();
    if(input_file){
        fclose(fp);
    }
    return true;
}

/**
 * shell
 * CS 241 - Fall 2020
 */
#include "format.h"
#include "shell.h"
#include "vector.h"
#include "sstring.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <ctype.h>

typedef struct process {
    char *command;
    pid_t pid;
} process;

/** GLOBAL: VARIABLES: **/
static char* command_file = NULL;
static char* history_file = NULL;

// if history_file is specified, each subsequent command is pushed to the vector and
// also written to the file
// if not, just push command to the vector
static vector* history_vec;

static bool history_flag = false;   // start false
static bool command_flag = false;   //

static vector* pid_vec;

//static FILE* hist_fptr = NULL;

/**
 * use to reap zombies
 * from http://voyager.deanza.edu/~perry/sigchld.html
 **/
void cleanup() {
    int status;
    pid_t kidpid;
    while ((kidpid = waitpid(-1, &status, WNOHANG)) > 0) {
        //printf("Child %d terminated\n", kidpid);
    }
}

void history_write() {
    
   // printf("hi\n");
    //printf("size: %zu\n", vector_size(history_vec));
    FILE* hist_fptr = fopen(history_file, "a+");
    
    for (size_t i = 0; i < vector_size(history_vec); i++) {
        char* command = vector_get(history_vec, i);
        char* c = strdup(command);
        strcat(c, "\n");
        
        int write_status = fputs(c, hist_fptr);
        if (write_status < 0) {
            print_history_file_error();
            exit(1);
        }
    }
    fclose(hist_fptr);

}

// return '|' if OR operator
// return '&' if AND operator
// return ';' if SEPARATOR operator
// return NULL if no separators :o xD
vector* parse_logic_ops(const char* command) {
    vector* ops = vector_create(string_copy_constructor, string_destructor, string_default_constructor);

    char* first = strdup(command);

    char* or = strchr(first, '|');
    char* and = strchr(first, '&');
    char* separator = strchr(first, ';');

    if (or) {
        char* c = or;
        c++;
        if (*c  == '|') {
            c += 2;
            vector_push_back(ops, c);       // MID
            vector_push_back(ops, "||");    // BACK
        }
        *or = 0;

    } else if (and) {
        char* c = and;
        c++;
        if (*c  == '&') {
            c += 2;
            vector_push_back(ops, c);       // MID
            vector_push_back(ops, "&&");    // BACK
        }
        *and = 0;

    } else if (separator) {
        char* c = separator;        
         c += 2;
        vector_push_back(ops, c);       // MID
        vector_push_back(ops, ";");     // BACK
        
        *separator = 0;
    }

    char* end;
    end = first + strlen(first) - 1;
    while(end > first && isspace((unsigned char)*end)) end--;
    end[1] = '\0';


   // printf("front: %s\n", first);
    vector_insert(ops, 0,first);   //   FRONT
    return ops;
}

// >>, > or <
vector* parse_redirection(const char* command) {
    
    vector* redirection_vec;
    redirection_vec = vector_create(string_copy_constructor, string_destructor, string_default_constructor);

    char* first = strdup(command);
    char* redir_op;

    // <
    if ((redir_op = strchr(first, '<'))) {
        char* c = redir_op;
        if (*(++c)  == ' ') {
            c ++;
            vector_push_back(redirection_vec, c);       // MID
            vector_push_back(redirection_vec, "<");    // BACK
        }
        *redir_op = 0;
 
    } else if ((redir_op = strchr(first, '>'))) {

        // >>
        char* c = redir_op;
        c++;

        if (*c == '>') {
            
            c++;
            if (*c  == ' ') {
                c ++;
                vector_push_back(redirection_vec, c);       // MID
                vector_push_back(redirection_vec, ">>");    // BACK
            }
            
        // >
        } else if (*c == ' ') {
            c ++;
            vector_push_back(redirection_vec, c);       // MID
            vector_push_back(redirection_vec, ">");    // BACK
        }
        *redir_op = 0;
        
    }

    char* end;
    end = first + strlen(first) - 1;
    while(end > first && isspace((unsigned char)*end)) end--;
    end[1] = '\0';

    vector_insert(redirection_vec, 0,first); 
    return redirection_vec;
}

pid_t exec_external_command(char* command, bool* store_history) {
    *store_history = true;

    //if executing a background process command
    // if (command[strlen(command) - 1] == '&') {
    //     int status;
    //     pid_t child;
    //       // Register signal handler BEFORE the child can finish
    //     signal(SIGCHLD, cleanup); // or better - sigaction
    //     child = fork();
    //     if (child == -1) { exit(EXIT_FAILURE);}

    //     if (child == 0) {
    //     // Do background stuff e.g. call exec
    //     } else { /* I'm the parent! */
    //         sleep(4); // so we can see the cleanup
    //         puts("Parent is done");
    //     }
    //     return child;
    // }

    pid_t childpid;
    vector* redir_ops = parse_redirection(command);
    vector* log_ops;

    bool redir_flag = false;
    if (vector_size(redir_ops) == 3) {
        

        redir_flag = true;
        log_ops = vector_create(string_copy_constructor, string_destructor, string_default_constructor);
        vector_push_back(log_ops, vector_get(redir_ops, 0));

        
    } else {
        log_ops = parse_logic_ops(command);
    }

    

    int status = 0;

    // start executing commands
    for (size_t i = 0; i < vector_size(log_ops); i++) {
        
        // if using logic ops we don't want to use the last one
        if (vector_size(log_ops) > 1 && i == vector_size(log_ops)-1) break;
        // if using OR|| and the first command didn't fail then BREAK
        if (status != -1 && i == 1 && strcmp(*(char**)vector_back(log_ops), "||") == 0) break;
        
        pid_t pid = fork();
        childpid = pid;

    

        if (pid < 0) {     // y i k e s
            print_fork_failed();
            exit(1);

        } else if (pid == 0) {  // CHILD:
           
            
                char* command_i = vector_get(log_ops, i);
                
                childpid = getpid();

                char* com = strdup(command_i);
                char* arg1 = NULL;
                char* arg2 = NULL;

                
                if (strchr(command_i, ' ') != NULL) { 
                    sstring* s = cstr_to_sstring(command_i);
                    vector* v = sstring_split(s, ' ');

                    com = vector_get(v, 0);
                    arg1 = vector_get(v, 1);

                    if (vector_size(v) > 2) arg2 = vector_get(v, 2);

                } 
                if (arg1 && !arg2) {    // 1 arg    
                    status = execlp(com, com, arg1, NULL);
                    //status = execvp(com, args);
                } else if (arg2 && arg2) {      // 2 args
                    status = execlp(com, com, arg1, arg2, NULL);
                }else {         // 0 args
                    if (redir_flag) {

                        int wr_arg;

                        if (strcmp(vector_get(redir_ops, 2), "<") == 0 ) {wr_arg = O_RDONLY;}
                        else if (strcmp(vector_get(redir_ops, 2), ">") == 0 ) {wr_arg= O_WRONLY|O_CREAT;}
                        else {wr_arg = O_APPEND|O_WRONLY|O_CREAT;}

                        char* fil = (char*)vector_get(redir_ops, 1);
                        int fd = open(fil, wr_arg);
                        dup2(fd, 1);
                        close(fd);
                    }
                    status = execlp(com, com, NULL);
                }

                if (status < 0) {     /* execute the command  */
                    if (vector_size(log_ops) > 1) {
                            if (strcmp(vector_get(log_ops,2), "||") == 0) {
                            //("booo\n");
                            continue;
                        } else if (strcmp(vector_get(log_ops,2), "&&") == 0) {
                            //printf("nice\n");
                            print_exec_failed(command_i);
                            //exit(1);
                            break;
                        } else if (strcmp(vector_get(log_ops,2), ";") == 0) {
                            continue;
                        }
                    }
                
                    print_exec_failed(command_i);
                    exit(1);
                }
            
        } else {        // PARENT:
            if (command[strlen(command)-1] != '&') {
            
               // printf("in here\n");
                waitpid(pid, &status, 0);
            } else {
                continue;
            }
            
            //fflush(stdout);
                //print_prompt(get_full_path(command_file), pid);
        }
    }
    
    //free(com);
    return childpid;
}


void exec_cd(char* command) {
    // cd ...
    if (strlen(command) < 4) {
        command += 2;
        print_no_directory(command);
        
    }

    char path[1000];
    getcwd(path, sizeof(path));

    command += 3;       // move pointer forward three chars
    strcat(path, "/");
    strcat(path, command);

    int dir = chdir(path);
    if (dir != 0) {
        print_no_directory(path);
        //exit(1);
    }
}

void exec_print_history() {
    for (size_t i = 0; i < vector_size(history_vec); i++) {
        print_history_line(i, vector_get(history_vec, i));
    }

}

void exec_helpers(char* command, bool t) {
    if (command[0] == 'c' && command[1] == 'd') {
        exec_cd(command);
    } else {
        exec_external_command(command, &t);
    }
    
    vector_push_back(history_vec, command);
}

void exec_nth(char* command) {
    // command = #<n>
    command++;
    
    for (size_t i = 0; i < strlen(command); i++) {
        if (!isdigit(command[i])) {
            print_invalid_index();
            return;
        }
        
    }
    int n = atoi(command);
    //printf("n: %d\n", n);
    if (n < 0 || (unsigned int)n >= vector_size(history_vec)) {
        print_invalid_index();
        return;
    }

    //only works for external commands :(
    char* com = vector_get(history_vec, n);
    //printf("com: %s\n", com);
    bool t = true;

    exec_helpers(com, t);
    

}

void exec_prefix(char* command) {
    if(strcmp(command, "!") == 0 && (vector_size(history_vec) > 0)) {
            char* com = *(char**)vector_back(history_vec);
            bool t = true;
            exec_helpers(com, t);
            return;
        }
    
    char* to_exec = NULL;
    bool t = false;
    for (size_t i = 0; i < vector_size(history_vec); i++) {

        char* com = vector_get(history_vec, i);
        if (com[0] == command[1]) {
            t = true;

            to_exec = com;
        }
    }
    if (to_exec) {
        exec_helpers(to_exec, t);
        return;
    }
    print_no_history_match();
}

void exec_kill(char* command) {
    char* c = strdup(command);

    char* pid_num = malloc(sizeof(char) * 6);
        
    while(*c != '\0') {
    
        if (isdigit(*c)) {
            strcat(pid_num, c);
            
            break;
        }
        c++;
    }
    if (pid_num[0] == '\0') {
        print_invalid_command("kill");
        return;
    }

    pid_t p = atoi(pid_num);
    for (size_t i =0; i < vector_size(pid_vec); i++) {
        
        if (*(int*)vector_get(pid_vec ,i) == p) {
            //printf("i: %zu\n")
            char* com = vector_get(history_vec, i);
            print_killed_process(p, com);
            kill(p, SIGKILL);
        }
    }
    print_no_process_found(p);
}


bool exec_internal_command(char* command, bool* store_history) {
    if (strstr(command, "kill")) {
        exec_kill(command);
        return true;
    }
    // cd
    if (command[0] == 'c' && command[1] == 'd') {
        exec_cd(command);
        return true;
    } else if (command[0] == '!') {
        //  !history
        if (strcmp(command, "!history") == 0) {
            *store_history = false;
            exec_print_history();
            return true;
        // !<prefix>
        } else {
            exec_prefix(command);
            *store_history = false;
            return true;
        }
    //  #<n>
    } else if( command[0] == '#') {
        exec_nth(command);
        *store_history = false;
        return true;
    } else {
        return false;
    }
}


void execute_command(char* command) {
    if (!command ||  strlen(command) == 0) return;
    /**
    * get the current path
    **/
    char cwd[1000]; //getcwd(char *buf, size_t size);
    getcwd(cwd, sizeof(cwd));

    /**
    * print prompts IF IN -F MODE
    **/
   pid_t pid = getpid();
    if (command_flag) {
        fflush(stdout);
        print_prompt(cwd, pid);
        print_command(command);
    }

    /**
    * execute
    **/
    bool store_history = true;
    if (!exec_internal_command(command, &store_history)) {
        pid = exec_external_command(command, &store_history);
        //pid = parse_external_command(command, &store_history);
    }
    vector_push_back(pid_vec, &pid);

    /**
     * add to history vector
     **/
    if (store_history) {
        vector_push_back(history_vec, command);
    }

    print_command_executed(pid);

    
}

int shell(int argc, char *argv[]) {

   /** FIRST: parse argv[] 
    * can be in the following formats:
    * ./shell
    * ./shell -f filename
    * ./shell -h filename
    * ./shell -h filename1 ./f filename2
    * */
   if (argc != 1 && argc != 3 && argc!= 5) {
       print_usage();
       // exit TODO:
       exit(0);
   }

    /** SECOND: read args to determine optional arguments (if any) using getopt()     **/
    int c;
    while ((c = getopt(argc, argv, "h:f:")) != -1) {
        switch (c) {
            case 'f':
                command_file = strdup(optarg);
                command_flag = true;
                break;

            case 'h':
                history_file = strdup(optarg);
                history_flag = true;
                break;

            default:    // if there are no args that's ok
                break;
        }
    }


    // now we can initialize the history vector
    history_vec = vector_create(string_copy_constructor, string_destructor, string_default_constructor); 
    pid_vec = vector_create(int_copy_constructor, int_destructor, int_default_constructor);

    if (command_flag) {

        /**
         * open the file of commands
         **/
        FILE* fptr =  fopen(command_file, "r");
        if(fptr == NULL) {
            // there was an error
            print_script_file_error();  
            exit(1);             
        }

        

        /** 
         * get number of lines
         **/
        size_t count_lines = 0;
        char chr;
        for (chr = getc(fptr); chr != EOF; chr = getc(fptr)) {
            if (chr == '\n') { // Increment count if this character is newline 
                count_lines = count_lines + 1; 
            }
        }
        fclose(fptr);       // close the file!!


        /** 
         * put all the commands from the file into myargv
         **/
        fptr = fopen(command_file, "r");    // gotta reopen it smh
        char** myargv = malloc(sizeof(char*) * count_lines); 

        char* line = malloc(sizeof(char) * 100);
        size_t len;
        ssize_t read;

        size_t line_index = 0;


        /**
         * do the actual putting
         **/
        while ((read = getline(&line, &len, fptr)) != -1) {
            if (strcmp(line, "\n") != 0) {
                char* arg = strdup(line);
                strtok(arg, "\n");       // strip newline
                strcat(arg, "\0");
                myargv[line_index] = arg;
                line_index++;
             //   printf("%s", arg);
            }

        }
        fclose(fptr);

        free(line);



        /**
         * now start execing stuff
         **/ 
        for (size_t i = 0; i < count_lines; i++) {
            execute_command(myargv[i]);    
        }

        


    /** if executing commands from STDIN: **/
    } else {
        pid_t pid = getpid();
        //vector_push_back(pid_vec, &pid);

        char path[1000];
        char str[1000];
        char *p;

        while (true) {
            
            if (getcwd(path, sizeof(path)) != NULL) {
                print_prompt(path, pid);
            }

            if (*str == EOF) {
                break;
            }

            if (fgets(str, 1000, stdin) == NULL) {    // if EOF is encountered and no characters have been read
                //printf("^D\n");           //      - this happens when ctrl-D is pressed
                break;
            }

            if ((p = strchr(str, '\n')) != NULL) {
                *p = '\0';
                execute_command(str);
            }
        }
        cleanup();
    }
   //printf("flag\n");
    if (history_flag) {
        history_write();
    }
    return 0;
}
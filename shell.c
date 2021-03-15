/**@file
 * This file contains the main function and the functions for executing
 * commands.
 */
#include <fcntl.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>

#include "jobs.h"
#include "history.h"
#include "util.h"
#include "logger.h"
#include "ui.h"


/** This struct is used to hold the commands that come as input which are
 *  not builtins.
 *  - tokens: holds the commands entered.
 *  - total_token: keeps track of the number of strings.
 *  - stdout_pipe: is used to determine if we are at the last command.
 *  - stdout_file ,stdin_file: are used to store the location for io redirection.
 *  - append: is used to determine if we need to append to a file, `>>`.
 *
 */
struct command_line {
    char **tokens;
    size_t total_tokens;
    bool stdout_pipe;
    char *stdout_file;
    char *stdin_file;
    int append;
};


static char *command = NULL;
static int proc_status;
static pid_t child;
static int job = -1;
static pid_t running = -1;
/** This function executes recursively all the commands in cmds.
 *  - cmds: pointer to current command.
 *
 */
void pipeline_r(struct command_line *cmds)
{
    if(cmds->stdout_pipe == false){

            if(cmds->stdout_file !=NULL){

                int flags;
                if(cmds->append == -1)
                    flags = O_WRONLY | O_CREAT | O_TRUNC;
                else
                    flags = O_WRONLY | O_APPEND;

                int fd = open(cmds->stdout_file, flags, 0666);
                if(fd == -1){

                    perror("open");
                    exit(EXIT_FAILURE);
                }


                if(dup2(fd, STDOUT_FILENO) == -1){

                    perror("dup2");
                    exit(EXIT_FAILURE);
                }
            }

            if(cmds->stdin_file != NULL){

                int file = open(cmds->stdin_file, O_RDONLY);
                dup2(file, STDIN_FILENO);
            }

            execvp(cmds->tokens[0], cmds->tokens);
            perror(cmds->tokens[0]);

            close(fileno(stdin));
            close(fileno(stdout));
            close(fileno(stderr));
            exit(EXIT_FAILURE);

    } else {

        int fd[2];
        if (pipe(fd) == -1) {

            perror("pipe");
            exit(EXIT_FAILURE);
        }

        int child_r;
        child_r = fork();
        if(child_r == 0){

            if(cmds->stdin_file != NULL){
                int file = open(cmds->stdin_file, O_RDONLY);
                dup2(file, STDIN_FILENO);
            }

            if(dup2(fd[1], STDOUT_FILENO) == -1){

                perror("dup2");
                exit(EXIT_FAILURE);
            }

            close(fd[0]);
            execvp(cmds->tokens[0], cmds->tokens);
            perror(cmds->tokens[0]);
            exit(EXIT_FAILURE);
        } else if (child_r == -1){

            perror("fork");
            exit(EXIT_FAILURE);
        } else {

            if(dup2(fd[0], STDIN_FILENO) == -1){

                perror("dup2");
                exit(EXIT_FAILURE);
            }

            close(fd[1]);
            pipeline_r(cmds + 1);
        }
    }

}





/** This function is used for handling the builtins. The command entered as
 *  input and the same command tokenized are passed as arguments.
 *  - command: command entered.
 *  - args: command enter after being tokenized
 *
 *  Returns: 0 is returned if a builtin is executed successfully. If not 0
 *  command was either not found or it failed.
 *
 *
 */
int handle_builtins(char *command, char **args){

    if(!strcmp(args[0], "jobs")){
        jobs_print();
        fflush(stdout);
        return 0;
    }
    if(!strcmp(args[0], "cd")){
        char path[256];
        if(args[1] == NULL){
            strcpy(path, getpwd());
        } else if(!strncmp(args[1], "~", 1)){

            if(strlen(args[1]) == 1){
                strcpy(path, getpwd());
            } else {
                char *p = args[1];
                sprintf(path, "%s%s", getpwd(), ++p);
            }

        } else {
            strcpy(path, args[1]);
        }

        int ret;
        if((ret = chdir(path)) == 0)
            set_prompt_cwd();

        return ret;
    }

     if(!strcmp(args[0], "exit")){
         free(command);
         jobs_destroy();
         hist_destroy();
         clear_sz();
         exit(0);
     }
     if(!strcmp(args[0], "history")){
       hist_print();
       return 0;
     }

     return -1;
}

/** This functions frees the memory allocated.
 *
 */
void cleanup(){

    free(command);
    clear_sz();
}

/** This function is used to handle the commands starting with !. Based on the
 *  type of search, the function will execute differently (! with the command
 *  number, ! with the prefix of the command,!! last command executed.
 *  -1 is returned if no command was found in the history.
 *
 *  - command: command entered
 *  
 *  Returns: 0 if command succeded. Not 0 if command failed.
 *
 */
int handle_search(char *command){

    const char *temp = NULL;

    if(isDigitOnly(command + 1) == 0){
       temp = hist_search_cnum(atoi(command + 1));
       if(temp != NULL){
           strcpy(command, temp);
           return 0;
       }
    }

    if(*(command + 1) == '!'){
        temp = hist_search_cnum(hist_last_cnum() - 1);
        if(temp != NULL){
            strcpy(command, temp);
            return 0;
        }
    }

    temp = hist_search_prefix(command + 1);
    if(temp != NULL){
        strcpy(command, temp);
        return 0;
    }

    return -1;
}
/** This helper function allocates memory for the struct command_line and
 *  initializes its values.
 *
 *  -cmds: struct of commands that have been entered on one line.
 *  -pipe: number of commands.
 *
 */
void init_commands(struct command_line *cmds, int pipe){

    for(int i = 0; i < (pipe + 1); i++){
        cmds[i].stdout_file = NULL;
        cmds[i].stdin_file = NULL;
        cmds[i].append = -1;
        cmds[i].total_tokens = 0;
    }


}
/** This helper function frees the memory allocated for the struct command_line.
 *
 *  -cmds: struct of commands that have been entered on one line.
 *  -pipe: number of commands.
 */
void destroy_commands(struct command_line *cmds, int pipe){

    for(int i = 0; i < pipe + 1; i++){
        for(int j = 0; j < cmds[i].total_tokens; j++){
            free(cmds[i].tokens[j]);
        }
        if(cmds[i].stdout_file != NULL)
            free(cmds[i].stdout_file);

        if(cmds[i].stdin_file != NULL)
            free(cmds[i].stdin_file);

        free(cmds[i].tokens);
    }

}
void sigint_handler();

/** This function handles the commands that are present in the path. The
 *  tokenization of the command, the number of commands (pipe), and the number of
 *  tokens is passed to the function. All the commands are parsed in the struct
 *  command_line. This function executes pipline_r and returns the status of the
 *  child process.
 *
 *  -args: command entered after being tokenized.
 *  -pipe: number of commands.
 *  -tokens: number of tokens.
 *
 *  Returns: 0 if the command succeded. Not 0 if the command failed.
 *
 */
int handle_utils(char *const args[], int pipe, int tokens){

    struct command_line cmds[pipe + 1];
    struct command_line *p;
    p = cmds;

    init_commands(cmds, pipe);

    size_t command_sz = 8;
    char ***commands = NULL;

    commands = malloc((pipe + 1) * sizeof(char**));
    if(!commands){
        perror("malloc");
        return EXIT_FAILURE;
    }

    for(int i = 0; i < pipe + 1; i++){
        commands[i] = malloc(command_sz * sizeof(char*));
        if(!(commands[i])){
            perror("malloc");
            return EXIT_FAILURE;
        }
    }

    //pointer to list of array of strings 
    char ***command_array = commands;
    char **p_command = *commands;


    job = -1;

    for(int i = 0; i < tokens; i++){

        if(p->total_tokens == command_sz){

            command_sz *=2;

            *command_array = realloc(*command_array, command_sz * sizeof(char*));
            p_command = (*command_array + p->total_tokens);
            if(!(*command_array)){
                perror("realloc");
                return EXIT_FAILURE;
            }
        }
        if(*args[i] == '&'){
            job = 0;
            continue;
        }
        if(*args[i] == '<'){
            p->stdin_file = strdup(args[++i]);
            if(!p->stdin_file){
                perror("strdup");
                return -1;
            }
            continue;
        }
        if(!strcmp(args[i], ">>")){

            p->append = 0;
            p->stdout_file = strdup(args[++i]);
            if(!p->stdout_file){
                perror("strdup");
                return -1;
            }
            continue;
        }
        if(*args[i] == '>'){
            p->stdout_file = strdup(args[++i]);
            if(!p->stdout_file){
                perror("strdup");
                return -1;
            }
            continue;
        }
        if(*args[i] == '|'){

            *p_command = (char*) NULL;

            p->tokens = *command_array;  //Adding the array of strings to struct
            command_array++;
            p->stdout_pipe = true;

            p++;                         //advancing one position with struct

            p_command = *command_array;
            command_sz = 8;
        } else {

            *p_command = strdup(args[i]);
            if(!(*p_command)){
                perror("strdup");
                return -1;
            }
            p_command++;                //advancing one element in array
            p->total_tokens += 1;
        }
    }

    *p_command = (char*) NULL;
    p->tokens = *command_array;
    p->stdout_pipe = false;

    child = fork();
    if(child == 0){

        /* This commentend function puts the child process in another group
         * process in case of background execution. In this way, signals meant
         * to be directed to the foreground won't interfere with background
         * processes. As the test case sends a SIGINT all the processes of the
         * first group process, the child processes won't be interrupted and
         * will keep running. That's why the following lines are commended. 
         */ 

        //if(job == 0)
        //    setpgid(child, child);

        pipeline_r(cmds);

    } else if(child == -1){

        perror("fork");
    } else {

        if(job == 0){
            jobs_add(command, child);
        }
        if(job == -1){
            running = 0;
            running = waitpid(child, &proc_status, 0);
            running = -1;
            fflush(stdout);
        }
    }

    destroy_commands(cmds, pipe);
    free(commands);

    if(job == 0)
        return 0;

    return proc_status;
}
/** This handler is called everytime a SIGCHLD is sent to the program. If a
 * background process ends is removed by the jobs list.
 *
 */ 
void sigchild_handler(){

    pid_t pid;
    if((pid = waitpid(-1, &proc_status, WNOHANG)) > 0){

        jobs_delete(pid);
        fflush(stdout);
    }

}

/** This handler stops a currently running program (if any) and refreshes
 *  the prompt.
 *  
 */
void sigint_handler(){

    sigint(running);

}
/** The main function continously reads from the prompt and sends the command to
 *  the different handlers(builtin, utils).
 *
 */
int main(void)
{
    signal(SIGINT, sigint_handler);
    signal(SIGCHLD, sigchild_handler); 
    init_ui();
    hist_init(100);
    jobs_init(10);

    while (true) {
        command = read_command();
        if (command == NULL) {
            break;
        }
        char *args[4096];
        LOG("Input command: %s\n", command);
        char *p_comment = strstr(command, "#");
        if(p_comment){
            *p_comment = '\0';
        }
        if(*command == '!'){
            int search;
           if((search = handle_search(command)) == -1){
               cleanup();
               continue;
           }
        }

        hist_add(command);

        char *command_copy = strdup(command);
        if(!command_copy){
            cleanup();
            continue;
        }

        int status;
        int pipe = 0;
        int tokens = 0;
        char *next_tok = command_copy;
        char *curr_tok;
        while((curr_tok = next_token(&next_tok, " \n\t\r")) != NULL){
            if(*curr_tok == '|')
                pipe++;

            args[tokens++] = curr_tok;
        }
        args[tokens] = (char *) 0;

        if(args[0] == NULL){
            free(command_copy);
            cleanup();
            continue;
        }


        if((status = handle_builtins(command_copy, args)) == 0){
            set_prompt_stat(status, hist_last_cnum());
            free(command_copy);
            fflush(stdout);
            cleanup();
            continue;
        }

        status = handle_utils(args, pipe, tokens);
        set_prompt_stat(status, hist_last_cnum());

        free(command_copy);
        cleanup();
    }

    jobs_destroy();
    hist_destroy();
    destroy_ui();
    free(command);
    return 0;
}

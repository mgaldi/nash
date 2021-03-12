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

#include "history.h"
#include "util.h"
#include "logger.h"
#include "ui.h"


struct command_line {
    char **tokens;
    size_t total_tokens;
    bool stdout_pipe;
    char *stdout_file;
    char *stdin_file;
    int append;
};


static char *command = NULL;

void pipeline_r(struct command_line *cmds, int i)
{
   
    if(cmds[i].stdout_pipe == false){

            if(cmds[i].stdout_file !=NULL){

                int flags;
                if(cmds[i].append == -1){
                    flags = O_WRONLY | O_CREAT | O_TRUNC;
                } else {
                    flags = O_WRONLY | O_APPEND;
                    LOGP("APPENDING\n");
                }
                int fd = open(cmds[i].stdout_file, flags, 0666);
                if(fd == -1){

                    perror("open");
                    exit(EXIT_FAILURE);
                }

                if(cmds[i].stdin_file != NULL){
                    int file = open(cmds[i].stdin_file, O_RDONLY);
                    dup2(file, STDIN_FILENO);
                }

                if(dup2(fd, STDOUT_FILENO) == -1){

                    perror("dup2");
                    exit(EXIT_FAILURE);
                }
            }

            execvp(cmds[i].tokens[0], cmds[i].tokens);
            perror(cmds[i].tokens[0]);

            perror("execvp");
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
        pid_t child = fork(); 
        if(child == 0){

            if(cmds[i].stdin_file != NULL){
                int file = open(cmds[i].stdin_file, O_RDONLY);
                dup2(file, STDIN_FILENO);
            }

            if(dup2(fd[1], STDOUT_FILENO) == -1){

                perror("dup2");
                exit(EXIT_FAILURE);
            }

            close(fd[0]);
            execvp(cmds[i].tokens[0], cmds[i].tokens);
            perror(cmds[i].tokens[0]);
            exit(EXIT_FAILURE);
        } else if (child == -1){

            perror("fork");
            exit(EXIT_FAILURE);
        } else {

            if(dup2(fd[0], STDIN_FILENO) == -1){

                perror("dup2");
                exit(EXIT_FAILURE);
            }

            close(fd[1]);
            pipeline_r(cmds, ++i);
        }
    }

}

void execute_pipeline(struct command_line *cmds)
{
    pipeline_r(cmds, 0);
}
int handle_builtins(char *command, char **args){

    if(!strcmp(command, "cd")){
        char path[256];
        if(args[1] == NULL){
            strcpy(path, getenv("HOME"));
        } else if(!strncmp(args[1], "~", 1)){
            if(strlen(args[1]) == 1){
                strcpy(path, getenv("HOME"));
            } else {
                char *p = args[1];
                sprintf(path, "%s%s", getenv("HOME"), ++p);
            }
        } else {
            strcpy(path, args[1]);
        }

        int ret;
        if((ret = chdir(path)) == 0)
            set_prompt_cwd();

        return ret;
    }

     if(!strcmp(command, "exit")){
         free(command);
         hist_destroy();
         clear_sz();
         exit(0);
     }
     if(!strcmp(command, "history")){
       hist_print();
       return 0;
     }

     return -1;
}

void cleanup(){

    free(command);
    clear_sz();
}

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
void init_commands(struct command_line *cmds, int pipe){

    for(int i = 0; i < (pipe + 1); i++){
        cmds[i].stdout_file = NULL;
        cmds[i].stdin_file = NULL;
        cmds[i].append = -1;
        cmds[i].total_tokens = 0;
    }


}
void destroy_commands(struct command_line *cmds, int pipe){

    for(int i = 0; i < pipe + 1; i++){
        for(int j = 0; j < cmds[i].total_tokens; j++){
            free(cmds[i].tokens[j]);
        }
        if(cmds[i].stdout_file != NULL){
            free(cmds[i].stdout_file);
        }

        if(cmds[i].stdin_file != NULL){
            free(cmds[i].stdin_file);
        }
        free(cmds[i].tokens);
    }

}
int handle_utils(char *const args[], int pipe, int tokens){

    int status = -1;
    struct command_line cmds[pipe + 1];
    struct command_line *p;
    p = cmds;
    init_commands(cmds, pipe);

    size_t command_sz = 8;
    char ***commands = NULL;

    commands = malloc((pipe + 1) * sizeof(char**));
    if(!commands){
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    for(int i = 0; i < pipe + 1; i++){
        commands[i] = malloc(command_sz * sizeof(char*));
        if(!(commands[i])){
            perror("malloc");
            exit(EXIT_FAILURE);
        }
    }

    //pointer to list of string array
    char ***command_array = commands;
    //pointer to elements of string array
    char **p_command = *commands;


    //p->total_tokens = 8;
    for(int i = 0; i < tokens; i++){

        if(p->total_tokens == command_sz){
            command_sz *=2;
            *command_array = realloc(*command_array, sizeof(char*) * command_sz);
            if(!(*command_array)){
                perror("realloc");
                exit(EXIT_FAILURE);
            }
        }
        if(*args[i] == '<'){
            p->stdin_file = strdup(args[++i]);
            continue;
        }
        if(!strcmp(args[i], ">>")){

            p->append = 0;
            p->stdout_file = strdup(args[++i]);
            continue;
        }
        if(*args[i] == '>'){
            p->stdout_file = strdup(args[++i]);
            continue;
        }
        if(*args[i] == '|'){

            *p_command = (char*) NULL;

            p->tokens = *command_array++;  //Adding the array of strings to struct
            p->stdout_pipe = true;
            p++;                         //advancing one position with struct
            p_command = *command_array;
            command_sz = 8;
        } else {

            *p_command = strdup(args[i]);
            p_command++;                //advancing one element in array
            p->total_tokens += 1;
        }
    }
    *p_command = (char*) NULL;
    p->tokens = *command_array;
    p->stdout_pipe = false;


    pid_t child = fork();
    if( child == 0 ){

        execute_pipeline(cmds);
    } else if( child == -1){

        perror("fork");
    } else {

        waitpid(child, &status, 0);
        fflush(stdout);
    }
    destroy_commands(cmds, pipe);
    free(commands);


    return status;
}
int main(void)
{
    init_ui();
    hist_init(100);

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
        LOG("New command: %s\n", command);
        hist_add(command);

        int status;
        int pipe = 0;
        int tokens = 0;
        char *next_tok = command;
        char *curr_tok;
        while((curr_tok = next_token(&next_tok, " \n\t\r")) != NULL){
            if(*curr_tok == '|')
                pipe++;

            args[tokens++] = curr_tok;
        }
        args[tokens] = (char *) 0;
        for(int i = 0; i < tokens; i++){
            LOG("Token: %d %s\n", i, args[i]);
        }


        if((status = handle_builtins(command, args)) == 0){
            set_prompt_stat(status, hist_last_cnum());
            fflush(stdout);
            cleanup();
            continue;
        }

        status = handle_utils(args, pipe, tokens);
        set_prompt_stat(status, hist_last_cnum());

        cleanup();
    }

    hist_destroy();
    destroy_ui();
    free(command);
    return 0;
}

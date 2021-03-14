/**@file
 *
 */
#include <stdio.h>
#include <readline/readline.h>
#include <locale.h>
#include <stdbool.h>
#include <stdlib.h>

#include "dirent.h"
#include "pwd.h"
#include "history.h"
#include "util.h"
#include "logger.h"
#include "ui.h"

struct tab_completion {
    char **env_dirs;
    size_t size;
    int index;
    int builtins;
};
static int readline_init(void);
static char prompt_str[4096] = { 0 };
static char *username = NULL;
static char hostname[256];
static char full_usr_hst[288];
char *pw_dir = NULL;
char cwd[64];
static bool scripting = false;
static char *line = NULL;
static size_t line_sz = 0;
static char emoji[5];
static unsigned int c_num;
DIR *directory;
static struct tab_completion *tab_dirs;









void init_ui(void)
{
    LOGP("Initializing UI...\n");

    char *locale = setlocale(LC_ALL, "en_US.UTF-8");
    LOG("Setting locale: %s\n",
            (locale != NULL) ? locale : "could not set locale!");

    if(!isatty(STDIN_FILENO))
        scripting = true;

    username = getlogin();
    if(gethostname(hostname, 32) == -1){
        perror("gethostname");
        exit(EXIT_FAILURE);
    }

    sprintf(full_usr_hst, "[%s@%s", username, hostname);
    pw_dir = getpwd();
    set_prompt_cwd();
    set_prompt_stat(0,1);
    rl_startup_hook = readline_init;
}

void destroy_ui(){

    free(line);
}

void clear_sz(){

    line_sz = 0;
}

char *prompt_line(void) {
    set_prompt_cwd();
    sprintf(prompt_str, "[%s]-[%u]-%s:%s]$ ", emoji, c_num, full_usr_hst, cwd);
    return prompt_str;
}
void set_prompt_cwd(){

    char temp_cwd[64];

    getcwd(temp_cwd, 64);
    if((strstr(temp_cwd, pw_dir)) == temp_cwd){
        
        sprintf(cwd, "~%s", temp_cwd + strlen(pw_dir));
    } else {
    
        strcpy(cwd, temp_cwd);
    } 
}
void set_prompt_stat(int status, unsigned int cnum){

    if(status !=0 ){
        strcpy(emoji, "\xF0\x9F\x93\x89");
    } else {
        
        strcpy(emoji, "\xF0\x9F\x93\x88");
    }
    c_num = cnum;

}
char *read_command(void)
{
    if(scripting){

        size_t read_sz;
        if((read_sz = getline(&line, &line_sz, stdin)) == -1){
            perror("getline");
            return NULL;
        }

        line[read_sz - 1] = '\0';
        return line;
    } else {

        return readline(prompt_line());
    }
}

int readline_init(void)
{
    rl_bind_keyseq("\\e[A", key_up);
    rl_bind_keyseq("\\e[B", key_down);
    rl_variable_bind("show-all-if-ambiguous", "on");
    rl_variable_bind("colored-completion-prefix", "on");
    rl_attempted_completion_function = command_completion;
    return 0;
}

int key_up(int count, int key)
{
    /* Modify the command entry text: */
    rl_replace_line(hist_search_cnum(hist_last_cnum() - count), 1);

    /* Move the cursor to the end of the line: */
    rl_point = rl_end;

    // TODO: reverse history search

    return 0;
}

int key_down(int count, int key)
{
    /* Modify the command entry text: */
    rl_replace_line("User pressed 'down' key", 1);

    /* Move the cursor to the end of the line: */
    rl_point = rl_end;

    // TODO: forward history search

    return 0;
}

char **command_completion(const char *text, int start, int end)
{
    /* Tell readline that if we don't find a suitable completion, it should fall
     * back on its built-in filename completion. */
    rl_attempted_completion_over = 0;

    return rl_completion_matches(text, command_generator);
}
void clean_tabs(){

    for(int i = 0; i < tab_dirs->index; i++){
        free(tab_dirs->env_dirs[i]);
    }
    free(tab_dirs->env_dirs);
    free(tab_dirs);

}
/**
 * This function is called repeatedly by the readline library to build a list of
 * possible completions. It returns one match per function call. Once there are
 * no more completions available, it returns NULL.
 */
char *command_generator(const char *text, int state)
{
    // TODO: find potential matching completions for 'text.' If you need to
    // initialize any data structures, state will be set to '0' the first time
    // this function is called. You will likely need to maintain static/global
    // variables to track where you are in the search so that you don't start
    // over from the beginning.
  

    char builtins[4][16] = {"cd", "history", "exit", "jobs"};
    if(state == 0){
        LOG("INPUT STRING: %s\n", text);
        tab_dirs = calloc(1, sizeof(struct tab_completion)); 
        if(!tab_dirs){
            perror("calloc");
            exit(EXIT_FAILURE);
        }
        tab_dirs->builtins = 0;
        tab_dirs->index = 0;
        tab_dirs->size = 0;
        char *env_path = NULL;
        env_path = getenv("PATH");
        
        
        size_t paths_sz = 4;
        tab_dirs->env_dirs = malloc(paths_sz * sizeof(char*));
        if(*env_path != '\0'){
            LOGP("INLOOP\n");
            char *next_tok = env_path;
            char *curr_tok;
            while((curr_tok = next_token(&next_tok, ":")) != NULL){
                if(paths_sz == tab_dirs->size){
                    paths_sz *= 2;
                    tab_dirs->env_dirs = realloc(tab_dirs->env_dirs, paths_sz * sizeof(char*));
                }
                tab_dirs->env_dirs[tab_dirs->size] = strdup(curr_tok);
                tab_dirs->size += 1;
            }

            while(tab_dirs->index < tab_dirs->size){
                if((directory = opendir(tab_dirs->env_dirs[tab_dirs->index])) != NULL){
                    break;
                } else {
                    tab_dirs->index += 1;
                }
            }
        }
    }

    while(tab_dirs->index < tab_dirs->size){
        struct dirent *entry;
        while((entry = readdir(directory)) != NULL){
            char *search;
            if((search = strstr(entry->d_name, text)) != NULL){
                if(search == entry->d_name)
                    return strdup(entry->d_name);
            }
        }
        tab_dirs->index += 1;
        while(tab_dirs->index < tab_dirs->size){
            if((directory = opendir(tab_dirs->env_dirs[tab_dirs->index])) != NULL){
                break;
            } else {
                tab_dirs->index += 1;
            }
        }
    }

    while(tab_dirs->builtins < 4){
    
        int i = tab_dirs->builtins;
        tab_dirs->builtins += 1;
        LOG("builtinS INDEX: %d\n", tab_dirs->builtins);
        if(*text == '\0')
            return strdup(builtins[i]);

        char *search;
        if((search = strstr(builtins[i], text)) != NULL){
            if(search == builtins[i]) 
                return strdup(builtins[i]);
        }
    }


    clean_tabs();
    return NULL;
}

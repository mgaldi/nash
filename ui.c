/**@file
 *  This file handles the UI for the program.
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

/** This struct is used for the autocomplete. It keeps track of different
 *  information used for checking if the input matches a command or a file.
 *
 *  -env_dirs: is used to store the environment paths.
 *  -size: stores the number of different paths.
 *  -index: keeps track of the current path we are checking.
 *  -builtins: keeps track of the current builtin we are checking.
 *
 */
struct tab_completion {
    char **env_dirs;
    size_t size;
    int index;
    int builtins;
};
static int readline_init(void);
static char prompt_str[4096] = { 0 };
static char *username = NULL;
static char hostname[64];
static char full_usr_hst[320];
static char *pw_dir = NULL;
static char cwd[2048];
static bool scripting = false;
static char *line = NULL;
static size_t line_sz = 0;
static char emoji[5];
static unsigned int c_num;
static int key_search = 0;
static char *key_buffer = NULL;
static DIR *directory;
static struct tab_completion *tab_dirs;

static char builtins[4][16] = {"cd", "history", "exit", "jobs"};

/** This function initializes the ui and gathers some initial information, such
 *  as hostname, login, home directory, and if we are in a interactive shell.
 *
 */
void init_ui(void)
{
    LOGP("Initializing UI...\n");

    char *locale = setlocale(LC_ALL, "en_US.UTF-8");
    LOG("Setting locale: %s\n",
            (locale != NULL) ? locale : "could not set locale!");

    if(!isatty(STDIN_FILENO)){
        scripting = true;
    }else{
        username = getlogin();
        if(gethostname(hostname, 64) == -1){
            perror("gethostname");
            exit(EXIT_FAILURE);
        }

        sprintf(full_usr_hst, "[%s@%s", username, hostname);

        pw_dir = getpwd();
        set_prompt_cwd();
        set_prompt_stat(0,1);
    }
    rl_startup_hook = readline_init;
}

/** This function frees the memory allocated by lineread
 *
 */
void destroy_ui(){

    free(line);
}

/** This function resets the parameter for lineread to 0.
 *
 */
void clear_sz(){

    line_sz = 0;
}

/** This function creates the prompt.
 *  
 *  Returns: the string representing the prompt.
 */
char *prompt_line(void) {
    set_prompt_cwd();
    sprintf(prompt_str, "[%s]-[%u]-%s:%s]$ ", emoji, c_num, full_usr_hst, cwd);
    return prompt_str;
}

/** This function sets the current working directory in a string which is then
 *  used for the prompt
 *
 */
void set_prompt_cwd(){

    if(!scripting){
        char temp_cwd[2048];
        getcwd(temp_cwd, 2048);

        if((strstr(temp_cwd, pw_dir)) == temp_cwd)
            sprintf(cwd, "~%s", temp_cwd + strlen(pw_dir));
        else
            strcpy(cwd, temp_cwd);
    }
}

/** This function sets the prompt emoji in case of success or failure.
 *
 *  -status: represents the final status of the last command executed.
 *  -cnum: the number which represents the last command entered.
 *
 */
void set_prompt_stat(int status, unsigned int cnum){

    if(!scripting){
        if(status !=0 )
            strcpy(emoji, "\xF0\x9F\x93\x89");
        else
            strcpy(emoji, "\xF0\x9F\x93\x88");

        c_num = cnum;
        key_search = cnum;

    }
}
/** This function is used for reading the command from stdin
 *
 *  Returns: the command that was read.
 */
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

        if(key_buffer)
            free(key_buffer);
        key_buffer = NULL;
        return readline(prompt_line());
    }
}

/** This function sets different handlers for different keys and some initial
 *  settings for readline.
 *
 */
int readline_init(void)
{
    rl_bind_keyseq("\\e[A", key_up);
    rl_bind_keyseq("\\e[B", key_down);
    rl_variable_bind("show-all-if-ambiguous", "on");
    rl_variable_bind("colored-completion-prefix", "on");
    rl_attempted_completion_function = command_completion;
    return 0;
}

/** This function replaces the line with the commands executed in the history
 *  going back.
 *
 */
int key_up(int count, int key)
{

    if(key_buffer == NULL){
        LOG("LINEBUF: %s\n", rl_line_buffer);
        if(*rl_line_buffer == '\0'){
            key_buffer = "";
        } else {
            key_buffer = strdup(rl_line_buffer);
            if(!key_buffer){
                perror("strdup");
                exit(EXIT_FAILURE);
            }
        }
    }

    key_search--;
    if(key_search <= 0)
        key_search = 1;

    const char *search;
    if((search = hist_search_cnum(key_search)) == NULL){
        return 0;
    }

    rl_replace_line(search, 1);


    /* Move the cursor to the end of the line: */
    rl_point = rl_end;


    return 0;
}

/** This function replaces the line with the commands executed in the history
 *  going forward.
 *
 */
int key_down(int count, int key)
{
    if(key_buffer == NULL){
        if(*rl_line_buffer == '\0'){
            key_buffer = "";
        } else {
            key_buffer = strdup(rl_line_buffer);
            if(!key_buffer){
                perror("strdup");
                exit(EXIT_FAILURE);
            }
        }
    }

    key_search++;
    if(key_search >= c_num){
        key_search = c_num;

        rl_replace_line(key_buffer, 1);
        rl_point = rl_end;
        return 0;
    }
    const char *search;
    if((search = hist_search_cnum(key_search)) == NULL){
        return 0;
    }
    rl_replace_line(search, 1);

    /* Move the cursor to the end of the line: */
    rl_point = rl_end;


    return 0;
}

/** This function is used for command completion. The found commands are handled
 *  through command_generator().
 *
 *  -text: the string entered as input.
 *  -start: beginning of string.
 *  -end: end of string.
 *
 *  Returns: an array of string which matched the string searched
 *
 */
char **command_completion(const char *text, int start, int end)
{
    /* Tell readline that if we don't find a suitable completion, it should fall
     * back on its built-in filename completion. */
    rl_attempted_completion_over = 0;

    return rl_completion_matches(text, command_generator);
}
/** This function frees the memory allocated for the struct tab_completion.
 *
 */ 
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
 *
 * -text: string to search.
 * -state: iterator for number of callse.
 *
 * Returns: returns the match found or NULL if no match.
 */
char *command_generator(const char *text, int state)
{

    if(state == 0){
        tab_dirs = calloc(1, sizeof(struct tab_completion)); 
        if(!tab_dirs){
            perror("calloc");
            exit(EXIT_FAILURE);
        }
        tab_dirs->builtins = 0;
        tab_dirs->index = 0;
        tab_dirs->size = 0;

        char *env_path = NULL;
        env_path = strdup(getenv("PATH"));

        size_t paths_sz = 4;
        tab_dirs->env_dirs = malloc(paths_sz * sizeof(char*));

        if(*env_path != '\0'){
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
                if((directory = opendir(tab_dirs->env_dirs[tab_dirs->index])) != NULL)
                    break;
                else
                    tab_dirs->index += 1;
            }
            free(env_path);
        }
    }

    while(tab_dirs->index < tab_dirs->size){
        struct dirent *entry;
        while((entry = readdir(directory)) != NULL){
            char *search;
            if((search = strstr(entry->d_name, text)) == entry->d_name)
                return strdup(entry->d_name);
        }
        tab_dirs->index += 1;
        while(tab_dirs->index < tab_dirs->size){
            if((directory = opendir(tab_dirs->env_dirs[tab_dirs->index])) != NULL)
                break;
            else
                tab_dirs->index += 1;
        }
    }

    while(tab_dirs->builtins < sizeof(builtins)/sizeof(*builtins)){

        int i = tab_dirs->builtins;
        tab_dirs->builtins += 1;
        if(*text == '\0')
            return strdup(builtins[i]);

        char *search;
        if((search = strstr(builtins[i], text)) == builtins[i])
            return strdup(builtins[i]);
    }


    clean_tabs();
    return NULL;
}
/** This handler stops an executing command and refreshes the prompt if there is
 *  no command running.
 */
void sigint(int running){


    if(!scripting){

        printf("\n");
        if(running != 0){
            rl_on_new_line();
            rl_replace_line("",1);
            rl_redisplay();
        }

        fflush(stdout);
    }

}

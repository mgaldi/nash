/**@file
 * This file handles the history structure which is used for the history
 * command.
 *
 */
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "history.h"
#include "logger.h"

/** This struct is used to hold all the history commands. Total is used for the
 *  total commands in the history. Limit is the maximum number of commands that
 *  should be stored. **commands holds the list of commands.
 *
 */
struct history {
    unsigned int total;
    unsigned int limit;
    char **commands;
};

static struct history *c_history; 
/** This function initializes the history struct using the limit which is passed
 *  as the max numbers of history commands to be saved.
 *
 */
void hist_init(unsigned int limit)
{
    // TODO: set up history data structures, with 'limit' being the maximum
    // number of entries maintained.
    c_history = malloc(1 * sizeof(struct history));
    if(!c_history){
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    c_history->total = 0;
    c_history->limit = limit;
    c_history->commands = malloc(limit * sizeof(char *));
    if(!c_history->commands){
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    for(int i = 0; i < limit; i++){
        c_history->commands[i] = NULL;
    }
}

/** Thist function deallocates all the memory allocated for the structure
 *  containing the commands of the history.
 *
 */
void hist_destroy(void)
{
    size_t cmds_sz = c_history->total;
    if(c_history->total > c_history->limit)
        cmds_sz = c_history->limit;

    for(int i = 0; i < cmds_sz; i++){
        free(c_history->commands[i]);
    }
    free(c_history->commands);
    free(c_history);

}

/** This function adds a new  command to the history struct
 *
 */
void hist_add(const char *cmd)
{
    if(c_history->commands[c_history->total % c_history->limit] != NULL){
       free(c_history->commands[c_history->total % c_history->limit]);  
    }
    c_history->commands[c_history->total % c_history->limit] = strdup(cmd);
    c_history->total += 1;


}

/** This function prints all the commands in the history up to limit commands
 *
 */
void hist_print(void)
{

    size_t cmds_sz = c_history->total;

    int i = c_history->total - c_history->limit;
    if(i < 0)
       i = 0; 

    if(c_history->total > c_history->limit)
        cmds_sz = c_history->limit + i;

    while(i < cmds_sz){
        printf("%d  %s\n", i + 1, (c_history->commands[i % c_history->limit]));
        i++;
    }

}

/** This function search a command based on the prefix that it receives
 *  strstr helps with the goal
 */
const char *hist_search_prefix(char *prefix)
{
    // TODO: Retrieves the most recent command starting with 'prefix', or NULL
    // if no match found.
    size_t commands_sz = c_history->total;
    if(c_history->total > c_history->limit)
        commands_sz = c_history->limit;

    int command = (c_history->total - 1)% c_history->limit;
    for(int i = 0; i < commands_sz; i++){

        if(strstr(c_history->commands[command], prefix) == c_history->commands[command])
            return c_history->commands[command]; 

        if(command == 0){
            command = commands_sz - 1;
            continue;
        }
        command--;
    }
    return NULL;
}

/** This function returns the command corresponding to the command_number 
 *
 */
const char *hist_search_cnum(int command_number)
{
    // TODO: Retrieves a particular command number. Return NULL if no match
    // found.
    int lower = c_history->total - c_history->limit;
    if(lower < 0)
        lower = 0;

    int upper = c_history->total;
    if(((command_number) > lower) && ((command_number) <= upper)){

        return c_history->commands[(command_number - 1) % c_history->limit];
    }
    return NULL;
}

/** This function returns the index of the last command
 *  the prompt starts from 1, hence the +1
 */
unsigned int hist_last_cnum(void)
{
    if(c_history)
        return (c_history->total + 1);
    return 0;
}

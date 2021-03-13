/**
 * @file
 *
 * Text-based UI functionality. These functions are primarily concerned with
 * interacting with the readline library.
 */

#ifndef _UI_H_
#define _UI_H_

char **command_completion(const char *text, int start, int end);
char *command_generator(const char *text, int state);
void init_ui(void);
void destroy_ui();
int key_up(int, int);
int key_down(int, int);
char *prompt_line(void);
char *read_command(void);
void set_prompt_cwd();
void set_prompt_stat(int, unsigned int);
void clear_sz();

#endif

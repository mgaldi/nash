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
int key_up(int count, int key);
int key_down(int count, int key);
char *prompt_line(void);
char *read_command(void);
void set_prompt_cwd();
void set_prompt_stat(int status, unsigned int i);
void clear_sz();

#endif

#ifndef _JOBS_H_
#define _JOBS_H_

void jobs_init(unsigned int);
void jobs_destroy(void);
void jobs_add(char *, int);
void jobs_delete(int);
void jobs_print(void);
#endif

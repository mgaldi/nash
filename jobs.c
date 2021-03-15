/**@file
 *  This file is used for handling the background jobs.
 */
#include <stdlib.h>
#include <string.h>
#include "jobs.h"
#include "util.h"
#include "logger.h"


/** This struct holds the different nodes which store the background jobs.
 *  
 *  -total: the total number of jobs.
 *  -size:  the maximum number of jobs it can hold.
 *  -head:  the first member of the list. In this case a dummy head.
 */
struct jobs_list {
    size_t total;
    size_t limit;
    struct node *head;
};

/** This struct holds the information for a background job.
 *
 *  -bg_job: name of the job.
 *  -pid: pid associated with job.
 *  -next: pointer to next job in the list.
 *
 */
struct node {
    char *bg_job;
    pid_t pid;
    struct node *next;

};

static struct jobs_list *jobs = NULL;
static struct node *head = NULL;

/** This function allocates the memory for the list of jobs and initializes a
 *  dummy head.
 *  -size: max number of jobs.
 *
 */
void jobs_init(unsigned int limit){

    jobs = malloc(1 * sizeof(struct jobs_list));
    if(!jobs){
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    jobs->total = 0;
    jobs->limit = limit;

    head = jobs->head = malloc(1 * sizeof(struct node));
    if(!head){
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    head->next = NULL;
    head->bg_job = NULL;

}

/** This function frees the memory allocated for storing the background jobs.
 *
 */
void jobs_destroy(){

    if(head->next == NULL){
        free(head);
        free(jobs);
        return;
    }

    while(head != NULL){
    struct node *temp_node = head;
        head = head->next;
        if(temp_node->bg_job)
            free(temp_node->bg_job);

        free(temp_node);
    }
    free(jobs);

}

/** This function add a new background job to the list.
 *
 *  -command: the command of the job.
 *  -pid: the pid associated with the job.
 *
 */
void jobs_add(char *command, int pid){

    struct node *new_node = malloc(1 * sizeof(struct node));
    if(!new_node){
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    new_node->bg_job = strdup(command);
    if(!new_node->bg_job){
        perror("strdup");
        exit(EXIT_FAILURE);
    }

    new_node->pid = pid;
    new_node->next = NULL;

    struct node *curr_node = head;
    while(curr_node->next != NULL)
        curr_node = curr_node->next;

    curr_node->next = new_node;
    jobs->total += 1;

}

/** This function deletes a job with the same pid passed as argument.
 *
 *  -pid: pid associated with background job.
 *
 */
void jobs_delete(int pid){

    if(!head)
        return;

    struct node *curr_node = head->next;
    struct node *prev_node = head;
    while(curr_node != NULL){
        if(curr_node->pid == pid){

            prev_node->next = curr_node->next;
            free(curr_node->bg_job);
            free(curr_node);
            jobs->total -= 1;
            return;
        }

        prev_node = curr_node;
        curr_node = curr_node->next; 
    }

}

/** This function prints all the current background jobs.
 *
 */
void jobs_print(){

    struct node *curr_node = head->next;
    while(curr_node != NULL){

        printf("%s\n", curr_node->bg_job);
        curr_node = curr_node->next;
    }

}

/** This function checks wether or not the job limit has been reached.
 *  
 *  Returns: 0 if the limit has not been reached. -1 if limit has been reached.
 *
 */
int jobs_check(){

    if(jobs->total == 10)
        return -1;

    return 0;

}

#include <stdlib.h>
#include <string.h>
#include "jobs.h"
#include "util.h"
#include "logger.h"





struct node {
    char *bg_job;
    pid_t pid;
    struct node *next;

};

struct node *head = NULL;
void jobs_init(unsigned int size){

    head = malloc(sizeof(struct node));
    if(!head){
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    head->next = NULL;

}

void jobs_destroy(){

    while(head != NULL){
    struct node *temp_node = head;
        head = head->next;
        free(temp_node);
    }
}

void jobs_add(char *command, int pid){

    struct node *new_node = malloc(sizeof(struct node));
    if(!new_node){
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    new_node->bg_job = strdup(command);
    if(!new_node->bg_job){
        exit(EXIT_FAILURE);
    }
    new_node->pid = pid;
    new_node->next = NULL;

    struct node *curr_node = head;
    while(curr_node->next != NULL){
        curr_node = curr_node->next;
    }
    curr_node->next = new_node;

}

void jobs_delete(int pid){

    struct node *curr_node = head->next;
    struct node *prev_node = head;
    while(curr_node != NULL){
        if(curr_node->pid == pid){

            prev_node->next = curr_node->next;
            free(curr_node->bg_job);
            free(curr_node);
            return;
        }

        prev_node = curr_node;
        curr_node = curr_node->next; 
    }
}
void jobs_print(){

    struct node *curr_node = head->next;
    while(curr_node != NULL){

        printf("%s\n", curr_node->bg_job);
        curr_node = curr_node->next;
    }

}

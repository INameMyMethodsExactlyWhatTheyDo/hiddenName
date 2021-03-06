#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

typedef int pid_t;

// /*Info of current file*/
// struct file_table_entry {
//    int fd;
//    struct file* file;
//    struct list_elem elem;
//    struct dir *dir;
// };

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

#endif /* userprog/process.h */

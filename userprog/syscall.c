#include <stdio.h>
#include <stdlib.h>
#include <syscall-nr.h>
#include <stdbool.h>
#include "userprog/syscall.h"
#include "userprog/process.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "threads/palloc.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "filesys/directory.h"
#include "filesys/inode.h"

static void syscall_handler(struct intr_frame *);

// System Calls
static void sys_halt(void);
//static void sys_exit(int status);
static tid_t sys_exec(const char *cmd_line);
static int sys_wait(tid_t pid);
static bool sys_create(const char *file, unsigned initial_size);
static bool sys_remove(const char *file);
static int sys_open(const char *file);
static int sys_filesize(int fd);
static int sys_read(int fd, void *buffer, unsigned size);
static int sys_write(int fd, const void *buffer, unsigned size);
static void sys_seek(int fd, unsigned position);
static unsigned sys_tell(int fd);
static void sys_close(int fd);

static void invalid_access(void);
static void read_user_mem(void *dest, void *uaddr, size_t size);
static int get_user(const uint8_t *uaddr);
static bool put_user(uint8_t *udst, uint8_t byte);

static struct file_table_entry *get_file_table_entry(int fd);
static void check_init_list(struct list* list);

static struct lock lock;

struct file_table_entry{
  int fd;
  struct file *file;
  struct list_elem elem;
};

void syscall_init(void){
  intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");

  lock_init(&lock);
}

static void syscall_handler(struct intr_frame *f UNUSED){
  uint32_t syscall_num;
  read_user_mem(&syscall_num, f->esp, sizeof(syscall_num));
  thread_current() -> esp = &f->esp;
  switch (syscall_num){
    /* Halt the operating system. */
    case SYS_HALT:{
      sys_halt();
      break;
    }
    /* Terminate this process. */
    case SYS_EXIT:{
      thread_current()->exit_status = 0;
      int status=0;
      if (is_kernel_vaddr(syscall_num + 1))
        sys_exit(-1);
      status = syscall_num + 1;
      read_user_mem(&status, f->esp + 4, sizeof(status));
      sys_exit(status);
      break;
    }
    /* Start another process. */
    case SYS_EXEC:{
      const char *cmd_line;
      read_user_mem(&cmd_line, f->esp + 4, sizeof(cmd_line));
      f->eax = sys_exec(cmd_line);
      break;
    }
    /* Wait for a child process to die. */
    case SYS_WAIT:{
      tid_t pid;
      read_user_mem(&pid, f->esp + 4, sizeof(pid));
      f->eax = sys_wait(pid);
      break;
    }
    /* Create a file. */
    case SYS_CREATE:{
      const char *file;
      unsigned initial_size;
      read_user_mem(&file, f->esp + 4, sizeof(file));
      read_user_mem(&initial_size, f->esp + 8, sizeof(initial_size));
      f->eax = sys_create(file, initial_size);
      break;
    }
    /* Delete a file. */
    case SYS_REMOVE:{
      const char *file;
      read_user_mem(&file, f->esp + 4, sizeof(file));
      f->eax = sys_remove(file);
      break;
    }
    /* Open a file. */
    case SYS_OPEN:{
      const char *file;
      read_user_mem(&file, f->esp + 4, sizeof(file));
      f->eax = sys_open(file);
      break;
    }
    /* Obtain a file's size. */
    case SYS_FILESIZE:{
      int fd;
      read_user_mem(&fd, f->esp + 4, sizeof(fd));
      f->eax = sys_filesize(fd);
      break;
    }
    /* Read from a file. */
    case SYS_READ:{
      int fd;
      void *buffer;
      unsigned size;
      read_user_mem(&fd, f->esp + 4, sizeof(fd));
      read_user_mem(&buffer, f->esp + 8, sizeof(buffer));
      read_user_mem(&size, f->esp + 12, sizeof(size));
      f->eax = sys_read(fd, buffer, size);
      break;
    }
    /* Write to a file. */
    case SYS_WRITE:{
      int fd;
      void *buffer;
      unsigned size;
      read_user_mem(&fd, f->esp + 4, sizeof(fd));
      read_user_mem(&buffer, f->esp + 8, sizeof(buffer));
      read_user_mem(&size, f->esp + 12, sizeof(size));
      f->eax = sys_write(fd, buffer, size);
      break;
    }
    /* Change position in a file. */
    case SYS_SEEK:{
      int fd;
      unsigned position;
      read_user_mem(&fd, f->esp + 4, sizeof(fd));
      read_user_mem(&position, f->esp + 8, sizeof(position));
      sys_seek(fd, position);
      break;
    }
    /* Report current position in a file. */
    case SYS_TELL:{
      int fd;
      read_user_mem(&fd, f->esp + 4, sizeof(fd));
      f->eax = sys_tell(fd);
      break;
    }
    /* Close a file. */
    case SYS_CLOSE:{
      int fd;
      read_user_mem(&fd, f->esp + 4, sizeof(fd));
      sys_close(fd);
      break;
    }
    default:{
      thread_current()->exit_status = -1;
      thread_exit();
    }
  }
}

//----------------------- System Call Functions --------------------------//

/**
 * Terminates Pintos by calling shutdown_power_off() 
 * (declared in "threads/init.h"). This should be seldom used, 
 * because you lose some information about possible deadlock 
 * situations, etc. 
 */
void sys_halt(void){
  shutdown_power_off();
}

/**
 * Terminates the current user program, returning status to the 
 * kernel. If the process's parent waits for it (see below), 
 * this is the status that will be returned. Conventionally, 
 * a status of 0 indicates success and nonzero values indicate 
 * errors. 
 */
void sys_exit(int status){
  struct thread *curr = thread_current();
  curr->exit_status = status;
  char *command_name = curr->name;

  char *saveptr;
  printf("%s: exit(%d)\n", strtok_r(command_name, " ", &saveptr), curr->exit_status);
  // Save status to kernel
  thread_exit();
}

/**
 * Runs the executable whose name is given in cmd_line, passing 
 * any given arguments, and returns the new process's program id (pid). 
 * Must return pid -1, which otherwise should not be a valid pid, if 
 * the program cannot load or run for any reason. Thus, the parent 
 * process cannot return from the exec until it knows whether the 
 * child process successfully loaded its executable. You must use 
 * appropriate synchronization to ensure this. 
 */
tid_t sys_exec(const char *cmd_line){
  lock_acquire(&lock);
  tid_t pid = process_execute(cmd_line);
  lock_release(&lock);
  return pid;
}

/**
 * Waits for a child process pid and retrieves the child's exit status. 
 */
int sys_wait(tid_t pid){
  int status = process_wait(pid);
  return status;
}

/**
 * Creates a new file called name initially initial_size bytes in size. 
 * Returns true if successful, false otherwise. Creating a new file does 
 * not open it: opening the new file is a separate operation which would 
 * require a open system call. 
 */
bool sys_create(const char *name, unsigned initial_size){
  if(name == NULL){
    sys_exit(-1);
  }
  lock_acquire(&lock);
  bool success = filesys_create(name, initial_size);
  lock_release(&lock);
  return success;
}

/**
 * Deletes the file called name. Returns true if successful, false 
 * otherwise. A file may be removed regardless of whether it is open 
 * or closed, and removing an open file does not close it. See Removing 
 * an Open File, for details. 
 */
bool sys_remove(const char *name){
  lock_acquire(&lock);
  bool success = filesys_remove(name);
  lock_release(&lock);
  return success;
}

/**
 * Opens the file called name. Returns a nonnegative integer handle 
 * called a "file descriptor" (fd), or -1 if the file could not be opened. 
 */
int sys_open(const char *name){
  if(name == NULL){
    return -1;
  }

  if(!is_user_vaddr(name)){
    return -1;
  }

  struct file_table_entry *fte = palloc_get_page(0);
  if (!fte){
    return -1;
  }

  lock_acquire(&lock);
  struct file *file = filesys_open(name);
  if (file == NULL){
    lock_release(&lock);
    palloc_free_page(fte);
    return -1;
  }
  fte->file = file;
  lock_release(&lock);

  // Add entry to current thread's file table
  struct thread *curr = thread_current();
  struct list *file_table = &curr->file_table;
  check_init_list(file_table);

  if (list_empty(file_table)){
    fte->fd = 3;
  } else{
    struct file_table_entry *back = list_entry(list_back(file_table), struct file_table_entry, elem);
    fte->fd = back->fd + 1;
  }
  list_push_back(file_table, &fte->elem);

  return fte->fd;
}

/**
 * Returns the size, in bytes, of the file open as fd. 
 */
int sys_filesize(int fd){
  int size;

  lock_acquire(&lock);
  struct file_table_entry *fte = get_file_table_entry(fd);
  if (fte == NULL){
    lock_release(&lock);
    return -1;
  }
  size = file_length(fte->file);
  lock_release(&lock);

  return size;
}

/**
 * Reads size bytes from the file open as fd into buffer. Returns the 
 * number of bytes actually read (0 at end of file), or -1 if the file 
 * could not be read (due to a condition other than end of file). Fd 0 
 * reads from the keyboard using input_getc(). 
 */
int sys_read(int fd, void *buffer, unsigned size){
  unsigned read;
  bool success;

  for (unsigned i = 0; i < size; i++) {
    if (buffer == NULL || !is_user_vaddr(buffer)){
      invalid_access();
    } 
  }

  lock_acquire(&lock);
  // reads from keyboard
  if (fd == 0){
    for (unsigned i = 0; i < size; i++){
      success = put_user(buffer, input_getc());
      if (!success){
        lock_release(&lock);
        invalid_access();
      }
    }
    read = size;
    // reads from open file
  } else{
    struct file_table_entry *fte = get_file_table_entry(fd);
    if (fte == NULL){
      lock_release(&lock);
      return -1;
    }
    read = file_read(fte->file, buffer, size);
  }

  lock_release(&lock);
  return read;
}

/**
 * Writes size bytes from buffer to the open file fd. Returns the number 
 * of bytes actually written, which may be less than size if some bytes 
 * could not be written. 
 */
int sys_write(int fd, const void *buffer, unsigned size){
  unsigned written;

  for (unsigned i = 0; i < size; i++) {
    if (buffer == NULL || !is_user_vaddr(buffer)){
      invalid_access();
    } 
  }

  lock_acquire(&lock);
  // writes to console
  if (fd == 1){
    putbuf(buffer, size);
    lock_release(&lock);
    return size;

    // write to file
  } else{
    struct file_table_entry *fte = get_file_table_entry(fd);
    if (fte == NULL || fte->file == NULL){
      lock_release(&lock);
      return -1;
    }

    written = file_write(fte->file, buffer, size);
  }

  lock_release(&lock);
  return written;
}

/**
 * Changes the next byte to be read or written in open file fd to position, 
 * expressed in bytes from the beginning of the file. (Thus, a position of 
 * 0 is the file's start.) 
 */
void sys_seek(int fd, unsigned position){
  lock_acquire(&lock);

  struct file_table_entry *fte = get_file_table_entry(fd);
  if (fte == NULL){
    lock_release(&lock);
    return;
  }

  file_seek(fte->file, position);
  lock_release(&lock);
}

/**
 * Returns the position of the next byte to be read or written in open file fd, 
 * expressed in bytes from the beginning of the file. 
 */
unsigned sys_tell(int fd){
  unsigned pos;

  lock_acquire(&lock);
  struct file_table_entry *fte = get_file_table_entry(fd);
  if (fte == NULL){
    lock_release(&lock);
    return -1;
  }

  pos = file_tell(fte->file);
  lock_release(&lock);
  return pos;
}

/**
 * Closes file descriptor fd. Exiting or terminating a process implicitly closes 
 * all its open file descriptors, as if by calling this function for each one. 
 */
void sys_close(int fd){
  lock_acquire(&lock);
  struct file_table_entry *fte = get_file_table_entry(fd);
  if (fte == NULL || fte->file == NULL){
    lock_release(&lock);
    return;
  }

  file_close(fte->file);
  list_remove(&fte->elem);
  palloc_free_page(fte);
  lock_release(&lock);
}

//----------------------- Accessing User Memory Functions --------------------------//

/**
 * All types of invalid pointers must be rejected without harm to the 
 * kernel or other running processes, by terminating the offending process and 
 * freeing its resources. 
 */
static void invalid_access(){
  if (lock_held_by_current_thread(&lock)){
    lock_release(&lock);
  }
  sys_exit(-1);
}

/**
 * As part of a system call, the kernel must often access memory through pointers 
 * provided by a user program. The kernel must be very careful about doing so, 
 * because the user can pass a null pointer, a pointer to unmapped virtual memory, 
 * or a pointer to kernel virtual address space (above PHYS_BASE)
 */
static void read_user_mem(void *dest, void *uaddr, size_t size){
  // make sure uaddr is below PHYS_BASE and is not a NULL pointer
  if (uaddr == NULL || !is_user_vaddr(uaddr)){
    invalid_access();
  }

  for (unsigned int i = 0; i < size; i++){
    int byte = get_user(uaddr + i);

    // hit segment fault during mem read
    if (byte == -1){
      invalid_access();
    }

    // save byte data to destination address
    *(uint8_t *)(dest + i) = byte & 0xFF;
  }
}

/** 
 * Reads a byte at user virtual address UADDR. UADDR must be below PHYS_BASE. Returns 
 * the byte value if successful, -1 if a segfaultoccurred. 
 */
static int get_user(const uint8_t *uaddr){
  int result;
  asm("movl $1f, %0; movzbl %1, %0; 1:"
      : "=&a"(result)
      : "m"(*uaddr));
  return result;
}

/** 
 * Writes BYTE to user address UDST. UDST must be below PHYS_BASE. Returns true if 
 * successful, false if a segfault occurred. 
 */
static bool put_user(uint8_t *udst, uint8_t byte){
  int error_code;
  asm("movl $1f, %0; movb %b2, %1; 1:"
      : "=&a"(error_code), "=m"(*udst)
      : "q"(byte));
  return error_code != -1;
}

//--------------------------- File Table Functions -----------------------------//

// Get pointer to file table entry in current file table
static struct file_table_entry *get_file_table_entry(int fd){
  struct thread *curr = thread_current();
  struct list *file_table = &(curr->file_table);
  struct file_table_entry *fte;
  struct list_elem *e;
  check_init_list(file_table);

  for (e = list_begin(file_table); e != list_end(file_table); e = list_next(e)){
    fte = list_entry(e, struct file_table_entry, elem);
    if (fte->fd == fd){
      return fte;
    }
  }
  return NULL;
}

static void check_init_list(struct list* list){

  if(list -> head.next == NULL){
    list_init(list);
  }

}
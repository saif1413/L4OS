#ifndef _PROCESS_H_
#define _PROCESS_H_

#include "pager.h"
#include "vfs.h"

#define RUN_AS_PROCESS 0
#define RUN_AS_THREAD 1

// The process data structure
typedef struct Process_t Process;

// Find a process
Process *process_lookup(L4_Word_t key);

// Create a new process with a given name
Process *process_init(void);

// Add a region to a process
void process_add_region(Process *p, Region *r);

// Set the name of the process
void process_set_name(Process *p, char *name);

// Set an initial stack pointer for the process
void process_set_sp(Process *p, void *sp);

// Set an initial instruction pointer for the process
void process_set_ip(Process *p, void *ip);

// Prepare a process to be run
void process_prepare(Process *p);

// Run a process
L4_ThreadId_t process_run(Process *p, int asThread);

// Get the process id of a process
pid_t process_get_pid(Process *p);

// Get the threadid of a process
L4_ThreadId_t process_get_tid(Process *p);

// Get the page table of a process
PageTable *process_get_pagetable(Process *p);

// Get the regions of a process
Region *process_get_regions(Process *p);

// Kill a process and free its resources
// Returns 0 if successful, -1 otherwise
int process_kill(Process *p);

// Write the status of the first n processes to dest
int process_write_status(process_t *dest, int n);

// Get a pointer to a process_t struct from the process
process_t *process_get_info(Process *p);

// Get the list of files used by a process
VFile *process_get_files(Process *p);

// Wait for any process to exit before waking
void process_wait_any(Process *waiter);

// Wait for a specified process to exit before waking
void process_wait_for(Process *waitFor, Process *waiter);

// Give the rootserver a PCB
void process_add_rootserver(void);

#endif // process.h

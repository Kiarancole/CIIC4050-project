#ifndef AIR_CONTROL_C_INCLUDE_FUNCTIONS_H_
#define AIR_CONTROL_C_INCLUDE_FUNCTIONS_H_

#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define SH_MEMORY_NAME "/air_control_memory"
#define TOTAL_TAKEOFFS 20

// Global variables
extern int planes;
extern int takeoffs;
extern int total_takeoffs;

// Shared memory
extern int* sh_memory;
extern int sh_memory_fd;

// Radio process PID
extern pid_t radio_pid;

// Mutexes
extern pthread_mutex_t state_lock;
extern pthread_mutex_t runway1_lock;
extern pthread_mutex_t runway2_lock;

// Function declarations
void MemoryCreate();
void SigHandler2(int signal);
void* TakeOffsFunction(void* arg);

#endif  // AIR_CONTROL_C_INCLUDE_FUNCTIONS_H_

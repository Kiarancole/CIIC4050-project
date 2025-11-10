// #include "../include/functions.h"

// #include <stdio.h>

// // Global variables
// int planes = 0;
// int takeoffs = 0;
// int total_takeoffs = 0;

// // Shared memory
// int* sh_memory = NULL;
// int sh_memory_fd = -1;

// // Radio process PID
// pid_t radio_pid = -1;

// // Mutexes
// pthread_mutex_t state_lock = PTHREAD_MUTEX_INITIALIZER;
// pthread_mutex_t runway1_lock = PTHREAD_MUTEX_INITIALIZER;
// pthread_mutex_t runway2_lock = PTHREAD_MUTEX_INITIALIZER;

// void MemoryCreate() {
//   // Create the shared memory segment
//   sh_memory_fd = shm_open(SH_MEMORY_NAME, O_CREAT | O_RDWR, 0666);
//   if (sh_memory_fd == -1) {
//     perror("shm_open");
//     exit(1);
//   }

//   // Configure the size (3 integers: air_control, radio, ground_control)
//   if (ftruncate(sh_memory_fd, sizeof(int) * 3) == -1) {
//     perror("ftruncate");
//     exit(1);
//   }

//   // Map the memory block
//   sh_memory = (int*)mmap(NULL, sizeof(int) * 3, PROT_READ | PROT_WRITE,
//                          MAP_SHARED, sh_memory_fd, 0);
//   if (sh_memory == MAP_FAILED) {
//     perror("mmap");
//     exit(1);
//   }

//   // Store the PID of air_control in the first position
//   sh_memory[0] = getpid();
// }

// void SigHandler2(int signal) {
//   // Increment planes by 5 when SIGUSR2 is received
//   (void)signal;  // Unused parameter
//   pthread_mutex_lock(&state_lock);
//   planes += 5;
//   pthread_mutex_unlock(&state_lock);
// }

// void* TakeOffsFunction(void* arg) {
//   (void)arg;  // Unused parameter
//   while (1) {
//     pthread_mutex_lock(&state_lock);
//     if (total_takeoffs >= TOTAL_TAKEOFFS) {
//       pthread_mutex_unlock(&state_lock);
//       break;
//     }
//     pthread_mutex_unlock(&state_lock);

//     // Try to acquire a runway
//     pthread_mutex_t* runway = NULL;
//     while (runway == NULL) {
//       if (pthread_mutex_trylock(&runway1_lock) == 0) {
//         runway = &runway1_lock;
//       } else if (pthread_mutex_trylock(&runway2_lock) == 0) {
//         runway = &runway2_lock;
//       } else {
//         usleep(1000);
//       }
//     }

//     // Check if there are planes and process takeoff
//     pthread_mutex_lock(&state_lock);
//     if (total_takeoffs >= TOTAL_TAKEOFFS) {
//       pthread_mutex_unlock(&state_lock);
//       pthread_mutex_unlock(runway);
//       break;
//     }

//     if (planes > 0) {
//       planes--;
//       takeoffs++;
//       total_takeoffs++;

//       // Send SIGUSR1 every 5 takeoffs
//       if (takeoffs == 5) {
//         kill(radio_pid, SIGUSR1);
//         takeoffs = 0;
//       }

//       // Check if we've reached the total
//       if (total_takeoffs >= TOTAL_TAKEOFFS) {
//         pthread_mutex_unlock(&state_lock);
//         sleep(1);  // Simulate takeoff time
//         pthread_mutex_unlock(runway);

//         // Send SIGTERM to radio
//         kill(radio_pid, SIGTERM);
//         break;
//       }
//       pthread_mutex_unlock(&state_lock);

//       // Simulate takeoff time
//       sleep(1);
//     } else {
//       pthread_mutex_unlock(&state_lock);
//     }

//     // Release the runway
//     pthread_mutex_unlock(runway);
//   }

//   return NULL;
// }

#include "../include/functions.h"

// Global variables
int planes = 0;
int takeoffs = 0;
int total_takeoffs = 0;

// Shared memory
int* sh_memory = NULL;
int sh_memory_fd = -1;

// Radio process PID
pid_t radio_pid = -1;

// Mutexes
pthread_mutex_t state_lock;
pthread_mutex_t runway1_lock;
pthread_mutex_t runway2_lock;

void MemoryCreate() {
  // Create shared memory
  sh_memory_fd = shm_open(SH_MEMORY_NAME, O_CREAT | O_RDWR, 0666);
  if (sh_memory_fd == -1) {
    perror("shm_open failed");
    exit(1);
  }

  // Set size for 3 integers (air, radio, ground)
  if (ftruncate(sh_memory_fd, sizeof(int) * 3) == -1) {
    perror("ftruncate failed");
    exit(1);
  }

  // Map memory
  sh_memory = mmap(NULL, sizeof(int) * 3,
                   PROT_READ | PROT_WRITE, MAP_SHARED, sh_memory_fd, 0);
  if (sh_memory == MAP_FAILED) {
    perror("mmap failed");
    exit(1);
  }

  // Initialize all 3 values
  sh_memory[0] = getpid();  // air_control PID
  sh_memory[1] = 0;         // will be radio PID
  sh_memory[2] = 0;         // ground_control PID placeholder
}

void SigHandler2(int signal) {
  (void)signal;
  pthread_mutex_lock(&state_lock);
  planes += 5;
  pthread_mutex_unlock(&state_lock);
}

void* TakeOffsFunction(void* arg) {
  (void)arg;
  pthread_mutex_t* runway = NULL;

  while (1) {
    pthread_mutex_lock(&state_lock);
    if (total_takeoffs >= TOTAL_TAKEOFFS) {
      pthread_mutex_unlock(&state_lock);
      break;
    }
    pthread_mutex_unlock(&state_lock);

    // Try to get a runway
    runway = NULL;
    while (!runway) {
      if (pthread_mutex_trylock(&runway1_lock) == 0) {
        runway = &runway1_lock;
      } else if (pthread_mutex_trylock(&runway2_lock) == 0) {
        runway = &runway2_lock;
      } else {
        usleep(1000);
      }
    }

    pthread_mutex_lock(&state_lock);
    if (planes > 0) {
      planes--;
      takeoffs++;
      total_takeoffs++;

      // Every 5 takeoffs, notify radio
      if (takeoffs % 5 == 0) {
        kill(radio_pid, SIGUSR1);
        takeoffs = 0;
      }

      pthread_mutex_unlock(&state_lock);
      sleep(1);  // simulate takeoff
    } else {
      pthread_mutex_unlock(&state_lock);
      usleep(5000);
    }

    pthread_mutex_unlock(runway);

    // Exit condition check
    pthread_mutex_lock(&state_lock);
    if (total_takeoffs >= TOTAL_TAKEOFFS) {
      pthread_mutex_unlock(&state_lock);
      break;
    }
    pthread_mutex_unlock(&state_lock);
  }

  // When total reached, send SIGTERM
  kill(radio_pid, SIGTERM);

  return NULL;
}

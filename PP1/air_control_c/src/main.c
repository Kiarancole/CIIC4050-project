// #include <stdio.h>

// #include "../include/functions.h"

// int main() {
//   // Create the shared memory segment
//   MemoryCreate();

//   // Configure the SIGUSR2 signal to increment the planes on the runway by 5
//   signal(SIGUSR2, SigHandler2);

//   // Launch the 'radio' executable and store its PID in shared memory
//   radio_pid = fork();
//   if (radio_pid == 0) {
//     // Child process - execute radio
//     execlp("../radio/build/radio", "radio", SH_MEMORY_NAME, NULL);
//     perror("execlp has failed");
//     exit(1);

//   } else if (radio_pid > 0) {
//     // Parent process - store radio PID
//     sh_memory[1] = radio_pid;
//     // Give radio process time to initialize
//     usleep(100000);  // 100ms
//   } else {
//     perror("fork");
//     exit(1);
//   }

//   // Launch 5 threads which will be the controllers
//   pthread_t threads[5];
//   for (int i = 0; i < 5; i++) {
//     if (pthread_create(&threads[i], NULL, TakeOffsFunction, NULL) != 0) {
//       perror("pthread_create failed");
//       exit(1);
//     }
//   }

//   // Wait for all threads to finish
//   for (int i = 0; i < 5; i++) {
//     pthread_join(threads[i], NULL);
//   }

//   // Wait for radio process to finish
//   waitpid(radio_pid, NULL, 0);

//   // Cleanup shared memory
//   munmap(sh_memory, sizeof(int) * 3);
//   close(sh_memory_fd);
//   shm_unlink(SH_MEMORY_NAME);

//   return 0;
// }

#include "../include/functions.h"

int main() {
  // Initialize mutexes
  pthread_mutex_init(&state_lock, NULL);
  pthread_mutex_init(&runway1_lock, NULL);
  pthread_mutex_init(&runway2_lock, NULL);

  // Ensure no leftover shared memory
  shm_unlink(SH_MEMORY_NAME);

  // Create shared memory
  MemoryCreate();

  // Configure SIGUSR2 handler
  struct sigaction sa;
  sa.sa_handler = SigHandler2;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  if (sigaction(SIGUSR2, &sa, NULL) == -1) {
    perror("sigaction failed");
    exit(1);
  }

  // Launch radio process
  radio_pid = fork();
  if (radio_pid == 0) {
    // Child -> execute radio
    sh_memory[1] = getpid();
    execlp("../radio/build/radio", "radio", SH_MEMORY_NAME, NULL);
    perror("execlp failed");
    exit(1);
  } else if (radio_pid < 0) {
    perror("fork failed");
    exit(1);
  } else {
    // Parent
    sh_memory[1] = radio_pid;
  }

  // Launch 5 controller threads
  pthread_t controllers[5];
  for (int i = 0; i < 5; i++) {
    if (pthread_create(&controllers[i], NULL, TakeOffsFunction, NULL) != 0) {
      perror("pthread_create failed");
      exit(1);
    }
  }

  // Wait for threads
  for (int i = 0; i < 5; i++) {
    pthread_join(controllers[i], NULL);
  }

  // Wait for radio process to finish
  waitpid(radio_pid, NULL, 0);

  // Cleanup
  munmap(sh_memory, sizeof(int) * 3);
  close(sh_memory_fd);
  shm_unlink(SH_MEMORY_NAME);

  return 0;
}

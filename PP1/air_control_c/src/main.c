#include <stdio.h>

#include "../include/functions.h"

int main() {
  // Create the shared memory segment
  MemoryCreate();

  // Configure the SIGUSR2 signal to increment the planes on the runway by 5
  signal(SIGUSR2, SigHandler2);

  // Launch the 'radio' executable and store its PID in shared memory
  radio_pid = fork();
  if (radio_pid == 0) {
    // Child process - execute radio
    execlp("../radio/build/radio", "radio", SH_MEMORY_NAME, NULL);
    perror("execlp has failed");
    exit(1);

  } else if (radio_pid > 0) {
    // Parent process - store radio PID
    sh_memory[1] = radio_pid;
    // Give radio process time to initialize
    usleep(100000);  // 100ms
  } else {
    perror("fork");
    exit(1);
  }

  // Launch 5 threads which will be the controllers
  pthread_t threads[5];
  for (int i = 0; i < 5; i++) {
    if (pthread_create(&threads[i], NULL, TakeOffsFunction, NULL) != 0) {
      perror("pthread_create failed");
      exit(1);
    }
  }

  // Wait for all threads to finish
  for (int i = 0; i < 5; i++) {
    pthread_join(threads[i], NULL);
  }

  // Wait for radio process to finish
  waitpid(radio_pid, NULL, 0);

  // Cleanup shared memory
  munmap(sh_memory, sizeof(int) * 3);
  close(sh_memory_fd);
  shm_unlink(SH_MEMORY_NAME);

  return 0;
}
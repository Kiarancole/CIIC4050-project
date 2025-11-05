#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <unistd.h>

#define SH_MEMORY_NAME "/air_control_memory"
#define PLANES_LIMIT 20

int planes = 0;
int takeoffs = 0;
int traffic = 0;

int* sh_memory = NULL;
int sh_memory_fd = -1;

void SigTerm(int signum) {
  (void)signum;  // Unused parameter
  // Close shared memory
  if (sh_memory != NULL) {
    munmap(sh_memory, sizeof(int) * 3);
  }
  if (sh_memory_fd != -1) {
    close(sh_memory_fd);
  }
  printf("finalization of operations...\n");
  exit(0);
}

void SigHandler1(int signum) {
  (void)signum;  // Unused parameter
  // Increase the number of takeoffs by 5
  takeoffs += 5;
}

void Traffic(int signum) {
  (void)signum;  // Unused parameter
  // Calculate the number of waiting planes
  int waiting_planes = planes - takeoffs;

  // Check if there are 10 or more waiting planes
  if (waiting_planes >= 10) {
    printf("RUNWAY OVERLOADED\n");
  }

  // Check if we can add more planes (don't exceed PLANES_LIMIT)
  if (planes < PLANES_LIMIT) {
    planes += 5;
    traffic++;
    // Send SIGUSR2 to radio process
    if (sh_memory != NULL) {
      kill(sh_memory[1], SIGUSR2);
    }
  }
}

int main(int argc, char* argv[]) {
  (void)argc;  // Unused parameter
  (void)argv;  // Unused parameter

  // Open the shared memory block and store this process PID in position 2
  // Retry opening shared memory (wait for air_control to create it)
  int retry_count = 0;
  while (retry_count < 10) {
    sh_memory_fd = shm_open(SH_MEMORY_NAME, O_RDWR, 0666);
    if (sh_memory_fd != -1) {
      break;
    }
    usleep(100000);  // Wait 100ms
    retry_count++;
  }

  if (sh_memory_fd == -1) {
    perror("shm_open");
    exit(1);
  }

  sh_memory = (int*)mmap(NULL, sizeof(int) * 3,
                         PROT_READ | PROT_WRITE, MAP_SHARED, sh_memory_fd, 0);
  if (sh_memory == MAP_FAILED) {
    perror("mmap");
    exit(1);
  }

  // Store ground_control PID in position 2
  sh_memory[2] = getpid();

  // 3. Configure SIGTERM and SIGUSR1 handlers
  signal(SIGTERM, SigTerm);
  signal(SIGUSR1, SigHandler1);

  // 2. Configure the timer to execute the Traffic function.
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = Traffic;
  sigaction(SIGALRM, &sa, NULL);

  struct itimerval timer;
  // Set the initial delay to 500ms
  timer.it_value.tv_sec = 0;
  timer.it_value.tv_usec = 500000;
  // Set the interval to 500ms
  timer.it_interval.tv_sec = 0;
  timer.it_interval.tv_usec = 500000;

  if (setitimer(ITIMER_REAL, &timer, NULL) == -1) {
    perror("setitimer");
    exit(1);
  }

  // Wait for signals
  while (1) {
    pause();
  }

  return 0;
}
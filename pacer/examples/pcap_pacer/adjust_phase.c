#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <unistd.h>
#include <sys/types.h>
#include <inttypes.h>
#include <sys/mman.h>
#include <fcntl.h>

void main (int argc, char* argv[]){
  int64_t phase = atoll(argv[1]);
  int fd = shm_open("pacer_phase",O_RDWR, 0);
  volatile int64_t * phaseShared = mmap(NULL, 8,PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  *phaseShared = phase;
  return;
}

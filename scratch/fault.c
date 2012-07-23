// Read all of the bytes in a mmaped file. Compile like:
//   gcc --std=gnu99 -g fault.c -o fault
// Run like
//   ./fault path/to/big/file

#include <assert.h>
#include <stdio.h>
#include <sys/mman.h>

int main(int argc, char **argv) {
  size_t sum = 0;
  FILE *f = fopen(argv[1], "r");
  assert(f != NULL);
  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  rewind(f);
  void *addr = mmap(NULL, size, PROT_READ, MAP_SHARED, fileno(f), 0);
  if (addr == MAP_FAILED) {
    perror("mmap()");
    return 1;
  }
  const char *caddr = (const char *) addr;
  fclose(f);
  for (long i = 0; i < size; i++) {
    sum += (caddr[i] & 0xff);
  }
  printf("%lu\n", sum);
  munmap(addr, size);
  return 0;
}

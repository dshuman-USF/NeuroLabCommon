#include <stdio.h>
#include <unistd.h>
#include <stdint.h>

__inline__ uint64_t rdtsc() {
  uint32_t lo, hi;
  __asm__ __volatile__ (      // serialize
                        "xorl %%eax,%%eax \n        cpuid"
                        ::: "%rax", "%rbx", "%rcx", "%rdx");
  /* We cannot use "=A", since this would use %rax on x86_64 */
  __asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));
  return (uint64_t)hi << 32 | lo;
}

int
main (void)
{
  uint64_t start = rdtsc ();
  sleep (1);
  uint64_t stop = rdtsc ();
  printf ("%ld\n", (stop - start));
  return 0;
}

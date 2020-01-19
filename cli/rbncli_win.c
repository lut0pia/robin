#include "rbncli.h"

#include <Windows.h>

static double perfcounter_mult;

uint64_t rbncli_get_time() {
  int64_t counter;
  QueryPerformanceCounter((LARGE_INTEGER*)&counter);
  return (uint64_t)(counter * perfcounter_mult);
}
void rbncli_sleep(uint32_t ms) {
  Sleep(ms);
}

void rbncli_platform_init() {
  // Initialize multiplier for performance counter
  LARGE_INTEGER freq;
  QueryPerformanceFrequency((LARGE_INTEGER*)&freq);
  perfcounter_mult = 1.0 / ((double)freq.QuadPart / 1000000.0);
}


#include <chrono>
#include <sys/time.h>
#include <execinfo.h>

#include "spmachine.hpp"

#ifndef _SPTL_TIME_H_
#define _SPTL_TIME_H_

namespace sptl {

using cycles_type = uint64_t;
  
namespace cycle_counter {

  static inline
  cycles_type rdtsc() {
    unsigned int hi, lo;
    __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
    return  ((cycles_type) lo) | (((cycles_type) hi) << 32);
  }
  
  static inline
  void rdtsc_wait(cycles_type n) {
    const cycles_type start = rdtsc();
    while (rdtsc() < (start + n)) {
      __asm__("PAUSE");
    }
  }
  
  static inline
  cycles_type now() {
    return rdtsc();
  }
  
  static inline
  double elapsed(cycles_type time_start, cycles_type time_end) {
    return (double)time_end - (double)time_start;
  }
  
  static inline
  double since(cycles_type time_start) {
    return elapsed(time_start, now());
  }

} // end namespace

namespace wall_clock {

  cycles_type now() {
    struct timespec t;
    clock_gettime(CLOCK_REALTIME, &t);
    return (cycles_type)(t.tv_sec * 1000000000LL + t.tv_nsec);
  }

  double since(cycles_type start) {
    return (now() - start) * machine::cpu_frequency_ghz;
  }

  
} // end namespace
  
} // end namespace

#endif

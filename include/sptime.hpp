
#include <chrono>
#include <sys/time.h>
#include <execinfo.h>

#include "spmachine.hpp"

#ifndef _SPTL_TIME_H_
#define _SPTL_TIME_H_

namespace sptl {

/*---------------------------------------------------------------------*/
/* Cycle counter */

using cycles_type = uint64_t;
  
namespace cycle_counter {

  namespace {
    
    static inline
    cycles_type rdtsc() {
      unsigned int hi, lo;
      __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
      return  ((cycles_type) lo) | (((cycles_type) hi) << 32);
    }

  } // end namespace
  
  static inline
  void wait(cycles_type n) {
    const cycles_type start = rdtsc();
    while (rdtsc() < (start + n)) {
      __asm__("PAUSE");
    }
  }
  
  static inline
  cycles_type now() {
    return rdtsc();
  }

  // pre: cpu_frequency_ghz is initialized by sptl runtime
  // later: enforce the precondition by dynamic check
  static
  double microseconds_of_cycles(double c) {
    double ticks_per_microsecond = cpu_frequency_ghz * 1000.0;
    return c / ticks_per_microsecond;
  }
  
} // end namespace

/*---------------------------------------------------------------------*/
/* Lightweight timer */
  
namespace time {

  using time_type = double;

  static inline
  time_type now() {
    return (time_type)cycle_counter::now();
  }

  static inline
  time_type elapsed(time_type time_start, time_type time_end) {
    return time_end - time_start;
  }

  static inline
  time_type since(time_type time_start) {
    return elapsed(time_start, now());
  }
  
} // end namespace

/*---------------------------------------------------------------------*/
/* Atomic compare-and-exchange, with backoff */
  
namespace {
  
static constexpr
int backoff_nb_cycles = 1l << 17;

static inline
void spin_for(uint64_t nb_cycles) {
  cycle_counter::wait(nb_cycles);
}

template <class T>
bool compare_exchange_with_backoff(std::atomic<T>& cell, T& expected, T desired) {
  if (cell.compare_exchange_strong(expected, desired)) {
    return true;
  }
  spin_for(backoff_nb_cycles);
  return false;
}

} // end namespace
  
} // end namespace

#endif


#include <chrono>
#include <sys/time.h>
#include <execinfo.h>

#include "spmachine.hpp"

#ifndef _SPTL_TIME_H_
#define _SPTL_TIME_H_

namespace sptl {

/*---------------------------------------------------------------------*/
/* Cycle counter */
  
namespace cycle_counter {

  using cycles_type = uint64_t;

  namespace {
    
    static inline
    cycles_type rdtsc() {
      unsigned int hi, lo;
      __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
      return  ((cycles_type) lo) | (((cycles_type) hi) << 32);
    }

  } // end namespace

  // spin wait for at least n cycles
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
  
} // end namespace

/*---------------------------------------------------------------------*/
/* Lightweight timer */
  
namespace time {

#ifdef SPTL_USE_WALL_CLOCK

  using time_type = uint64_t;

  static inline
  time_type now() {
    struct timespec t;
    clock_gettime(CLOCK_REALTIME, &t);
    return t.tv_sec * 1000000000LL + t.tv_nsec;
  }

  // pre: cpu_frequency_ghz is initialized by sptl runtime
  // later: enforce the precondition by dynamic check
  static
  double microseconds_of(time_type cycles) {
    return (double)cycles / 1000.0;
  }

#else // (defaultly) use the cycle counter
  
  using time_type = cycle_counter::cycles_type;

  static inline
  time_type now() {
    return cycle_counter::now();
  }

  // pre: cpu_frequency_ghz is initialized by sptl runtime
  // later: enforce the precondition by dynamic check
  static
  double microseconds_of(time_type cycles) {
    double ticks_per_microsecond = cpu_frequency_ghz * 1000.0;
    return (double)cycles / ticks_per_microsecond;
  }

#endif

} // end namespace

/*---------------------------------------------------------------------*/
/* Atomic compare-and-exchange, with backoff */
  
namespace {
  
static constexpr
cycle_counter::cycles_type backoff_nb_cycles = 1l << 12;

static inline
void spin_for(cycle_counter::cycles_type nb_cycles) {
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

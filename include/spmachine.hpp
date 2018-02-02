
#include <assert.h>
#include <atomic>
#include <iostream>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef SPTL_HAVE_HWLOC
#include <hwloc.h>
#endif
#ifdef SPTL_TARGET_MAC_OS
#include <sys/param.h>
#include <sys/sysctl.h>
#endif

#include "cmdline.hpp"
#include "spcallback.hpp"
#include "splogging.hpp"

#ifndef _SPTL_MACHINE_H_
#define _SPTL_MACHINE_H_

namespace sptl {

/*---------------------------------------------------------------------*/
/* Runtime teardown */

namespace {
  
static
pthread_mutex_t print_lock;
  
void init_print_lock() {
  pthread_mutex_init(&print_lock, nullptr);
}

void acquire_print_lock() {
  pthread_mutex_lock (&print_lock);
}

void release_print_lock() {
  pthread_mutex_unlock (&print_lock);
}
  
} // end namespace
  
void die (const char *fmt, ...) {
  va_list	ap;
  va_start (ap, fmt);
  acquire_print_lock(); {
    fprintf (stderr, "Fatal error -- ");
    vfprintf (stderr, fmt, ap);
    fprintf (stderr, "\n");
    fflush (stderr);
  }
  release_print_lock();
  va_end(ap);
  assert(false);
  exit (-1);
}

/*---------------------------------------------------------------------*/
/* Hardware-specific configuration */

namespace {
  
#ifdef SPTL_HAVE_HWLOC
hwloc_topology_t topology;
#endif

void initialize_hwloc(int nb_workers, bool numa_alloc_interleaved = true) {
#ifdef SPTL_HAVE_HWLOC
  hwloc_topology_init(&topology);
  hwloc_topology_load(topology);
  if (numa_alloc_interleaved) {
    hwloc_cpuset_t all_cpus =
      hwloc_bitmap_dup(hwloc_topology_get_topology_cpuset(topology));
    int err = hwloc_set_membind(topology, all_cpus, HWLOC_MEMBIND_INTERLEAVE, 0);
    if (err < 0) {
      die("sptl: failed to set NUMA round-robin allocation policy\n");
    }
  }
#endif
}

} // end namespace

/*---------------------------------------------------------------------*/
/* CPU frequency */

namespace {
  
double cpu_frequency_ghz = 1.2;
  
void initialize_cpuinfo() {
  float cpu_frequency_mhz = 0.0;
#ifdef TARGET_LINUX
  /* Get information from /proc/cpuinfo.
   * cpu MHz         : <float>             # cpu frequency in MHz
   */
  FILE *cpuinfo_file = fopen("/proc/cpuinfo", "r");
  char buf[1024];
  int cache_line_szb;
  if (cpuinfo_file != NULL) {
    while (fgets(buf, sizeof(buf), cpuinfo_file) != 0) {
      sscanf(buf, "cpu MHz : %f", &(cpu_frequency_mhz));
    }
    fclose (cpuinfo_file);
  }
#endif
#ifdef TARGET_MAC_OS
  uint64_t freq = 0;
  size_t size;
  size = sizeof(freq);
  if (sysctlbyname("hw.cpufrequency", &freq, &size, NULL, 0) < 0) {
    perror("sysctl");
  }
  cpu_frequency_mhz = (float)freq / 1000000.;
#endif
  if (cpu_frequency_mhz == 0.) {
    die("Failed to read CPU frequency\n");
  }
  cpu_frequency_ghz = (double) (cpu_frequency_mhz / 1000.0);
}

/*---------------------------------------------------------------------*/
/* SPTL initialization */
  
int nb_proc = 1;

#ifdef SPTL_USE_CILK_PLUS_RUNTIME
  static
  long seq_fib (long n){
    if (n < 2) {
      return n;
    } else {
      return seq_fib (n - 1) + seq_fib (n - 2);
    }
  }
#endif
  
} // end namespace
  
double kappa = 100;

double update_size_ratio = 1.2; // aka alpha
  
template <class Body>
void launch(int argc, char** argv, const Body& body) {
  deepsea::cmdline::set(argc, argv);
  initialize_cpuinfo();
  callback::init();
  logging::buffer::init();
  kappa = deepsea::cmdline::parse_or_default_double("kappa", kappa);
  update_size_ratio = deepsea::cmdline::parse_or_default_double("alpha", update_size_ratio);
#ifdef SPTL_USE_CILK_PLUS_RUNTIME
  nb_proc = deepsea::cmdline::parse_or_default_int("proc", 1);
  __cilkrts_set_param("nworkers", std::to_string(nb_proc).c_str());
#endif
  bool numa_alloc_interleaved = (nb_proc == 1) ? false : true;
  numa_alloc_interleaved =
    deepsea::cmdline::parse_or_default_bool("numa_alloc_interleaved", numa_alloc_interleaved, false);
  initialize_hwloc(nb_proc, numa_alloc_interleaved);
#if defined(SPTL_USE_CILK_PLUS_RUNTIME)
  // hack that seems to be required to initialize cilk runtime cleanly
  cilk_spawn seq_fib(2);
  cilk_sync;
  body();
#else
  body();
#endif
  logging::buffer::output();
  callback::output();
  callback::destroy();
}
  
} // end namespace

#endif

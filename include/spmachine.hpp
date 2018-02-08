
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


#if defined(SPTL_USE_CILK_PLUS_RUNTIME)
#include <cilk/cilk.h>
#include <cilk/cilk_api.h>
#elif defined(SPTL_USE_FIBRIL)
extern "C" {
#include <fibril.h>
}
#endif

#include "cmdline.hpp"
#include "spcallback.hpp"
#include "splogging.hpp"

#ifndef _SPTL_MACHINE_H_
#define _SPTL_MACHINE_H_

namespace sptl {

/*---------------------------------------------------------------------*/
/* Global settings */

#ifdef SPTL_SIZE_BITS32
using size_type = unsigned;
#else
using size_type = std::size_t;
#endif

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
#ifdef SPTL_TARGET_LINUX
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
#ifdef SPTL_TARGET_MAC_OS
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
  
int nb_proc = -1;

#ifdef SPTL_USE_CILK_PLUS_RUNTIME
static
long ___silly_cilk (long n){
  if (n < 2) {
    return n;
  } else {
    return ___silly_cilk (n - 1) + ___silly_cilk (n - 2);
  }
}
#endif
  
} // end namespace
  
double kappa = 100;

double update_size_ratio = 1.2; // aka alpha

template <class Body>
void _launch(int argc, char** argv, const Body& body) {
#if defined(SPTL_USE_FIBRIL)
  fibril_rt_init(nb_proc);
#endif
  initialize_cpuinfo();
  callback::init();
  logging::buffer::init();
  kappa = deepsea::cmdline::parse_or_default_double("sptl_kappa", kappa);
  update_size_ratio = deepsea::cmdline::parse_or_default_double("sptl_alpha", update_size_ratio);
  bool numa_alloc_interleaved = (nb_proc == 1) ? false : true;
  numa_alloc_interleaved =
    deepsea::cmdline::parse_or_default_bool("numa_alloc_interleaved", numa_alloc_interleaved, false);
  initialize_hwloc(nb_proc, numa_alloc_interleaved);
#if defined(SPTL_USE_CILK_PLUS_RUNTIME)
  // hack that seems to be required to initialize cilk runtime cleanly
  cilk_spawn ___silly_cilk(2);
  cilk_sync;
#endif
  body();
  logging::buffer::output();
  callback::output();
  callback::destroy();
#ifdef SPTL_USE_FIBRIL
    fibril_rt_exit();
#endif
}

template <class Body>
void launch(int argc, char** argv, const Body& body) {
  deepsea::cmdline::set(argc, argv);
  if (nb_proc == -1) {
    nb_proc = deepsea::cmdline::parse_or_default_int("sptl_proc", -1);
    nb_proc = std::max(1, nb_proc); // nb_proc = 1, defaultly
  }
#if defined(SPTL_USE_CILK_PLUS_RUNTIME)
  // this operation will fail if it is called inside the same funcion call that
  // first initializes Cilk...
  int cilk_failed = __cilkrts_set_param("nworkers", std::to_string(nb_proc).c_str());
  if (cilk_failed) {
    die("Failed to set number of processors to %d in Cilk runtime", nb_proc);
  }
#endif
  _launch(argc, argv, body);
}
  
template <class Body>
void launch(int argc, char** argv, unsigned nb_workers, const Body& body) {
  nb_proc = nb_workers;
  sptl::launch(argc, argv, body);
}

  
} // end namespace

#endif

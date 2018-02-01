
#include <chrono>
#include <vector>
#include <string>
#include <cstdlib>
#include <cstdio>
#include <algorithm>
#include <memory>

#include "spperworker.hpp"

#ifndef _SPTL_PERWORKER_
#define _SPTL_PERWORKER_

namespace sptl {
namespace logging {

using event_tag_type = enum {
  sequential_run,
  measured_run
};
  
using event_type = struct {
  event_tag_type tag;
  double timestamp;
  union {
    struct {
      const char* name;
      double size;
      double time;
    } sequential_run;
    struct {
      const char* name;
      double size;
      double time;
    } measured_run;
  } u;
};

using time_point_type = std::chrono::time_point<std::chrono::system_clock>;
    
template <bool enabled>
class _buffer {
public:

  static
  time_point_type basetime;

  static
  perworker::array<std::vector<event_type>> buf;

  static
  void init() {
    basetime = std::chrono::system_clock::now();
  }

  static inline
  void push(event_type e) {
    if (! enabled) {
      return;
    }
    std::chrono::duration<double> elapsed = std::chrono::system_clock::now() - basetime;
    e.timestamp = elapsed.count() * 1000000;
    buf.mine().push_back(e);
  }

  static inline
  void push_sequential_run(const char* name, double size, double time) {
    event_type e;
    e.tag = sequential_run;
    e.u.sequential_run.name = name;
    e.u.sequential_run.size = size;
    e.u.sequential_run.time = time;
    push(e);
  }

  static inline
  void push_measured_run(const char* name, double size, double time) {
    event_type e;
    e.tag = measured_run;
    e.u.measured_run.name = name;
    e.u.measured_run.size = size;
    e.u.measured_run.time = time;
    push(e);
  }

  static inline
  void print_text(FILE* f, event_type e) {
    switch (e.tag) {
      case sequential_run: {
        fprintf(f, "sequential_run\t%s\t%f\t%f",
                e.u.sequential_run.name,
                e.u.sequential_run.size,
                e.u.sequential_run.time);
        break;
      }
      case measured_run: {
        fprintf(f, "measured_run\t%s\t%f\t%f",
                e.u.measured_run.name,
                e.u.measured_run.size,
                e.u.measured_run.time);
        break;
      }
    }
    fprintf(f, "\n");
  }

  static
  void output_text(std::vector<event_type>& b) {
    std::string fname = deepsea::cmdline::parse_or_default_string("sptl_log_text", "");
    if (fname == "") {
      return;
    }
    FILE* f = fopen(fname.c_str(), "w");
    for (auto e : b) {
      print_text(f, e);
    }
    fclose(f);
  }

  static
  void output() {
    std::vector<event_type> out;
    buf.iterate([&] (std::vector<event_type>& b) {
      for (auto e : b) {
        out.push_back(e);
      }
    });
    std::stable_sort(out.begin(), out.end(), [] (const event_type& e1, const event_type& e2) {
      return e1.timestamp < e2.timestamp;
    });
    output_text(out);
  }
  
};

template <bool enabled>
perworker::array<std::vector<event_type>> _buffer<enabled>::buf;

template <bool enabled>
time_point_type _buffer<enabled>::basetime;

#ifdef SPTL_ENABLE_LOGGING
using buffer = _buffer<true>;
#else
using buffer = _buffer<false>;
#endif
  
} // end namespace
} // end namespace

#endif

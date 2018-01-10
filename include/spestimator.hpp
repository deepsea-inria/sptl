
#include <execinfo.h>
#include <string>
#include <atomic>
#include <map>
#include <sstream>
#include <stdio.h>
#include <iostream>

#include "spperworker.hpp"
#include "sptime.hpp"

#ifndef _SPTL_SPESTIMATOR_H_
#define _SPTL_SPESTIMATOR_H_

namespace sptl {

using complexity_type = double;
  
using cost_type = double;

/*---------------------------------------------------------------------*/
/* Special cost values */

namespace cost {

  //! an `undefined` execution time indicates that the value hasn't been computed yet
  static constexpr
  cost_type undefined = -1.0;

  //! a `pessimistic` cost is 1 microsecond per unit of complexity
  static constexpr
  cost_type pessimistic = std::numeric_limits<double>::infinity();

} // end namespace

/*---------------------------------------------------------------------*/
/* File IO for reading/writing estimator values */
  
namespace {

using constant_map_type = std::map<std::string, double>;
  
// values of constants which are read from a file
static
constant_map_type preloaded_constants;
  
// values of constants which are to be written to a file
static
constant_map_type recorded_constants;

static
void print_constant(FILE* out, std::string name, double cst) {
  fprintf(out,         "%s %lf\n", name.c_str(), cst);
}

static
void parse_constant(char* buf, double& cst, std::string line) {
  sscanf(line.c_str(), "%s %lf", buf, &cst);
}

static
std::string get_dflt_constant_path() {
//  std::string executable = deepsea::cmdline::name_of_my_executable();
//  return executable + ".cst";
  return "constants.txt";
}

static
std::string get_path_to_constants_file_from_cmdline(std::string flag) {
  std::string outfile;
  if (deepsea::cmdline::parse_or_default_bool(flag, false, false)) {
    return get_dflt_constant_path();
  } else {
    return deepsea::cmdline::parse_or_default_string(flag + "_in", "", false);
  }
}

static
bool loaded = false;

static
void try_read_constants_from_file() {
  if (loaded) {
    return;
  }
  loaded = true;
  std::string infile_path = get_dflt_constant_path();
  if (infile_path == "") {
    return;
  }
  std::string cst_str;
  std::ifstream infile;
  infile.open (infile_path.c_str());
  if (!infile.good()) {
    return;
  }
  std::cerr << "Load constants from constants.txt\n";
  while(! infile.eof()) {
    getline(infile, cst_str);
    if (cst_str == "") {
      continue; // ignore trailing whitespace
    }
    char buf[4096];
    double cst;
    parse_constant(buf, cst, cst_str);
    std::string name(buf);
    preloaded_constants[name] = cst;
  }
}

static
void try_write_constants_to_file() {
  std::string outfile_path = get_path_to_constants_file_from_cmdline("write_csts");
  if (outfile_path == "")
    return;
  static FILE* outfile;
  outfile = fopen(outfile_path.c_str(), "w");
  constant_map_type::iterator it;
  for (it = recorded_constants.begin(); it != recorded_constants.end(); it++) {
    print_constant(outfile, it->first, it->second);
  }
  fclose(outfile);
}

} // end namespace

/*---------------------------------------------------------------------*/
/* The estimator data structure */
  
cost_type kappa = 100;

double update_size_ratio = 1.5; // aka alpha

template <class Item>
using perworker_type = perworker::array<Item>;

class estimator : public callback::client {
//private:
public:

  constexpr static
  const double min_report_shared_factor = 2.0;
  
  constexpr static
  const double weighted_average_factor = 8.0;

  cost_type shared;

  perworker_type<cost_type> privates;

  std::string name;    

  // 5 cold runs
  constexpr static const
  int number_of_cold_runs = 5;
  
  bool estimated;
  
  perworker_type<double> first_estimation;
  
  perworker_type<int> estimations_left;

  constexpr static const
  long long cst_mask = (1LL << 32) - 1;
  
  char padding[108];
  
  std::atomic_llong shared_info;

  cost_type get_constant() {
    int cst_int = (shared_info.load() & cst_mask);
    cost_type cst = *((float*)(&cst_int));
    // else return local constant
    return cst;
  }
  
  cost_type get_constant_or_pessimistic() {
    int cst_int = (shared_info.load() & cst_mask);
    cost_type cst = *((float*)(&cst_int));
    if (cst_int == 0) {
      return cost::pessimistic;
    } else {
      return cst;
    }
  }

  static constexpr
  int backoff_nb_cycles = 1l << 17;
  
  static inline
  void spin_for(uint64_t nb_cycles) {
    cycle_counter::rdtsc_wait(nb_cycles);
  }
  
  template <class T>
  bool compare_exchange(std::atomic<T>& cell, T& expected, T desired) {
    if (cell.compare_exchange_strong(expected, desired)) {
      return true;
    }
    spin_for(backoff_nb_cycles);
    return false;
  }

  using info_loader = union {
    struct { float size, cst; } f;
    long long l;
  };

  void update(cost_type new_cst_d, complexity_type new_size_d) {
    info_loader new_info;
    new_info.f.cst = (float) new_cst_d;
    new_info.f.size = (float) new_size_d;

    info_loader info;
    info.l = shared_info.load();

    while (true) {
      if (info.f.size < new_info.f.size) {
        if (compare_exchange(shared_info, info.l, new_info.l)) {
          break;
        }
      } else {
        break;
      }
    }
  }
  
//public:
  
  void init() {
    shared = cost::undefined;
    privates.init(cost::undefined);
    estimated = false;
    estimations_left.init(5);
    first_estimation.init(std::numeric_limits<double>::max());
    
    try_read_constants_from_file();
    
    constant_map_type::iterator preloaded = preloaded_constants.find(get_name());
    if (preloaded != preloaded_constants.end()) {
      estimator::estimated = true;
      shared = preloaded->second;
    }
  }
  
  void output() {
    recorded_constants[name] = estimator::get_constant();
  }
  
  void destroy() {

  }

  estimator() : shared_info(0) {
    init();
  }

  estimator(std::string name) : shared_info(0) {
    std::stringstream stream;
    stream << name.substr(0, std::min(40, (int)name.length())) << this;
    this->name = stream.str();
    init();
    callback::register_client(this);
  }

  std::string get_name() {
    return name;
  }

  bool is_undefined() {
    return shared_info.load() == 0;
  }

  void report(complexity_type complexity, cost_type elapsed) {
    double local_ticks_per_microsecond = machine::cpu_frequency_ghz * 1000.0;
    double elapsed_time = elapsed / local_ticks_per_microsecond;
    cost_type measured_cst = elapsed_time / complexity;    
    if (elapsed_time > kappa) {
      return;
    }
    update(measured_cst, complexity);
  }
  
  cost_type predict(complexity_type complexity) {
    info_loader info;
    info.l = shared_info.load();
    if (complexity > update_size_ratio * info.f.size) { // was 2
      return kappa + 1;
    }
    if (complexity <= info.f.size) {
      return kappa - 1;
    }
    return info.f.cst * ((double) complexity) / update_size_ratio; // allow kappa * alpha runs
  }
  
};
  
} // end namespace


#endif

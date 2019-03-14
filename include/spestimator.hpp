
#include <string>
#include <sstream>

#include "spperworker.hpp"
#include "sptime.hpp"
#include "spmachine.hpp"
#include "splogging.hpp"

#ifndef _SPTL_SPESTIMATOR_H_
#define _SPTL_SPESTIMATOR_H_

namespace sptl {

// type of the result of a complexity function
using complexity_type = long;

/*---------------------------------------------------------------------*/
/* The estimator data structure */
  
class estimator {
private:
  
  char padding1[128]; // to protect from false sharing

  // initially, contents set to zero, indicating the
  // undefined configuration
  std::atomic_long nmax;

  char padding2[128]; // to protect from false sharing

  std::string name;
    
public:

  estimator(std::string name) : nmax(1) {
    std::stringstream stream;
    stream << name.substr(0, std::min(40, (int)name.length())) << this;
    this->name = stream.str();
  }

  const char* get_name() {
    return name.c_str();
  }

  void report(complexity_type complexity, time::time_type elapsed) {
    double elapsed_us = time::microseconds_of(elapsed);
    if (elapsed_us > kappa) {
      return;
    }
    auto measured_cst = elapsed_us / std::max((complexity_type)1, complexity);
    complexity_type cost = complexity;
    while (true) {
      complexity_type current = nmax.load();
      if (cost <= current) {
	break;
      }
      if (compare_exchange_with_backoff(nmax, current, cost)) {
	logging::buffer::push_estimator_update(get_name(),
					       0, (float)cost,
					       0, (float)current);
	break;
      }
    }
  }

  bool is_small(complexity_type complexity) {
    return complexity <= update_size_ratio * nmax.load();
  }
  
};
  
} // end namespace


#endif

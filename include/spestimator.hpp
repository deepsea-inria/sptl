
#include <string>
#include <sstream>

#include "spperworker.hpp"
#include "sptime.hpp"
#include "spmachine.hpp"

#ifndef _SPTL_SPESTIMATOR_H_
#define _SPTL_SPESTIMATOR_H_

namespace sptl {

// type of the result of a complexity function
using complexity_type = double;

// type to represent an amount of time in number of cycles;
// this type is used internally by the estimator to compare predicted
// running times
using cost_type = double;

/*---------------------------------------------------------------------*/
/* Cost measurement */

namespace cost {

  // an undefined execution time indicates that the value hasn't been computed yet
  static constexpr
  cost_type undefined = -1.0;

  // a pessimistic cost is 1 microsecond per unit of complexity
  static constexpr
  cost_type pessimistic = std::numeric_limits<double>::infinity();

} // end namespace


/*---------------------------------------------------------------------*/
/* The estimator data structure */
  
class estimator {
private:

  // we use this union to represent the internal state
  // of the estimator cell; the field named f is the
  // view of the internal state of the estimator cell,
  // and the field l a view that can be updated atomically
  // via the compare-and-exchange instruction
  using cell_type = union {
    struct {
      float cst;  // constant for estimations
      float nmax; // max complexity measure
    } f;
    long long l;
  };
  
  char padding1[128]; // to protect from false sharing

  // initially, contents set to zero, indicating the
  // undefined configuration
  std::atomic_llong cell;

  char padding2[128]; // to protect from false sharing

  std::string name;
    
public:
  
  estimator(std::string name) : cell(0) {
    std::stringstream stream;
    stream << name.substr(0, std::min(40, (int)name.length())) << this;
    this->name = stream.str();
  }

  std::string get_name() {
    return name;
  }

  bool is_undefined() {
    return cell.load() == 0;
  }

  void report(complexity_type complexity, cost_type elapsed) {
    double elapsed_us = microseconds_of_cycles(elapsed);
    if (elapsed_us > kappa) {
      return;
    }
    complexity = std::max((complexity_type)1, complexity);
    cost_type measured_cst = elapsed_us / complexity;
    cell_type proposed;
    proposed.f.cst = (float)measured_cst;
    proposed.f.nmax = (float)complexity;
    cell_type current;
    current.l = cell.load();
    while (true) {
      if (proposed.f.nmax > current.f.nmax) {
	if (compare_exchange_with_backoff(cell, current.l, proposed.l)) {
	  break;
	}
      } else {
	break;
      }
    }
  }

  bool is_small(complexity_type complexity) {
    assert(! is_undefined());
    complexity = std::max((complexity_type)1, complexity);
    cell_type current;
    current.l = cell.load();
    auto cst = current.f.cst;
    auto nmax = current.f.nmax;
    auto alpha = update_size_ratio;
    return (complexity <= nmax) || ((complexity <= alpha * nmax) && (complexity * cst <= alpha * kappa));
  }
  
};
  
} // end namespace


#endif

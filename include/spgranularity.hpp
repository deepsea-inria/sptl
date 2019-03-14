
#include "spestimator.hpp"
#include "splogging.hpp"

#ifndef _SPTL_GRANULARITY_H_
#define _SPTL_GRANULARITY_H_

namespace sptl {

/*---------------------------------------------------------------------*/
/* Series-parallel guard */
  
namespace {

using time_type = time::time_type;
    
perworker::array<time_type> timer(0);

perworker::array<time_type> total(0);

perworker::array<bool> is_small(false);

static
time_type total_now(time_type t) {
  return total.mine() + (time::now() - t);
}

template <class Func>
time_type measured_run(const Func& f) {
  total.mine() = 0;
  timer.mine() = time::now();
  f();
  return total_now(timer.mine());
}
                    
template <
  class Complexity,
  class Par_body,
  class Seq_body
  >
void _spguard(estimator& estim,
              const Complexity& complexity,
              const Par_body& par_body,
              const Seq_body& seq_body) {
#ifdef SPTL_USE_SEQUENTIAL_ELISION_RUNTIME
  if ((complexity_type)complexity() < 10000) {
    seq_body();
  } else {
    par_body();
  }
  return;
#endif
  if (is_small.mine()) {
    seq_body();
    return;
  }
  complexity_type comp = (complexity_type)complexity();
  if (estim.is_small(comp)) {
    is_small.mine() = true;
    auto t = time::now();
    seq_body();
    auto elapsed = time::now() - t;
    estim.report(comp, elapsed);
    is_small.mine() = false;
    logging::buffer::push_sequential_run(estim.get_name(), (float)comp, elapsed);
  } else {
    auto t_before = total_now(timer.mine());
    auto t_body = measured_run(par_body);
    estim.report(comp, t_body);
    total.mine() = t_before + t_body;
    timer.mine() = time::now();
  }
}  

template <class Last>
std::string type_name() {
  return std::string(typeid(Last).name());
}

template <class First, class Second, class ... Types>
std::string type_name() {
  return type_name<First>() + "_" + type_name<Second, Types...>();
}

template <const char* ename, class ... Types>
class estim_wrp {
public:

  static
  estimator e;
  
};

template <const char* ename, class ... Types>
estimator estim_wrp<ename, Types ...>::e("" + std::string(ename) + "_" + type_name<Types ...>());

char dflt_ename[] = "_";

} // end namespace

template <
  const char* ename,
  class Complexity,
  class Par_body,
  class Seq_body
  >
void spguard(const Complexity& complexity,
             const Par_body& par_body,
             const Seq_body& seq_body) {
  using wrapper = estim_wrp<ename, Complexity, Par_body, Seq_body>;
  _spguard(wrapper::e, complexity, par_body, seq_body);
}
  
template <
  class Complexity,
  class Par_body,
  class Seq_body
  >
void spguard(const Complexity& complexity,
             const Par_body& par_body,
             const Seq_body& seq_body) {
  spguard<dflt_ename>(complexity, par_body, seq_body);
}

template <
  const char* ename,
  class Complexity,
  class Par_body
  >
void spguard(const Complexity& complexity,
             const Par_body& par_body) {
  using wrapper = estim_wrp<ename, Complexity, Par_body, Par_body>;
  _spguard(wrapper::e, complexity, par_body, par_body);
}
  
template <
  class Complexity,
  class Par_body
  >
void spguard(const Complexity& complexity,
             const Par_body& par_body) {
  spguard<dflt_ename>(complexity, par_body, par_body);
}
  
/*---------------------------------------------------------------------*/
/* Fork join */

namespace {

#ifdef SPTL_USE_FIBRIL
template <class Fct>
__attribute__((noinline, hot, optimize(3)))
void my_fibril_fork(const Fct& fct, fibril_t * f) {
  fibrili_push(f);
  fct();
  if (!fibrili_pop()) fibrili_resume(f);
}
#endif
    
template <class Body_fct1, class Body_fct2>
void primitive_fork2(const Body_fct1& f1, const Body_fct2& f2) {
#if defined(SPTL_USE_CILK_PLUS_RUNTIME)
  cilk_spawn f1();
  f2();
  cilk_sync;
#elif defined(SPTL_USE_FIBRIL)
  fibril_t fr;
  fibril_init(&fr);
  fibrili_membar(my_fibril_fork(f1, &fr));
  f2();
  fibril_join(&fr);
#else // if defined(SPTL_USE_SEQUENTIAL_ELISION_RUNTIME)
  f1();
  f2();
#endif
}

} // end namespace
  
template <class Body_left, class Body_right>
void fork2(const Body_left& bl, const Body_right& br) {
#ifdef SPTL_USE_SEQUENTIAL_ELISION_RUNTIME
  bool sequentialize = true;
#else
  bool sequentialize = is_small.mine();
#endif
  if (sequentialize) {
    bl();
    br();
    return;
  }
  auto t_before = total_now(timer.mine());
  time_type t_left, t_right;
  primitive_fork2([&] {
    t_left = measured_run(bl);
  }, [&] {
    t_right = measured_run(br);
  });
  total.mine() = t_before + t_left + t_right;
  timer.mine() = time::now();
}

} // end namespace

#endif


#include "spestimator.hpp"

#ifndef _SPTL_GRANULARITY_H_
#define _SPTL_GRANULARITY_H_

namespace sptl {

  namespace granularity {

    /*---------------------------------------------------------------------*/
    /* Granularity-control policies */

    class control_by_force_parallel {
    public:
      
      control_by_force_parallel(std::string) { }
      
    };
    
    class control_by_force_sequential {
    public:
      
      control_by_force_sequential(std::string) { }
      
    };
    
    class control_by_prediction {
    public:
      
      estimator e;
      
      control_by_prediction(std::string name = ""): e(name) { }
      
      estimator& get_estimator() {
        return e;
      }
      
    };
    
    /*---------------------------------------------------------------------*/
    /* Dynamic scope */

    namespace {
      
    template <class Item>
    class dynidentifier {
    private:
      
      Item bk;
      
    public:
      
      dynidentifier() {};
      
      dynidentifier(const Item& bk_) : bk(bk_) {};

      Item& back() {
        return bk;
      }

      template <class Block_fct>
      void block(Item x, const Block_fct& f) {
        Item tmp = bk;
        bk = x;
        f();
        bk = tmp;
      }
    };

    } // end namespace
    
    /*---------------------------------------------------------------------*/
    /* Execution mode */

    namespace {

    using execmode_type = enum {
      Force_sequential = 0,
      Force_parallel = 1,
      Sequential = 2,
      Parallel = 3,
      Unknown = 4
    };
    
    template <class Item>
    using perworker_type = perworker::array<Item>;

    perworker_type<dynidentifier<execmode_type>> execmode(Parallel);

    perworker_type<long long> timer(0);
    
    perworker_type<cost_type> work(0);

    static inline
    execmode_type& my_execmode() {
      return execmode.mine().back();
    }

    template <class Body_fct>
    void run(execmode_type c, const Body_fct& body_fct) {
      execmode.mine().block(c, body_fct);
    }
    
    template <class Seq_body_fct>
    void run_and_report(complexity_type m,
			const Seq_body_fct& seq_body_fct,
			estimator& estimator) {
      auto start = cycle_counter::now();
      run(Sequential, seq_body_fct);
      auto elapsed = cycle_counter::since(start);
      estimator.report(std::max((complexity_type)1, m), elapsed);
    }
            
    template <
      class Complexity_measure_fct,
      class Par_body_fct,
      class Seq_body_fct
      >
    void spguard(control_by_prediction& contr,
                 const Complexity_measure_fct& complexity_measure_fct,
                 const Par_body_fct& par_body_fct,
                 const Seq_body_fct& seq_body_fct) {
#ifdef SPTL_SEQUENTIAL_BASELINE
      seq_body_fct();
      return;
#endif
#if defined(SPTL_SEQUENTIAL_ELISION)
      par_body_fct();
      return;
#endif
      execmode_type p = my_execmode();
      if ((p == Sequential) || (p == Force_Sequential)) {
        run(Sequential, seq_body_fct);
        return;
      }
      if (p == Force_parallel) {
        run(Parallel, par_body_fct);
        return;
      }
      estimator& estim = contr.get_estimator();
      execmode_type c = (estim.is_undefined()) ? Unknown : p;
      complexity_type m = complexity_measure_fct();
      if (c == Parallel) {
        complexity_type comp = std::max((complexity_type)1, m);
        cost_type pred = estim.predict(comp);
        c = (pred <= kappa) ? Sequential : Parallel;
      }
      if (c == Sequential) {
        run_and_report(m, seq_body_fct, estim);
      } else if (c == Parallel) {
        run(Parallel, par_body_fct);
      } else if (c == Unknown) {
        cost_type upper_work = work.mine() + wall_clock::since(timer.mine());
        work.mine() = 0;
        timer.mine() = wall_clock::now();
        run(Unknown, body_fct);
        work.mine() += wall_clock::since(timer.mine());
        estimator.report(std::max((complexity_type) 1, m), work.mine());
        work.mine() += upper_work;
        timer.mine() = wall_clock::now();
      } else {
        assert(false);
      }
    }

    template <
      class Complexity_measure_fct,
      class Par_body_fct
      >
    void spguard(control_by_prediction& contr,
                 const Complexity_measure_fct& complexity_measure_fct,
                 const Par_body_fct& par_body_fct) {
      spguard(contr, complexity_measure_fct, par_body_fct, par_body_fct);
    }

    template <class Last>
    std::string type_name() {
      return std::string(typeid(Last).name());
    }
    
    template <class First, class Second, class ... Types>
    std::string type_name() {
      return type_name<First>() + "_" + type_name<Second, Types...>();
    }
    
    template <const char* estim_name, class ... Types>
    class controller_holder {
    public:
      static control_by_prediction controller;
    };
    
    template <const char* estim_name, class ... Types>
    control_by_prediction controller_holder<estim_name, Types ...>::controller(std::string("controller_holder_") + std::string(estim_name) + "_" + "_" + type_name<Types ...>());
    
    static constexpr
    char dflt_estim_name[] = "auto";

  } // end namespace
  } // end namespace
  
/*---------------------------------------------------------------------*/
/* Series-parallel guard */

template <
  class Complexity_measure_fct,
  class Par_body_fct,
  class Seq_body_fct
  >
void spguard(const Complexity_measure_fct& complexity_measure_fct,
             const Par_body_fct& par_body_fct,
             const Seq_body_fct& seq_body_fct) {
  using controller_type = controller_holder<dflt_estim_name, Complexity_measure_fct, Par_body_fct, Seq_body_fct>;
  spguard(controller_type::controller, complexity_measure_fct, par_body_fct, seq_body_fct);
}

template <
  class Complexity_measure_fct,
  class Par_body_fct
  >
void spguard(const Complexity_measure_fct& complexity_measure_fct,
 const Par_body_fct& par_body_fct) {
  using controller_type = controller_holder<dflt_estim_name, Complexity_measure_fct, Par_body_fct>;
  spguard(controller_type::controller, complexity_measure_fct, par_body_fct);
}

template <class Par_body_fct>
void spguard(control_by_force_parallel&, const Par_body_fct& par_body_fct) {
  run(Force_parallel, par_body_fct);
}

template <
  class Complexity_measure_fct,
  class Par_body_fct,
  class Seq_body_fct
  >
void spguard(control_by_force_parallel& contr,
             const Complexity_measure_fct&,
             const Par_body_fct& par_body_fct,
             const Seq_body_fct&) {
  spguard(contr, par_body_fct);
}

template <class Seq_body_fct>
void spguard(control_by_force_sequential&, const Seq_body_fct& seq_body_fct) {
  run(Force_sequential, seq_body_fct);
}

// same as above but accepts all arguments to support general case
template <
  class Complexity_measure_fct,
  class Par_body_fct,
  class Seq_body_fct
  >
void spguard(control_by_force_sequential& contr,
             const Complexity_measure_fct&,
             const Par_body_fct&,
             const Seq_body_fct& seq_body_fct) {
  spguard(contr, seq_body_fct);
}

/*---------------------------------------------------------------------*/
/* Granularity-control enriched fork join */

namespace {
    
template <class Body_fct1, class Body_fct2>
void primitive_fork2(const Body_fct1& f1, const Body_fct2& f2) {
#if defined(SPTL_USE_CILK_PLUS_RUNTIME)
  cilk_spawn f1();
  f2();
  cilk_sync;
#else
  f1();
  f2();
#endif
}

} // end namespace
  
template <class Body_fct1, class Body_fct2>
void fork2(const Body_fct1& f1, const Body_fct2& f2) {
#if defined(SPTL_SEQUENTIAL_ELISION) || defined(SPTL_SEQUENTIAL_BASELINE)
  f1();
  f2();
  return;
#endif
#if defined(SPTL_PARALLEL_ELISION)
  primitive_fork2(f1, f2);
  return;
#endif
  execmode_type c = my_execmode();
  if ((c == Sequential) || (c == Force_sequential)) {
    f1();
    f2();
  } else if ((c == Parallel) || (c == Force_parallel)) {
    primitive_fork(f1, f2);
  } else if (c == Unknown) {
    cost_type upper_work = work.mine() + wall_clock::since(timer.mine());
    work.mine() = 0;
    cost_type left_work, right_work;
    primitive_fork2([&] {
        work.mine() = 0;
        timer.mine() = wall_clock::now();
        run(c, f1);
        left_work = work.mine() + wall_clock::since(timer.mine());
      }, [&] {
        work.mine() = 0;
        timer.mine() = wall_clock::now();
        run(c, f2);
        right_work = work.mine() + wall_clock::since(timer.mine());
      });
    work.mine() = upper_work + left_work + right_work;
    timer.mine() = wall_clock::now();
  }
}

} // end namespace

#endif

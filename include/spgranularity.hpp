
#include "spestimator.hpp"

#ifndef _SPTL_GRANULARITY_H_
#define _SPTL_GRANULARITY_H_

namespace sptl {

  namespace granularity {

    /*---------------------------------------------------------------------*/
    /* Granularity-control policies */
    
    class control {};
    
    class control_by_force_parallel : public control {
    public:
      control_by_force_parallel(std::string) { }
    };
    
    class control_by_force_sequential : public control {
    public:
      control_by_force_sequential(std::string) { }
    };
    
    class control_by_prediction : public control {
    public:
      estimator e;
      
      control_by_prediction(std::string name = ""): e(name) { }
      
      estimator& get_estimator() {
        return e;
      }
      
    };
    
    /*---------------------------------------------------------------------*/
    /* Dynamic scope */

    template <class Item>
    class dynidentifier {
    private:
      
      Item bk;
      
    public:
      
      dynidentifier() {};
      
      dynidentifier(Item& bk_) : bk(bk_) {};

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

    /*---------------------------------------------------------------------*/
    /* Execution mode */

    // names of configurations supported by the granularity controller
    using execmode_type = enum {
      Force_parallel = 0,
      Force_sequential = 1,
      Sequential = 2,
      Parallel = 3,
      Unknown = 4
    };
    
    template <class Item>
    using perworker_type = perworker::array<Item>;

    perworker_type<long long> timer(0);
    
    perworker_type<cost_type> work(0);

    perworker_type<dynidentifier<execmode_type>> execmode;

    static inline
    execmode_type& my_execmode() {
      return execmode.mine().back();
    }

    // `p` configuration of caller; `c` callee
    static inline
    execmode_type execmode_combine(execmode_type p, execmode_type c) {
      // callee gives priority to caller when caller is Sequential
      if (p == Sequential) {
        return Sequential;
      }
      
      // otherwise, callee takes priority
      return c;
    }

    /*---------------------------------------------------------------------*/
    /* Fork-join primitive */

    template <class Body_fct1, class Body_fct2>
    void primitive_fork2(const Body_fct1& f1, const Body_fct2& f2) {
#if defined(USE_CILK_PLUS_RUNTIME)
      cilk_spawn f1();
      f2();
      cilk_sync;
#else
      f1();
      f2();
#endif
    }

    /*---------------------------------------------------------------------*/
    /* Series-parallel guard */

    template <class Body_fct>
    void spguard_sequential(execmode_type c, const Body_fct& body_fct) {
      execmode_type p = my_execmode();
      execmode_type e = execmode_combine(p, c);
      execmode.mine().block(e, body_fct);
    }

    template <class Body_fct>
    void spguard_parallel(execmode_type c, const Body_fct& body_fct) {
      execmode.mine().block(c, body_fct);
    }
    
    template <class Body_fct>
    void spguard_unknown(execmode_type c,
                         complexity_type m,
                         Body_fct& body_fct,
                         estimator& estimator) {
      cost_type upper_work = work.mine() + wall_clock::since(timer.mine());
      work.mine() = 0;
      timer.mine() = wall_clock::now();
      execmode.mine().block(c, body_fct);
      work.mine() += wall_clock::since(timer.mine());
      estimator.report(std::max((complexity_type) 1, m), work.mine(), estimator.is_undefined());
      work.mine() = upper_work + work.mine();
      timer.mine() = wall_clock::now();
    }

    template <class Seq_body_fct>
    void spguard_sequential_with_reporting(complexity_type m,
                                           const Seq_body_fct& seq_body_fct,
                                           estimator& estimator) {
      auto start = cycle_counter::now();
      execmode.mine().block(Sequential, seq_body_fct);
      auto elapsed = cycle_counter::since(start);
      estimator.report(std::max((complexity_type)1, m), elapsed);
    }
    
    template <
      class Complexity_measure_fct,
      class Par_body_fct
      >
    void spguard(control& contr,
                 const Complexity_measure_fct&,
                 const Par_body_fct& par_body_fct) {
      spguard_sequential(Force_parallel, par_body_fct);
    }

    template <class Par_body_fct>
    void spguard(control_by_force_parallel&, const Par_body_fct& par_body_fct) {
      spguard_parallel(Force_parallel, par_body_fct);
    }

    // same as above but accepts all arguments to support general case
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
      spguard_sequential(Force_sequential, seq_body_fct);
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
    
    template <
      class Seq_complexity_measure_fct,
      class Par_complexity_measure_fct,
      class Par_body_fct,
      class Seq_body_fct
      >
    void spguard(control_by_prediction& contr,
                 const Par_complexity_measure_fct& par_complexity_measure_fct,
                 const Seq_complexity_measure_fct& seq_complexity_measure_fct,
                 const Par_body_fct& par_body_fct,
                 const Seq_body_fct& seq_body_fct) {
#ifdef SPTL_SEQUENTIAL_BASELINE
      seq_body_fct();
      return;
#endif
#if defined(SPTL_SEQUENTIAL_ELISION) || defined(SPTL_PARALLEL_ELISION)
      par_body_fct();
      return;
#endif
      estimator& estimator = contr.get_estimator();
      complexity_type m = seq_complexity_measure_fct();
      cost_type predicted;
      execmode_type c;
      if (estimator.is_undefined()) {
        c = Parallel;
      } else {
        if (my_execmode() == Sequential) {
          execmode.mine().block(Sequential, seq_body_fct);
          return;
        }
        if (m == complexity::tiny) {
          c = Sequential;
        } else if (m == complexity::undefined) {
          c = Parallel;
        } else {
          complexity_type comp = std::max((complexity_type)1, m);
          predicted = estimator.predict(comp);
          if (predicted <= kappa) {
            c = Sequential;
          } else {
            c = Parallel;
          }
        }
      }
      c = execmode_combine(my_execmode(), c);
      if (c == Sequential) {
        spguard_sequential_with_reporting(m, seq_body_fct, estimator);
      } else {
        spguard_unknown(c, par_complexity_measure_fct(), par_body_fct, estimator);
      }
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
#if defined(SPTL_SEQUENTIAL_ELISION) || defined(SPTL_PARALLEL_ELISION)
      par_body_fct();
      return;
#endif
      estimator& estimator = contr.get_estimator();
      complexity_type m = complexity_measure_fct();
      cost_type predicted;
      execmode_type c;
      if (estimator.is_undefined()) {
        c = Parallel;
      } else {
        if (my_execmode() == Sequential) {
          execmode.mine().block(Sequential, seq_body_fct);
          return;
        }
        if (m == complexity::tiny) {
          c = Sequential;
        } else if (m == complexity::undefined) {
          c = Parallel;
        } else {
          complexity_type comp = std::max((complexity_type)1, m);
          predicted = estimator.predict(comp);
          if (predicted <= kappa) {
            c = Sequential;
        } else {
            c = Parallel;
          }
        }
      }
      c = execmode_combine(my_execmode(), c);
      if (c == Sequential) {
        spguard_sequential_with_reporting(m, seq_body_fct, estimator);
      } else {
        spguard_unknown(c, m, par_body_fct, estimator);
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
    
    template <const char* method_name, int id, class ... Types>
    class controller_holder {
    public:
      static control_by_prediction controller;
    };
    
    template <const char* method_name, int id, class ... Types>
    control_by_prediction controller_holder<method_name, id, Types ...>::controller(std::string("controller_holder_") + std::string(method_name) + "_" + std::to_string(id) + "_" + type_name<Types ...>());
    
    // controlled statement with built in estimators
    constexpr
    char default_name[] = "auto";

    template <
      class Par_complexity_measure_fct,
      class Seq_complexity_measure_fct,
      class Par_body_fct,
      class Seq_body_fct
      >
    void spguard(const Par_complexity_measure_fct& par_complexity_measure_fct,
               const Seq_complexity_measure_fct& seq_complexity_measure_fct,
               const Par_body_fct& par_body_fct,
               const Seq_body_fct& seq_body_fct) {
      using controller_type = controller_holder<default_name, 1, Par_complexity_measure_fct, Seq_complexity_measure_fct, Par_body_fct, Seq_body_fct>;
      spguard(controller_type::controller, par_complexity_measure_fct, seq_complexity_measure_fct, par_body_fct, seq_body_fct);
    }    
    
    template <
      class Complexity_measure_fct,
      class Par_body_fct,
      class Seq_body_fct
      >
    void spguard(const Complexity_measure_fct& complexity_measure_fct,
               const Par_body_fct& par_body_fct,
               const Seq_body_fct& seq_body_fct) {
      using controller_type = controller_holder<default_name, 1, Complexity_measure_fct, Par_body_fct, Seq_body_fct>;
      spguard(controller_type::controller, complexity_measure_fct, par_body_fct, seq_body_fct);
    }

    template <
      class Complexity_measure_fct,
      class Par_body_fct
      >
    void spguard(const Complexity_measure_fct& complexity_measure_fct,
               const Par_body_fct& par_body_fct) {
      using controller_type = controller_holder<default_name, 1, Complexity_measure_fct, Par_body_fct>;
      spguard(controller_type::controller, complexity_measure_fct, par_body_fct);
    }
    
    template <
      const char* estimator_name,
      class ... Types,
      class Par_complexity_measure_fct,
      class Seq_complexity_measure_fct,
      class Par_body_fct,
      class Seq_body_fct
      >
    void spguard(const Par_complexity_measure_fct& par_complexity_measure_fct,
               const Seq_complexity_measure_fct& seq_complexity_measure_fct,
               const Par_body_fct& par_body_fct,
               const Seq_body_fct& seq_body_fct) {
      using controller_type = controller_holder<estimator_name, 1, int>;
      spguard(controller_type::controller, par_complexity_measure_fct, seq_complexity_measure_fct, par_body_fct, seq_body_fct);
    }
    
    template <
      const char* estimator_name,
      class ... Types,
      class Complexity_measure_fct,
      class Par_body_fct,
      class Seq_body_fct
      >
    void spguard(const Complexity_measure_fct& complexity_measure_fct,
               const Par_body_fct& par_body_fct,
               const Seq_body_fct& seq_body_fct) {
      using controller_type = controller_holder<estimator_name, 1, int>;
      spguard(controller_type::controller, complexity_measure_fct, par_body_fct, seq_body_fct);
    }
    
    template <
      const char* estimator_name,
      class ... Types,
      class Complexity_measure_fct,
      class Par_body_fct
      >
    void spguard(const Complexity_measure_fct& complexity_measure_fct,
               const Par_body_fct& par_body_fct) {
      using controller_type = controller_holder<estimator_name, 1, int>;
      spguard(controller_type::controller, complexity_measure_fct, par_body_fct);
    }
    
    template <
      const char* method_name,
      int id,
      class ... Types,
      class Par_complexity_measure_fct,
      class Seq_complexity_measure_fct,
      class Par_body_fct,
      class Seq_body_fct
      >
    void spguard(const Par_complexity_measure_fct& par_complexity_measure_fct,
               const Seq_complexity_measure_fct& seq_complexity_measure_fct,
               const Par_body_fct& par_body_fct,
               const Seq_body_fct& seq_body_fct) {
      using controller_type = controller_holder<method_name, id, Types...>;
      spguard(controller_type::controller, par_complexity_measure_fct, seq_complexity_measure_fct, par_body_fct, seq_body_fct);
    }
    
    template <
      const char* method_name,
      int id,
      class ... Types,
      class Complexity_measure_fct,
      class Par_body_fct,
      class Seq_body_fct
      >
    void spguard(const Complexity_measure_fct& complexity_measure_fct,
               const Par_body_fct& par_body_fct,
               const Seq_body_fct& seq_body_fct) {
      using controller_type = controller_holder<method_name, id, Types...>;
      spguard(controller_type::controller, complexity_measure_fct, par_body_fct, seq_body_fct);
    }
    
    template <
      const char* method_name,
      int id,
      class ... Types,
      class Complexity_measure_fct,
      class Par_body_fct
>
    void spguard(const Complexity_measure_fct& complexity_measure_fct,
               const Par_body_fct& par_body_fct) {
      using controller_type = controller_holder<method_name, id, Types...>;
      spguard(controller_type::controller, complexity_measure_fct, par_body_fct);
    }
    
    template <
      class ... Types,
      class Complexity_measure_fct,
      class Par_body_fct,
      class Seq_body_fct
      >
    void spguard(const Complexity_measure_fct& complexity_measure_fct,
               const Par_body_fct& par_body_fct,
               const Seq_body_fct& seq_body_fct) {
      using controller_type = controller_holder<default_name, 1, Types...>;
      spguard(controller_type::controller, complexity_measure_fct, par_body_fct, seq_body_fct);
    }
    
    template <
      class ... Types,
      class Complexity_measure_fct,
      class Par_body_fct
      >
    void spguard(const Complexity_measure_fct& complexity_measure_fct,
               const Par_body_fct& par_body_fct
               ) {
      using controller_type = controller_holder<default_name, 1, Types...>;
      spguard(controller_type::controller, complexity_measure_fct, par_body_fct);
    }
    
    /*---------------------------------------------------------------------*/
    /* Granularity-control enriched fork join */
    
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
      execmode_type mode = my_execmode();
      if ((mode == Sequential) || (mode == Force_sequential)) {
        f1();
        f2();
      } else {
        cost_type upper_work = work.mine() + since_in_cycles(timer.mine());
        work.mine() = 0;
        cost_type left_work, right_work;
        primitive_fork2([&] {
            work.mine() = 0;
            timer.mine() = get_wall_time();
            execmode.mine().block(mode, f1);
            left_work = work.mine() + since_in_cycles(timer.mine());
          }, [&] {
            work.mine() = 0;
            timer.mine() = get_wall_time();
            execmode.mine().block(mode, f2);
            right_work = work.mine() + since_in_cycles(timer.mine());
          });
        work.mine() = upper_work + left_work + right_work;
        timer.mine() = get_wall_time();
        return;
      }
    }
    
  } // end namespace
  
  
} // end namespace

#endif

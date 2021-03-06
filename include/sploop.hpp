
#include "spgranularity.hpp"

#ifndef _SPTL_LOOP_H_
#define _SPTL_LOOP_H_

namespace sptl {
  
template <
  class Iter,
  class Body,
  class Comp_rng,
  class Seq_body_rng
>
void parallel_for(Iter lo,
                  Iter hi,
                  const Comp_rng& comp_rng,
                  const Body& body,
                  const Seq_body_rng& seq_body_rng) {
#ifdef SPTL_USE_SEQUENTIAL_ELISION_RUNTIME
  for (Iter i = lo; i < hi; i++) {
    body(i);
  }
  return;
#endif
  auto comp = comp_rng(lo, hi);
  spguard([&] { return comp; }, [&] {
    auto n = hi - lo;
    if (n <= 0) {
      // nothing to do
    } else if (n == 1) {
      body(lo);
    } else {
      Iter mid = lo + (n / 2);
      fork2([&] {
        parallel_for(lo, mid, comp_rng, body, seq_body_rng);
      }, [&] {
        parallel_for(mid, hi, comp_rng, body, seq_body_rng);
      });
    }
  }, [&] {
    seq_body_rng(lo, hi);
  });
}

template <class Iter, class Body, class Comp_rng>
void parallel_for(Iter lo, Iter hi, const Comp_rng& comp_rng, const Body& body) {
  auto seq_body_rng = [&] (Iter lo, Iter hi) {
    for (Iter i = lo; i != hi; i++) {
      body(i);
    }
  };
  parallel_for(lo, hi, comp_rng, body, seq_body_rng);
}

template <class Iter, class Body>
void parallel_for(Iter lo, Iter hi, const Body& body) {
  auto comp_rng = [&] (Iter lo, Iter hi) {
    return (size_type)(hi - lo);
  };
  parallel_for(lo, hi, comp_rng, body);
}
  
} // end namespace

#endif

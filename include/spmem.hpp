
#include <algorithm>
#include <type_traits>

#include "sploop.hpp"

#ifndef _SPTL_MEM_H_
#define _SPTL_MEM_H_

namespace sptl {

template <class Iter, class Item>
void fill(Iter lo, Iter hi, const Item& val) {
  if (std::is_trivially_copyable<Item>::value) {
    range::parallel_for(lo, hi, [&] (Iter lo, Iter hi) { return hi - lo; }, [&] (Iter i) {
      std::fill(i, i+1, val);
    }, [&] (Iter lo, Iter hi) {
      std::fill(lo, hi, val);
    });
  } else {
    range::parallel_for(lo, hi, [&] (Iter lo, Iter hi) { return hi - lo; }, [&] (Iter i) {
      new (i) Item();
    }, [&] (Iter lo, Iter hi) {
      for (Iter i = lo; i != hi; i++) {
        new (i) Item();
      }
    });
  }
}
  
template <class Iter, class Output_iterator>
void copy(Iter lo, Iter hi, Output_iterator dst) {
  range::parallel_for(lo, hi, [&] (Iter lo, Iter hi) { return hi - lo; }, [&] (Iter i) {
    std::copy(i, i + 1, dst + (i - lo));
  }, [&] (Iter lo2, Iter hi2) {
    std::copy(lo2, hi2, dst + (lo2 - lo));
  });
}

template <class Item, class Alloc>
void pdelete(Item* lo, Item* hi) {
  if (std::is_trivially_destructible<Item>()) {
    return;
  }
  parallel_for(lo, hi, [&] (Item* p) {
    Alloc alloc;
    alloc.destroy(p);
  });
}
  
} // end namespace

#endif

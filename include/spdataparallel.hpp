
#include "spreduce.hpp"

#ifndef _SPTL_DATAPAR_H_
#define _SPTL_DATAPAR_H_

namespace sptl {

/*---------------------------------------------------------------------*/
/* Max index */
  
template <
  class Item,
  class Comp,
  class Get
>
size_type max_index(size_type n, const Item& id, const Comp& comp, const Get& get) {
  if (n == 0) {
    return (size_type)-1L;
  }
  using result_type = std::pair<size_type, Item>;
  result_type res(0, id);
  using input_type = level4::tabulate_input;
  input_type in(0, n);
  auto combine = [&] (result_type x, result_type y) {
    if (comp(x.second, y.second)) { // x > y
      return x;
    } else {
      return y;
    }
  };
  using output_type = level3::cell_output<result_type, decltype(combine)>;
  output_type out(res, combine);
  auto convert_reduce = [&] (input_type& in, result_type& out) {
    for (size_type i = in.lo; i < in.hi; i++) {
      const Item& x = get(i);
      if (comp(x, out.second)) {
        out = result_type(i, x);
      }
    }
  };
  auto seq_convert_reduce = convert_reduce;
  level4::reduce(in, out, res, res, convert_reduce, seq_convert_reduce);
  return res.first;
}
  
template <
  class Iter,
  class Item,
  class Comp,
  class Lift
>
size_type max_index(Iter lo, Iter hi, const Item& id, const Comp& comp, const Lift& lift) {
  if (hi - lo == 0) {
    return (size_type)-1L;
  }
  using result_type = std::pair<size_type, Item>;
  result_type id2(0, id);
  auto combine = [&] (result_type x, result_type y) {
    if (comp(x.second, y.second)) { // x > y
      return x;
    } else {
      return y;
    }
  };
  auto lift_idx = [&] (size_type i, reference_of<Iter> x) {
    return result_type(i, lift(i, x));
  };
  auto seq_reduce_rng = [&] (Iter _lo, Iter _hi) {
    size_type i = _lo - lo;
    result_type res(0, id);
    for (Iter it = _lo; it != _hi; it++, i++) {
      auto x = lift(i, *it);
      if (comp(x, res.second)) {
        res = result_type(i, x);
      }
    }
    return res;
  };
  return level2::reduce(lo, hi, id2, combine, lift_idx, seq_reduce_rng).first;
}
  
template <
  class Iter,
  class Item,
  class Comp
>
size_type max_index(Iter lo, Iter hi, const Item& id, const Comp& comp) {
  return max_index(lo, hi, id, comp, [&] (size_type, const Item& x) {
    return x;
  });
}

/*---------------------------------------------------------------------*/
/* Pack and filter */

namespace __priv {

// later: can we generalize this algorithm so that we can simultaneously
// use the int-packing optimization and support non-contiguous layouts
// of the flags array
// sums a sequence of n boolean flags
// an optimized version that sums blocks of 4 booleans by treating
// them as an integer
// Only optimized when n is a multiple of 512 and Fl is 4byte aligned
size_type sum_flags_serial(const bool *Fl, size_type n) {
  size_type r = 0;
  if ((n >= 128) && ((n & 511) == 0) && (((long) Fl & 3) == 0)) {
    const int* IFl = (const int*) Fl;
    for (int k = 0; k < (n >> 9); k++) {
      int rr = 0;
      for (int j = 0; j < 128; j++) {
        rr += IFl[j];
      }
      r += (rr&255) + ((rr>>8)&255) + ((rr>>16)&255) + ((rr>>24)&255);
      IFl += 128;
    }
  } else {
    for (size_type j = 0; j < n; j++) {
      r += Fl[j];
    }
  }
  return r;
}

static constexpr
int pack_branching_factor = 2048;
  
template <
  class Flags_iter,
  class Iter,
  class Item,
  class Output,
  class F
>
size_type pack(Flags_iter flags_lo, Iter lo, Iter hi, Item&, const Output& out, const F f) {
  size_type n = hi - lo;
  if (n == 0) {
    return 0;
  }
  if (n <= pack_branching_factor) {
    size_type m = sum_flags_serial(flags_lo, n);
    auto dst_lo = out(m);
    for (size_type i = 0, j = 0; i < n; i++) {
      if (flags_lo[i]) {
        dst_lo[j++] = lo[i];
      }
    }
    return m;
  }
  size_type nb_branches = (n + pack_branching_factor - 1) / pack_branching_factor;
  parray<size_type> sizes(nb_branches, [&] (size_type i) {
    size_type lo = i * pack_branching_factor;
    size_type hi = std::min((i + 1) * pack_branching_factor, n);
    return level2::reduce(flags_lo + lo, flags_lo + hi, (size_type)0,
                          [&] (size_type x, size_type y) {
                            return x + y;
                          },
                          [&] (size_type, reference_of<Flags_iter> x) {
                            return x ? 1 : 0;
                          }, [&] (Flags_iter lo, Flags_iter hi) {
                            return (size_type)sum_flags_serial(lo, hi - lo);
                          });
  });
  size_type m = dps::scan(sizes.begin(), sizes.end(), (size_type)0,
                       [&] (size_type x, size_type y) {
                         return x + y;
                       }, sizes.begin(), forward_exclusive_scan);
  auto dst_lo = out(m);
  auto comp = [&] (size_type lo, size_type hi) {
    return hi - lo;
  };
  parallel_for((size_type)0, nb_branches, comp, [&, dst_lo, flags_lo, sizes] (size_type i) {
    size_type _lo = i * pack_branching_factor;
    size_type _hi = std::min(n, (i + 1) * pack_branching_factor);
    size_type b = i;
    size_type offset = sizes[b];
    for (auto i = _lo; i < _hi; i++) {
      if (flags_lo[i]) {
        dst_lo[offset++] = f(i, lo[i]);
      }
    }
  }, [&, dst_lo, flags_lo, sizes] (size_type _lo, size_type _hi) {
    size_type blo = _lo * pack_branching_factor;
    size_type bhi = std::min(n, (_hi + 1) * pack_branching_factor);
    size_type offset = sizes[_lo];
    for (auto i = blo; i < bhi; i++) {
      if (flags_lo[i]) {
        dst_lo[offset++] = f(i, lo[i]);
      }
    }
  });
  return m;
}

} // end namespace
  
template <class Item_iter, class Flags_iter>
parray<value_type_of<Item_iter>> pack(Item_iter lo, Item_iter hi, Flags_iter flags_lo) {
  parray<value_type_of<Item_iter>> result;
  value_type_of<Item_iter> tmp;
  __priv::pack(flags_lo, lo, hi, tmp, [&] (size_type m) {
    result.reset(m);
    return result.begin();
  }, [&] (size_type, reference_of<Item_iter> x) {
    return x;
  });
  return result;
}

template <class Item_iter, class Flags_iter>
parray<value_type_of<Item_iter>> pack_seq(Item_iter lo, Item_iter hi, Flags_iter flags_lo) {
  size_type total = 0;
  parray<value_type_of<Item_iter>> result;
  for (size_type it = 0; it < hi - lo; it++) {
    if (flags_lo[it]) {
      total++;
    }
  }
  result.reset(total);
  total = 0;
  for (size_type it = 0; it < hi - lo; it++) {
    if (flags_lo[it]) {
      result[total++] = lo[it];
    }
  }
  return result;
}        
  
template <class Flags_iter>
parray<size_type> pack_index(Flags_iter lo, Flags_iter hi) {
  parray<size_type> result;
  size_type dummy;
  __priv::pack(lo, lo, hi, dummy, [&] (size_type m) {
    result.reset(m);
    return result.begin();
  }, [&] (size_type offset, reference_of<Flags_iter>) {
    return offset;
  });
  return result;
}
  
template <class Iter, class Pred_idx>
parray<value_type_of<Iter>> filteri(Iter lo, Iter hi, const Pred_idx& pred_idx) {
  size_type n = hi - lo;
  parray<bool> flags(n, [&] (size_type i) {
    return pred_idx(i, *(lo+i));
  });
  value_type_of<Iter> dummy;
  parray<value_type_of<Iter>> dst;
  __priv::pack(flags.cbegin(), lo, hi, dummy, [&] (size_type m) {
    dst.reset(m);
    return dst.begin();
  }, [&] (size_type, reference_of<Iter> x) {
    return x;
  });
  return dst;
}
  
template <class Iter, class Pred>
parray<value_type_of<Iter>> filter(Iter lo, Iter hi, const Pred& pred) {
  auto pred_idx = [&] (size_type, reference_of<Iter> x) {
    return pred(x);
  };
  return filteri(lo, hi, pred_idx);
}

namespace dps {

  template <
    class Flags_iter,
    class Input_iter,
    class Output_iter
  >
  size_type pack(Flags_iter flags_lo, Input_iter lo, Input_iter hi, Output_iter dst_lo) {
    return __priv::pack(flags_lo, lo, hi, *lo, [&] (size_type) {
      return dst_lo;
    }, [&] (size_type, reference_of<Input_iter> x) {
      return x;
    });
  }

  template <
    class Input_iter,
    class Output_iter,
    class Pred_idx
  >
  size_type filteri(Input_iter lo, Input_iter hi, Output_iter dst_lo, const Pred_idx& pred_idx) {
    size_type n = hi - lo;
    parray<bool> flags(n, [&] (size_type i) {
      return pred_idx(i, *(lo+i));
    });
    return pack(flags.cbegin(), lo, hi, dst_lo);
  }

  template <
    class Input_iter,
    class Output_iter,
    class Pred
  >
  size_type filter(Input_iter lo, Input_iter hi, Output_iter dst_lo, const Pred& pred) {
    auto pred_idx = [&] (size_type, reference_of<Input_iter> x) {
      return pred(x);
    };
    return filteri(lo, hi, dst_lo, pred_idx);
  }

  
} // end namespace

/*---------------------------------------------------------------------*/
/* Array-sum and max */
  
template <class Iter>
value_type_of<Iter> sum(Iter lo, Iter hi) {
  using number = value_type_of<Iter>;
  return reduce(lo, hi, (number)0, [&] (number x, number y) {
    return x + y;
  });
}

template <class Iter>
value_type_of<Iter> max(Iter lo, Iter hi) {
  using number = value_type_of<Iter>;
  number id = std::numeric_limits<number>::lowest();
  return reduce(lo, hi, id, [&] (number x, number y) {
    return std::max(x, y);
  });
}
  
template <class Iter>
value_type_of<Iter> min(Iter lo, Iter hi) {
  using number = value_type_of<Iter>;
  number id = std::numeric_limits<number>::max();
  return reduce(lo, hi, id, [&] (number x, number y) {
    return std::min(x, y);
  });
}

} // end namespace

#endif

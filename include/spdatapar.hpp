
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
size_t max_index(size_t n, const Item& id, const Comp& comp, const Get& get) {
  if (n == 0) {
    return (size_t)-1L;
  }
  using result_type = std::pair<size_t, Item>;
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
    for (size_t i = in.lo; i < in.hi; i++) {
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
size_t max_index(Iter lo, Iter hi, const Item& id, const Comp& comp, const Lift& lift) {
  if (hi - lo == 0) {
    return (size_t)-1L;
  }
  using result_type = std::pair<size_t, Item>;
  result_type id2(0, id);
  auto combine = [&] (result_type x, result_type y) {
    if (comp(x.second, y.second)) { // x > y
      return x;
    } else {
      return y;
    }
  };
  auto lift_idx = [&] (size_t i, reference_of<Iter> x) {
    return result_type(i, lift(i, x));
  };
  auto seq_reduce_rng = [&] (Iter _lo, Iter _hi) {
    size_t i = _lo - lo;
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
size_t max_index(Iter lo, Iter hi, const Item& id, const Comp& comp) {
  return max_index(lo, hi, id, comp, [&] (size_t, const Item& x) {
    return x;
  });
}

/*---------------------------------------------------------------------*/
/* Pack and filter */

namespace __priv {

static constexpr
int pack_branching_factor = 2048;
  
template <
  class Flags_iter,
  class Iter,
  class Item,
  class Output,
  class F
>
size_t pack(Flags_iter flags_lo, Iter lo, Iter hi, Item&, const Output& out, const F f) {
  size_t n = hi - lo;
  if (n == 0) {
    return 0;
  }
  auto combine = [&] (size_t x, size_t y) {
    return x + y;
  };
  auto lift = [&] (reference_of<Flags_iter> x) {
    return (size_t)x;
  };
  if (n <= pack_branching_factor) {
    int total = 0;
    for (int i = 0; i < n; i++) {
      if (flags_lo[i]) {
         total++;
      }
    }
    auto dst_lo = out(total);
    total = 0;
    for (int i = 0; i < n; i++) {
      if (flags_lo[i]) {
        dst_lo[total++] = lo[i];
      }
    } return total;
  }
  size_t len = (n + pack_branching_factor - 1) / pack_branching_factor;
  auto body = [&] (size_t i) {
    size_t l = i * pack_branching_factor;
    size_t r = std::min((i + 1) * pack_branching_factor, n);
    return level1::reduce(flags_lo + l, flags_lo + r, (size_t)0, combine, lift);
  };
  parray<size_t> sizes(len, body);
  size_t m = dps::scan(sizes.begin(), sizes.end(), (size_t)0, combine, sizes.begin(), forward_exclusive_scan);
  auto dst_lo = out(m);
  parallel_for((size_t)0, len, [&] (size_t l, size_t r) { return r - l; }, [&, dst_lo, flags_lo, sizes] (size_t i) {
    size_t l = i * pack_branching_factor;
    size_t r = std::min(n, (i + 1) * pack_branching_factor);
    size_t b = i;
    size_t offset = sizes[b];
    for (int i = l; i < r; i++) {
      if (flags_lo[i]) {
        dst_lo[offset++] = f(i, lo[i]);
      }
    }
  }, [&, dst_lo, flags_lo, sizes] (size_t l, size_t r) {
    size_t ll = l * pack_branching_factor;
    size_t rr = std::min(n, (r + 1) * pack_branching_factor);
    size_t offset = sizes[l];
    for (int i = ll; i < rr; i++) {
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
  __priv::pack(flags_lo, lo, hi, tmp, [&] (size_t m) {
    result.prefix_tabulate(m, 0);
    return result.begin();
  }, [&] (size_t, reference_of<Item_iter> x) {
    return x;
  });
  return result;
}

template <class Item_iter, class Flags_iter>
parray<value_type_of<Item_iter>> pack_seq(Item_iter lo, Item_iter hi, Flags_iter flags_lo) {
  size_t total = 0;
  parray<value_type_of<Item_iter>> result;
  for (size_t it = 0; it < hi - lo; it++) {
    if (flags_lo[it]) {
      total++;
    }
  }
  result.prefix_tabulate(total, 0);
  total = 0;
  for (size_t it = 0; it < hi - lo; it++) {
    if (flags_lo[it]) {
      result[total++] = lo[it];
    }
  }
  return result;
}        
  
template <class Flags_iter>
parray<size_t> pack_index(Flags_iter lo, Flags_iter hi) {
  parray<size_t> result;
  size_t dummy;
  __priv::pack(lo, lo, hi, dummy, [&] (size_t m) {
    result.prefix_tabulate(m, 0);
    return result.begin();
  }, [&] (size_t offset, reference_of<Flags_iter>) {
    return offset;
  });
  return result;
}
  
template <class Iter, class Pred_idx>
parray<value_type_of<Iter>> filteri(Iter lo, Iter hi, const Pred_idx& pred_idx) {
  size_t n = hi - lo;
  parray<bool> flags(n, [&] (size_t i) {
    return pred_idx(i, *(lo+i));
  });
  value_type_of<Iter> dummy;
  parray<value_type_of<Iter>> dst;
  __priv::pack(flags.cbegin(), lo, hi, dummy, [&] (size_t m) {
    dst.prefix_tabulate(m, 0);
    return dst.begin();
  }, [&] (size_t, reference_of<Iter> x) {
    return x;
  });
  return dst;
}
  
template <class Iter, class Pred>
parray<value_type_of<Iter>> filter(Iter lo, Iter hi, const Pred& pred) {
  auto pred_idx = [&] (size_t, reference_of<Iter> x) {
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
  size_t pack(Flags_iter flags_lo, Input_iter lo, Input_iter hi, Output_iter dst_lo) {
    return __priv::pack(flags_lo, lo, hi, *lo, [&] (size_t) {
      return dst_lo;
    }, [&] (size_t, reference_of<Input_iter> x) {
      return x;
    });
  }

  template <
    class Input_iter,
    class Output_iter,
    class Pred_idx
  >
  size_t filteri(Input_iter lo, Input_iter hi, Output_iter dst_lo, const Pred_idx& pred_idx) {
    size_t n = hi - lo;
    parray<bool> flags(n, [&] (size_t i) {
      return pred_idx(i, *(lo+i));
    });
    return pack(flags.cbegin(), lo, hi, dst_lo);
  }

  template <
    class Input_iter,
    class Output_iter,
    class Pred
  >
  size_t filter(Input_iter lo, Input_iter hi, Output_iter dst_lo, const Pred& pred) {
    auto pred_idx = [&] (size_t, reference_of<Input_iter> x) {
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

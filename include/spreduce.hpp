
#include <type_traits>

#include "spparray.hpp"
#include "sppchunkedseqbase.hpp"

#ifndef _SPTL_REDUCE_H_
#define _SPTL_REDUCE_H_

namespace sptl {

/*---------------------------------------------------------------------*/
/* Type declarations */
  
template <class Iter>
using value_type_of = typename std::iterator_traits<Iter>::value_type;

template <class Iter>
using reference_of = typename std::iterator_traits<Iter>::reference;

template <class Iter>
using pointer_of = typename std::iterator_traits<Iter>::pointer;

using scan_type = enum {
  forward_inclusive_scan,
  forward_exclusive_scan,
  backward_inclusive_scan,
  backward_exclusive_scan
};

/*---------------------------------------------------------------------*/
/* Level 4 reduction */
  
namespace level4 {

template <
  class Input,
  class Output,
  class Result,
  class Convert_reduce_comp,
  class Convert_reduce,
  class Seq_convert_reduce
>
void reduce(Input& in,
            const Output& out,
            Result& id,
            Result& dst,
            const Convert_reduce_comp& convert_reduce_comp,
            const Convert_reduce& convert_reduce,
            const Seq_convert_reduce& seq_convert_reduce) {
  spguard([&] { return convert_reduce_comp(in); }, [&] {
    if (! in.can_split()) {
      convert_reduce(in, dst);
    } else {
      Input in2(in);
      in.split(in2);
      Result dst2;
      out.init(dst2);
      fork2([&] {
        reduce(in,  out, id, dst,  convert_reduce_comp, convert_reduce, seq_convert_reduce);
      }, [&] {
        reduce(in2, out, id, dst2, convert_reduce_comp, convert_reduce, seq_convert_reduce);
      });
      out.merge(dst2, dst);
    }
  }, [&] {
    seq_convert_reduce(in, dst);
  });
}

template <
  class Input,
  class Output,
  class Result,
  class Convert_reduce,
  class Seq_convert_reduce
>
void reduce(Input& in,
            const Output& out,
            Result& id,
            Result& dst,
            const Convert_reduce& convert_reduce,
            const Seq_convert_reduce& seq_convert_reduce) {
  auto convert_reduce_comp = [&] (Input& in) {
    return in.size();
  };
  reduce(in, out, id, dst, convert_reduce_comp, convert_reduce, seq_convert_reduce);
}

namespace {
    
template <
  class In_iter,
  class Out_iter,
  class Output,
  class Result,
  class Convert
>
void scan_seq(In_iter in_lo,
              In_iter in_hi,
              Out_iter out_lo,
              const Output& out,
              const Result& id,
              const Convert& convert,
              scan_type st) {
  Result x;
  out.copy(id, x);
  In_iter in_it = in_lo;
  Out_iter out_it = out_lo;
  if (st == forward_exclusive_scan) {
    in_it = in_lo;
    out_it = out_lo;
    for (; in_it != in_hi; in_it++, out_it++) {
      Result tmp1; // required because input and output ranges can overlap
      out.copy(x, tmp1);
      Result tmp2;
      convert(*in_it, tmp2);
      out.merge(tmp2, x);
      out.copy(tmp1, *out_it);
    }
  } else if (st == forward_inclusive_scan) {
    in_it = in_lo;
    out_it = out_lo;
    for (; in_it != in_hi; in_it++, out_it++) {
      Result tmp;
      convert(*in_it, tmp);
      out.merge(tmp, x);
      out.copy(x, *out_it);
    }
  } else if (st == backward_exclusive_scan) {
    auto n = in_hi - in_lo;
    auto m = n - 1;
    in_it = in_lo + m;
    out_it = out_lo + m;
    for (; in_it >= in_lo; in_it--, out_it--) {
      Result tmp1; // required because input and output ranges can overlap
      out.copy(x, tmp1);
      Result tmp2;
      convert(*in_it, tmp2);
      out.merge(tmp2, x);
      out.copy(tmp1, *out_it);
    }
  } else if (st == backward_inclusive_scan) {
    auto n = in_hi - in_lo;
    auto m = n - 1;
    in_it = in_lo + m;
    out_it = out_lo + m;
    for (; in_it >= in_lo; in_it--, out_it--) {
      Result tmp;
      convert(*in_it, tmp);
      out.merge(tmp, x);
      out.copy(x, *out_it);
    }
  } else {
    //util::atomic::die("Bogus scan type passed to scan");
  }
}
  
template <class In_iter, class Out_iter, class Output, class Result>
void scan_seq(In_iter in_lo,
              In_iter in_hi,
              Out_iter out_lo,
              const Output& out,
              const Result& id,
              scan_type st) {
  auto convert = [&] (const Result& src, Result& dst) {
    out.copy(src, dst);
  };
  scan_seq(in_lo, in_hi, out_lo, out, id, convert, st);
}

template <class Result, class Output>
void scan_seq(const parray<Result>& ins,
              typename parray<Result>::iterator outs_lo,
              const Output& out,
              const Result& id,
              scan_type st) {
  scan_seq(ins.cbegin(), ins.cend(), outs_lo, out, id, st);
}

static constexpr
size_t Scan_branching_factor = 2048;

static inline
size_t get_nb_blocks(size_t k, size_t n) {
  return 1 + ((n - 1) / k);
} 
  
static inline
std::pair<size_t,size_t> get_rng(size_t k, size_t n, size_t i) {
  size_t lo = i * k;
  size_t hi = std::min(lo + k, n);
  return std::make_pair(lo, hi);
}

static inline
bool is_backward_scan(scan_type st) {
  return (st == backward_inclusive_scan) || (st == backward_exclusive_scan);
}

template <class Result, class Output, class Merge_comp_rng>
void scan_rec(const parray<Result>& ins,
              typename parray<Result>::iterator outs_lo,
              const Output& out,
              const Result& id,
              const Merge_comp_rng& merge_comp_rng,
              scan_type st) {
  static constexpr
  int k = Scan_branching_factor;
  size_t n = ins.size();
  size_t m = get_nb_blocks(k, n);
  auto loop_comp = [&] (size_t _lo, size_t _hi) {
    auto beg = ins.cbegin();
    size_t lo = get_rng(k, n, _lo).first;
    size_t hi = get_rng(k, n, _hi).second;
    return merge_comp_rng(beg + lo, beg + hi);
  };
  spguard([&] { return merge_comp_rng(ins.cbegin(), ins.cend()); }, [&] {
    if (n <= k) {
      scan_seq(ins, outs_lo, out, id, st);
    } else {
      parray<Result> partials;
      if (std::is_fundamental<Result>::value) {
        partials.prefix_tabulate(m, 0);
      } else {
        partials.prefix_tabulate(m, m);
      }
      auto b1 = [&] (size_t i) {
        auto beg = ins.cbegin();
        size_t lo = get_rng(k, n, i).first;
        size_t hi = get_rng(k, n, i).second;
        out.merge(beg + lo, beg + hi, partials[i]);
      };
      parallel_for((size_t)0, m, loop_comp, b1);
      parray<Result> scans;
      if (std::is_fundamental<Result>::value) {
        scans.prefix_tabulate(m, 0);
      } else {
        scans.prefix_tabulate(m, m);
      }
      auto st2 = (is_backward_scan(st)) ? backward_exclusive_scan : forward_exclusive_scan;
      scan_rec(partials, scans.begin(), out, id, merge_comp_rng, st2);
      auto b2 = [&] (size_t i) {
        auto ins_beg = ins.cbegin();
        size_t lo = get_rng(k, n, i).first;
        size_t hi = get_rng(k, n, i).second;
        scan_seq(ins_beg + lo, ins_beg + hi, outs_lo + lo, out, scans[i], st);
      };
      parallel_for((size_t)0, m, loop_comp, b2);
    }
  }, [&] {
    scan_seq(ins, outs_lo, out, id, st);
  });
}

template <
  class Input,
  class Output,
  class Result,
  class Output_iter,
  class Merge_comp_rng,
  class Convert_reduce_comp_rng,
  class Convert_reduce,
  class Convert_scan,
  class Seq_convert_scan
>
void scan(Input& in,
          const Output& out,
          Result& id,
          Output_iter outs_lo,
          const Merge_comp_rng& merge_comp_rng,
          const Convert_reduce_comp_rng& convert_reduce_comp_rng,
          const Convert_reduce& convert_reduce,
          const Convert_scan& convert_scan,
          const Seq_convert_scan& seq_convert_scan,
          scan_type st) {
  const size_t k = Scan_branching_factor;
  size_t n = in.size();
  size_t m = get_nb_blocks(k, n);
  auto loop_comp = [&] (size_t _lo, size_t _hi) {
    size_t lo = get_rng(k, n, _lo).first;
    size_t hi = get_rng(k, n, _hi).second;
    return convert_reduce_comp_rng(lo, hi);
  };
  spguard([&] { return convert_reduce_comp_rng((size_t)0, n); }, [&] {
    if (n <= k) {
      convert_scan(id, in, outs_lo);
    } else {
      parray<Input> splits = in.split(m);
      parray<Result> partials;
      if (std::is_fundamental<Result>::value) {
        partials.prefix_tabulate(m, 0);
      } else {
        partials.prefix_tabulate(m, m);
      }
      auto b1 = [&] (size_t i) {
        size_t lo = get_rng(k, n, i).first;
        size_t hi = get_rng(k, n, i).second;
        Input in2 = in.slice(splits, lo, hi);
        convert_reduce(in2, partials[i]);
      };
      parallel_for((size_t)0, m, loop_comp, b1);
      parray<Result> scans;
      if (std::is_fundamental<Result>::value) {
        scans.prefix_tabulate(m, 0);
      } else {
        scans.prefix_tabulate(m, m);
      }
      auto st2 = (is_backward_scan(st)) ? backward_exclusive_scan : forward_exclusive_scan;
      scan_rec(partials, scans.begin(), out, id, merge_comp_rng, st2);
      auto b2 = [&] (size_t i) {
        size_t lo = get_rng(k, n, i).first;
        size_t hi = get_rng(k, n, i).second;
        Input in2 = in.slice(splits, lo, hi);
        scan(in2, out, scans[i], outs_lo + lo, merge_comp_rng, convert_reduce_comp_rng, convert_reduce, convert_scan, seq_convert_scan, st);
      };
      parallel_for((size_t)0, m, loop_comp, b2);
    }
  }, [&] {
    seq_convert_scan(id, in, outs_lo);
  });
}

} // end namespace

template <
  class Input,
  class Merge_comp_rng,
  class Output,
  class Result,
  class Output_iter,
  class Convert_reduce_comp_rng,
  class Convert_reduce,
  class Convert_scan,
  class Seq_convert_scan
>
void scan(Input& in,
	  const Merge_comp_rng& merge_comp_rng,
          const Output& out,
          Result& id,
          Output_iter outs_lo,
          const Convert_reduce_comp_rng& convert_reduce_comp_rng,
          const Convert_reduce& convert_reduce,
          const Convert_scan& convert_scan,
          const Seq_convert_scan& seq_convert_scan,
          scan_type st) {
  scan(in, out, id, outs_lo, merge_comp_rng, convert_reduce_comp_rng, convert_reduce, convert_scan, seq_convert_scan, st);
}
  
template <
  class Input,
  class Output,
  class Result,
  class Output_iter,
  class Convert_reduce,
  class Convert_scan,
  class Seq_convert_scan
>
void scan(Input& in,
          const Output& out,
          Result& id,
          Output_iter outs_lo,
          const Convert_reduce& convert_reduce,
          const Convert_scan& convert_scan,
          const Seq_convert_scan& seq_convert_scan,
          scan_type st) {
  auto merge_comp_rng = [&] (const Result* lo, const Result* hi) {
    return hi - lo;
  };
  auto convert_reduce_comp = [&] (Input& in) {
    return in.size();
  };
  scan(in, out, id, outs_lo, merge_comp_rng, convert_reduce_comp, convert_reduce, convert_scan, seq_convert_scan, st);
}
  
template <class Input_iter>
class random_access_iterator_input {
public:
  
  using self_type = random_access_iterator_input;
  using array_type = parray<self_type>;
  
  Input_iter lo;
  Input_iter hi;
  
  random_access_iterator_input() { }
  
  random_access_iterator_input(Input_iter lo, Input_iter hi)
  : lo(lo), hi(hi) { }
  
  bool can_split() const {
    return size() >= 2;
  }
  
  size_t size() const {
    return hi - lo;
  }
  
  void split(random_access_iterator_input& dst) {
    dst = *this;
    size_t n = size();
    assert(n >= 2);
    Input_iter mid = lo + (n / 2);
    hi = mid;
    dst.lo = mid;
  }
  
  array_type split(size_t) {
    array_type tmp;
    return tmp;
  }
  
  self_type slice(const array_type&, size_t _lo, size_t _hi) {
    self_type tmp(lo + _lo, lo + _hi);
    return tmp;
  }
  
};
  
using tabulate_input = random_access_iterator_input<size_t>;
  
template <class Chunkedseq>
class chunkedseq_input {
public:
  
  using self_type = chunkedseq_input<Chunkedseq>;
  using array_type = parray<self_type>;
  
  Chunkedseq seq;
  
  chunkedseq_input(Chunkedseq& _seq) {
    _seq.swap(seq);
  }
  
  chunkedseq_input(const chunkedseq_input& other) { }
  
  bool can_split() const {
    return seq.size() >= 2;
  }
  
  void split(chunkedseq_input& dst) {
    size_t n = seq.size() / 2;
    seq.split(seq.begin() + n, dst.seq);
  }
  
  array_type split(size_t) {
    array_type tmp;
    assert(false);
    return tmp;
  }
  
  self_type slice(const array_type&, size_t _lo, size_t _hi) {
    self_type tmp;
    assert(false);
    return tmp;
  }
  
};
  
} // end namespace

/*---------------------------------------------------------------------*/
/* Level 3 reduction */

namespace level3 {

template <
  class Input_iter,
  class Output,
  class Result,
  class Lift_comp_rng,
  class Lift_idx_dst,
  class Seq_reduce_rng_dst
>
void reduce(Input_iter lo,
            Input_iter hi,
            const Output& out,
            Result& id,
            Result& dst,
            const Lift_comp_rng& lift_comp_rng,
            const Lift_idx_dst& lift_idx_dst,
            const Seq_reduce_rng_dst& seq_reduce_rng_dst) {
  using input_type = level4::random_access_iterator_input<Input_iter>;
  input_type in(lo, hi);
  auto convert_reduce_comp = [&] (input_type& in) {
    return lift_comp_rng(in.lo, in.hi);
  };
  auto convert_reduce = [&] (input_type& in, Result& dst) {
    size_t i = in.lo - lo;
    dst = id;
    for (Input_iter it = in.lo; it != in.hi; it++, i++) {
      Result tmp;
      lift_idx_dst(i, *it, tmp);
      out.merge(tmp, dst);
    }
  };
  auto seq_convert_reduce = [&] (input_type& in, Result& dst) {
    seq_reduce_rng_dst(in.lo, in.hi, dst);
  };
  level4::reduce(in, out, id, dst, convert_reduce_comp, convert_reduce, seq_convert_reduce);
}

template <
  class Input_iter,
  class Output,
  class Result,
  class Lift_idx_dst,
  class Seq_reduce_rng_dst
>
void reduce(Input_iter lo,
            Input_iter hi,
            const Output& out,
            Result& id,
            Result& dst,
            const Lift_idx_dst& lift_idx_dst,
            const Seq_reduce_rng_dst& seq_reduce_rng_dst) {
  auto lift_comp_rng = [&] (const Input_iter* lo, const Input_iter* hi) {
    return hi - lo;
  };
  reduce(lo, hi, out, id, dst, lift_comp_rng, lift_idx_dst, seq_reduce_rng_dst);
}
  
template <
  class Input_iter,
  class Output_comp_rng,
  class Output,
  class Result,
  class Output_iter,
  class Lift_comp_rng,
  class Lift_idx_dst,
  class Seq_scan_rng_dst
>
void scan(Input_iter lo,
          Input_iter hi,
	  const Output_comp_rng& output_comp_rng,
          const Output& out,
          Result& id,
          Output_iter outs_lo,
          const Lift_comp_rng& lift_comp_rng,
          const Lift_idx_dst& lift_idx_dst,
          const Seq_scan_rng_dst& seq_scan_rng_dst,
          scan_type st) {
  using input_type = level4::random_access_iterator_input<Input_iter>;
  input_type in(lo, hi);
  auto merge_comp_rng = [&] (const Result* lo, const Result* hi) {
    return output_comp_rng(lo, hi);
  };
  auto convert_reduce_comp_rng = [&] (size_t lo, size_t hi) {
    return lift_comp_rng(in.lo + lo, in.lo + hi);
  };
  auto convert_reduce = [&] (input_type& in, Result& dst) {
    size_t i = in.lo - lo;
    dst = id;
    for (Input_iter it = in.lo; it != in.hi; it++, i++) {
      Result tmp;
      lift_idx_dst(i, *it, tmp);
      out.merge(tmp, dst);
    }
  };
  auto convert_scan = [&] (Result _id, input_type& in, Output_iter outs_lo) {
    size_t pos = in.lo - lo;
    level4::scan_seq(in.lo, in.hi, outs_lo, out, _id, [&] (reference_of<Input_iter> src, Result& dst) {
      lift_idx_dst(pos++, src, dst);
    }, st);
  };
  auto seq_convert_scan = [&] (Result _id, input_type& in, Output_iter outs_lo) {
    seq_scan_rng_dst(_id, in.lo, in.hi, outs_lo);
  };
  level4::scan(in, merge_comp_rng, out, id, outs_lo, convert_reduce_comp_rng, convert_reduce, convert_scan, seq_convert_scan, st);
}

template <
  class Input_iter,
  class Output,
  class Result,
  class Output_iter,
  class Lift_idx_dst,
  class Seq_scan_rng_dst
>
void scan(Input_iter lo,
          Input_iter hi,
          const Output& out,
          Result& id,
          Output_iter outs_lo,
          const Lift_idx_dst& lift_idx_dst,
          const Seq_scan_rng_dst& seq_scan_rng_dst,
          scan_type st) {
  auto output_comp_rng = [&] (const Result* lo, const Result* hi) {
    return hi - lo;
  };
  auto lift_comp_rng = [&] (Input_iter lo, Input_iter hi) {
    return hi - lo;
  };
  scan(lo, hi, output_comp_rng, out, id, outs_lo, lift_comp_rng, lift_idx_dst, seq_scan_rng_dst, st);
}
  
template <class T>
class trivial_output {
public:
  
  using result_type = T;
  
  void init(T&) const {
    
  }
  
  void merge(T&, T&) const {
    
  }
  
};
  
template <class Result, class Combine>
class cell_output {
public:
  
  using result_type = Result;
  using array_type = parray<result_type>;
  using const_iterator = typename array_type::const_iterator;
  
  result_type id;
  Combine combine;
  
  cell_output(result_type id, Combine combine)
  : id(id), combine(combine) { }
  
  cell_output(const cell_output& other)
  : id(other.id), combine(other.combine) { }
  
  void init(result_type& dst) const {
    dst = id;
  }
  
  void copy(const result_type& src, result_type& dst) const {
    dst = src;
  }
  
  void merge(const result_type& src, result_type& dst) const {
    dst = combine(dst, src);
  }
  
  void merge(const_iterator lo, const_iterator hi, result_type& dst) const {
    dst = id;
    for (const_iterator it = lo; it != hi; it++) {
      dst = combine(*it, dst);
    }
  }
  
};
  
template <class Chunked_sequence>
class chunkedseq_output {
public:
  
  using result_type = Chunked_sequence;
  using array_type = parray<result_type>;
  using const_iterator = typename array_type::const_iterator;
  
  result_type id;
  
  chunkedseq_output() { }
  
  void init(result_type& dst) const {
    
  }
  
  void copy(const result_type& src, result_type& dst) const {
    dst = src;
  }
  
  void merge(result_type& src, result_type& dst) const {
    dst.concat(src);
  }
  
  void merge(const_iterator lo, const_iterator hi, result_type& dst) const {
    dst = id;
    for (const_iterator it = lo; it != hi; it++) {
      merge(*it, dst);
    }
  }
  
};
  
} // end namespace

/*---------------------------------------------------------------------*/
/* Level 2 reduction */

namespace level2 {
  
template <
  class Iter,
  class Result,
  class Combine,
  class Lift_comp_rng,
  class Lift_idx,
  class Seq_reduce_rng
>
Result reduce(Iter lo,
              Iter hi,
              Result id,
              const Combine& combine,
              const Lift_comp_rng& lift_comp_rng,
              const Lift_idx& lift_idx,
              const Seq_reduce_rng& seq_reduce_rng) {
  using output_type = level3::cell_output<Result, Combine>;
  output_type out(id, combine);
  Result result;
  auto lift_idx_dst = [&] (size_t pos, reference_of<Iter> x, Result& dst) {
    dst = lift_idx(pos, x);
  };
  auto seq_reduce_rng_dst = [&] (Iter lo, Iter hi, Result& dst) {
    dst = seq_reduce_rng(lo, hi);
  };
  level3::reduce(lo, hi, out, id, result, lift_comp_rng, lift_idx_dst, seq_reduce_rng_dst);
  return result;
}
  
template <
  class Iter,
  class Result,
  class Combine_comp_rng,
  class Combine,
  class Lift_comp_rng,
  class Lift_idx,
  class Seq_scan_rng_dst
>
parray<Result> scan(Iter lo,
                    Iter hi,
                    Result id,
		    const Combine_comp_rng& combine_comp_rng,
                    const Combine& combine,
                    const Lift_comp_rng& lift_comp_rng,
                    const Lift_idx& lift_idx,
                    const Seq_scan_rng_dst& seq_scan_rng_dst,
                    scan_type st) {
  using output_type = level3::cell_output<Result, Combine>;
  auto output_comp_rng = combine_comp_rng;
  output_type out(id, combine);
  parray<Result> results;
  if (std::is_fundamental<Result>::value) {
    results.prefix_tabulate(hi - lo, 0);
  } else {
    results.prefix_tabulate(hi - lo, 0);
  }
  auto outs_lo = results.begin();
  auto lift_idx_dst = [&] (size_t pos, reference_of<Iter> x, Result& dst) {
    dst = lift_idx(pos, x);
  };
  level3::scan(lo, hi, output_comp_rng, out, id, outs_lo, lift_comp_rng, lift_idx_dst, seq_scan_rng_dst, st);
  return results;
}

template <
  class Iter,
  class Result,
  class Combine,
  class Lift_idx,
  class Seq_scan_rng_dst
>
parray<Result> scan(Iter lo,
                    Iter hi,
                    Result id,
                    const Combine& combine,
                    const Lift_idx& lift_idx,
                    const Seq_scan_rng_dst& seq_scan_rng_dst,
                    scan_type st) {
  auto combine_comp_rng = [&] (const Result* lo, const Result* hi) {
    return hi - lo;
  };
  auto lift_comp_rng = [&] (Iter lo, Iter hi) {
    return hi - lo;
  };
  return scan(lo, hi, id, combine_comp_rng, combine, lift_comp_rng, lift_idx, seq_scan_rng_dst, st);
}

namespace dps {

  template <
    class Iter,
    class Result,
    class Output_iter,
    class Combine_comp_rng,
    class Combine,
    class Lift_comp_rng,
    class Lift_idx,
    class Seq_scan_rng_dst
    >
  void scan(Iter lo,
            Iter hi,
            Result& id,
            const Combine_comp_rng& combine_comp_rng,
            const Combine& combine,
            Output_iter outs_lo,
            const Lift_comp_rng& lift_comp_rng,
            const Lift_idx& lift_idx,
            const Seq_scan_rng_dst& seq_scan_rng_dst,
            scan_type st) {
    using output_type = level3::cell_output<Result, Combine>;
    output_type out(id, combine);
    auto lift_idx_dst = [&] (size_t pos, reference_of<Iter> x, Result& dst) {
      dst = lift_idx(pos, x);
    };
    auto output_comp_rng = combine_comp_rng;
    level3::scan(lo, hi, out, id, outs_lo, output_comp_rng, lift_comp_rng, lift_idx_dst, seq_scan_rng_dst, st);
  }

  template <
    class Iter,
    class Result,
    class Output_iter,
    class Combine,
    class Lift_idx,
    class Seq_scan_rng_dst
    >
  void scan(Iter lo,
            Iter hi,
            Result& id,
            const Combine& combine,
            Output_iter outs_lo,
            const Lift_idx& lift_idx,
            const Seq_scan_rng_dst& seq_scan_rng_dst,
            scan_type st) {
    auto combine_comp_rng = [&] (const Result* lo, const Result* hi) {
      return hi - lo;
    };
    auto lift_comp_rng = [&] (Iter lo, Iter hi) {
      return hi - lo;
    };
    scan(lo, hi, id, combine_comp_rng, combine, outs_lo, lift_comp_rng, lift_idx, seq_scan_rng_dst, st);    
  }
  
} // end namespace
  
} // end namespace

/*---------------------------------------------------------------------*/
/* Level 1 reduction */
  
namespace level1 {
  
namespace {
  
template <class Iter, class Item>
class seq_reduce_rng_spec {
public:
  
  template <
    class Result,
    class Combine,
    class Lift_idx
  >
  Result f(size_t i, Iter lo, Iter hi, Result id, const Combine& combine, const Lift_idx& lift_idx) {
    Result r = id;
    for (auto it = lo; it != hi; it++, i++) {
      r = combine(r, lift_idx(i, *it));
    }
    return r;
  }

};

template <class Item>
class seq_reduce_rng_spec<typename pchunkedseq<Item>::iterator, Item> {
public:
  
  using iterator = typename pchunkedseq<Item>::iterator;
  using value_type = Item;
  using reference = reference_of<iterator>;
  
  template <
  class Result,
  class Combine,
  class Lift_idx
  >
  Result f(size_t i, iterator lo, iterator hi, Result id, const Combine& combine, const Lift_idx& lift_idx) {
    Result r = id;
    pasl::data::chunkedseq::extras::for_each(lo, hi, [&] (reference x) {
      r = combine(r, lift_idx(i++, x));
    });
    return r;
  }

};
  
} // end namespace
  
template <
  class Iter,
  class Result,
  class Combine,
  class Lift_comp_rng,
  class Lift_idx
>
Result reducei(Iter lo,
               Iter hi,
               Result id,
               const Combine& combine,
               const Lift_comp_rng& lift_comp_rng,
               const Lift_idx& lift_idx) {
  auto seq_reduce_rng = [&] (Iter _lo, Iter _hi) {
    seq_reduce_rng_spec<Iter, value_type_of<Iter>> f;
    return f.f(_lo - lo, _lo, _hi, id, combine, lift_idx);
  };
  return level2::reduce(lo, hi, id, combine, lift_comp_rng, lift_idx, seq_reduce_rng);
}
  
template <
  class Iter,
  class Result,
  class Combine,
  class Lift_comp_rng,
  class Lift
>
Result reduce(Iter lo,
              Iter hi,
              Result id,
              const Combine& combine,
              const Lift_comp_rng& lift_comp_rng,
              const Lift& lift) {
  auto lift_idx = [&] (size_t pos, reference_of<Iter> x) {
    return lift(x);
  };
  return reducei(lo, hi, id, combine, lift_comp_rng, lift_idx);
}

template <
  class Iter,
  class Result,
  class Combine,
  class Lift
>
Result reduce(Iter lo,
              Iter hi,
              Result id,
              const Combine& combine,
              const Lift& lift) {
  auto lift_comp_rng = [&] (Iter lo, Iter hi) {
    return hi - lo;
  };
  return reduce(lo, hi, id, combine, lift_comp_rng, lift);
}

template <
  class Iter,
  class Result,
  class Combine_comp_rng,
  class Combine,
  class Lift_comp_rng,
  class Lift_idx
>
parray<Result> scani(Iter lo,
                     Iter hi,
                     Result id,
		     const Combine_comp_rng& combine_comp_rng,
                     const Combine& combine,
                     const Lift_comp_rng& lift_comp_rng,
                     const Lift_idx& lift_idx,
                     scan_type st) {
  using output_type = level3::cell_output<Result, Combine>;
  output_type out(id, combine);
  using iterator = typename parray<Result>::iterator;
  auto seq_scan_rng_dst = [&] (Result _id, Iter _lo, Iter _hi, iterator outs_lo) {
    size_t pos = _lo - lo;
    level4::scan_seq(_lo, _hi, outs_lo, out, _id, [&] (reference_of<Iter> src, Result& dst) {
      dst = lift_idx(pos++, src);
    }, st);
  };
  return level2::scan(lo, hi, id, combine_comp_rng, combine, lift_comp_rng, lift_idx, seq_scan_rng_dst, st);
}

template <
  class Iter,
  class Result,
  class Combine_comp_rng,
  class Combine,
  class Lift_comp_rng,
  class Lift
>
parray<Result> scan(Iter lo,
                    Iter hi,
                    Result id,
		    const Combine_comp_rng& combine_comp_rng,
                    const Combine& combine,
                    const Lift_comp_rng& lift_comp_rng,
                    const Lift& lift,
                    scan_type st) {
  auto lift_idx = [&] (size_t pos, reference_of<Iter> x) {
    return lift(x);
  };
  return scani(lo, hi, id, combine_comp_rng, combine, lift_comp_rng, lift_idx, st);
}

template <
  class Iter,
  class Result,
  class Combine,
  class Lift
>
parray<Result> scan(Iter lo,
                    Iter hi,
                    Result id,
                    const Combine& combine,
                    const Lift& lift,
                    scan_type st) {
  auto combine_comp_rng = [&] (const Result* lo, const Result* hi) {
    return hi - lo;
  };
  auto lift_comp_rng = [&] (Iter lo, Iter hi) {
    return hi - lo;
  };
  return scan(lo, hi, id, combine_comp_rng, combine, lift_comp_rng, lift, st);
}

namespace dps {

  template <
    class Iter,
    class Result,
    class Output_iter,
    class Combine_comp_rng,
    class Combine,
    class Lift_comp_rng,
    class Lift_idx
  >
  Result scani(Iter lo,
               Iter hi,
               Result& id,
               const Combine_comp_rng& combine_comp_rng,
               const Combine& combine,
               Output_iter outs_lo,
               const Lift_comp_rng& lift_comp_rng,
               const Lift_idx& lift_idx,
               scan_type st) {
    using output_type = level3::cell_output<Result, Combine>;
    output_type out(id, combine);
    using iterator = typename parray<Result>::iterator;
    auto seq_scan_rng_dst = [&] (Result id, Iter lo, Iter hi, iterator outs_lo) {
      level4::scan_seq(lo, hi, outs_lo, out, id, [&] (reference_of<Iter> src, Result& dst) {
        dst = src;
      }, st);
    };
    if (lo >= hi) {
      return id;
    }
    if (st == forward_inclusive_scan) {
      level2::dps::scan(lo, hi, id, combine_comp_rng, combine, outs_lo, lift_comp_rng, lift_idx, seq_scan_rng_dst, st);
      return *(outs_lo + (hi - lo) - 1);
    } else if (st == backward_inclusive_scan) {
      level2::dps::scan(lo, hi, id, combine_comp_rng, combine, outs_lo, lift_comp_rng, lift_idx, seq_scan_rng_dst, st);
      return *outs_lo;
    } else if (st == forward_exclusive_scan) {
      value_type_of<Iter> v = *(hi - 1);
      level2::dps::scan(lo, hi, id, combine_comp_rng, combine, outs_lo, lift_comp_rng, lift_idx, seq_scan_rng_dst, st);
      return combine(*(outs_lo + (hi - lo) - 1), lift_idx(hi - lo - 1, v));
    } else if (st == backward_exclusive_scan) {
      value_type_of<Iter> v = *lo;
      level2::dps::scan(lo, hi, id, combine_comp_rng, combine, outs_lo, lift_comp_rng, lift_idx, seq_scan_rng_dst, st);
      return combine(*outs_lo, lift_idx(0, v));
    }
    assert(false);
  }

  template <
    class Iter,
    class Result,
    class Output_iter,
    class Combine_comp_rng,
    class Combine,
    class Lift_comp_rng,
    class Lift_idx
  >
  Result scan(Iter lo,
              Iter hi,
              Result& id,
              const Combine_comp_rng& combine_comp_rng,
              const Combine& combine,
              Output_iter outs_lo,
              const Lift_comp_rng& lift_comp_rng,
              const Lift_idx& lift,
              scan_type st) {
    auto lift_idx = [&] (size_t pos, reference_of<Iter> x) {
      return lift(x);
    };
    return scani(lo, hi, id, combine_comp_rng, combine, outs_lo, lift_comp_rng, lift_idx, st);
  }

  template <
    class Iter,
    class Result,
    class Output_iter,
    class Combine,
    class Lift
  >
  Result scan(Iter lo,
              Iter hi,
              Result& id,
              const Combine& combine,
              Output_iter outs_lo,
              const Lift& lift,
              scan_type st) {
    auto combine_comp_rng = [&] (const Result* lo, const Result* hi) {
      return hi - lo;
    };
    auto lift_comp_rng = [&] (Iter lo, Iter hi) {
      return hi - lo;
    };
    scan(lo, hi, id, combine_comp_rng, combine, outs_lo, lift_comp_rng, lift, st);
  }  
  
} // end namespace
  
} // end namespace

/*---------------------------------------------------------------------*/
/* Level 0 reduction */

template <
  class Iter,
  class Item,
  class Combine_comp_rng,
  class Combine
  >
Item reduce(Iter lo,
	    Iter hi,
	    Item id,
	    const Combine_comp_rng& combine_comp_rng,
	    const Combine& combine) {
  auto lift = [&] (reference_of<Iter> x) {
    return x;
  };
  auto lift_comp_rng = combine_comp_rng;
  return level1::reduce(lo, hi, id, combine, lift_comp_rng, lift);
}
  
template <class Iter, class Item, class Combine>
Item reduce(Iter lo, Iter hi, Item id, const Combine& combine) {
  auto combine_comp_rng = [&] (const value_type_of<Iter>* lo, const value_type_of<Iter>* hi) {
    return hi - lo;
  };
  return reduce(lo, hi, id, combine_comp_rng, combine);
}

template <
  class Iter,
  class Item,
  class Combine_comp_rng,
  class Combine
>
parray<Item> scan(Iter lo,
                  Iter hi,
                  Item id,
                  const Combine_comp_rng& combine_comp_rng,
                  const Combine& combine,
                  scan_type st) {
  auto lift_comp_rng = combine_comp_rng;
  auto lift = [&] (reference_of<Iter> x) {
    return x;
  };
  return level1::scan(lo, hi, id, combine_comp_rng, combine, lift_comp_rng, lift, st);
}
  
template <
  class Iter,
  class Item,
  class Combine
>
parray<Item> scan(Iter lo,
                  Iter hi,
                  Item id,
                  const Combine& combine,
		  scan_type st) {
  auto combine_comp_rng = [&] (const value_type_of<Iter>* lo, const value_type_of<Iter>* hi) {
    return hi - lo;
  };
  return scan(lo, hi, id, combine_comp_rng, combine, st);
}

namespace dps {

  template <
    class Iter,
    class Item,
    class Combine_comp_rng,
    class Combine
  >
  Item scan(Iter lo,
            Iter hi,
            Item id,
            const Combine_comp_rng& combine_comp_rng,            
            const Combine& combine,
            Iter outs_lo,
            scan_type st) {
    auto lift_comp_rng = combine_comp_rng;
    auto lift = [&] (reference_of<Iter> x) {
      return x;
    };
    return level1::dps::scan(lo, hi, id, combine_comp_rng, combine, outs_lo, lift, st);
  }

  template <
    class Iter,
    class Item,
    class Combine
  >
  Item scan(Iter lo,
            Iter hi,
            Item id,
            const Combine& combine,
            Iter outs_lo,
            scan_type st) {
    auto combine_comp_rng = [&] (const value_type_of<Iter>* lo, const value_type_of<Iter>* hi) {
      return hi - lo;
    };
    return scan(lo, hi, id, combine_comp_rng, combine, outs_lo, st);
  }
  
} // end namespace
  
} // end namespace

#endif

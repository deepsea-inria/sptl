
#include "spweight.hpp"

#ifndef _SPTL_DATAPAR_H_
#define _SPTL_DATAPAR_H_

namespace sptl {

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
  granularity::spguard([&] { return convert_reduce_comp(in); }, [&] {
    if (! in.can_split()) {
      convert_reduce(in, dst);
    } else {
      Input in2(in);
      in.split(in2);
      Result dst2;
      out.init(dst2);
      granularity::fork2([&] {
        reduce(in,  out, id, dst,  convert_reduce_comp, convert_reduce, seq_convert_reduce, contr);
      }, [&] {
        reduce(in2, out, id, dst2, convert_reduce_comp, convert_reduce, seq_convert_reduce, contr);
      });
      out.merge(dst2, dst);
    }
  }, [&] {
    seq_convert_reduce(in, dst);
  });
}

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

#ifdef SPTL_CONTROL_BY_FORCE_PARALLEL
const long Scan_branching_factor = 2;
#else
const long Scan_branching_factor = DATAPAR_THRESHOLD;
#endif

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

class merge_comp_is_linear {
public:

  static constexpr
  bool value = true;

};

class merge_comp_is_not_linear {
public:

  static constexpr
  bool value = false;

};

template <class Result, class Output, class Merge_comp, class Merge_comp_is_linear>
void scan_rec(const parray<Result>& ins,
              typename parray<Result>::iterator outs_lo,
              const Output& out,
              const Result& id,
              const Merge_comp& merge_comp,
              const Merge_comp_is_linear& merge_comp_is_linear,
              scan_type st) {
  static constexpr
  int k = Scan_branching_factor;
  size_t n = ins.size();
  size_t m = get_nb_blocks(k, n);
  auto loop_comp = [&] (size_t i) {
    auto beg = ins.cbegin();
    size_t lo = get_rng(k, n, i).first;
    size_t hi = get_rng(k, n, i).second;
    return merge_comp(beg+lo, beg+hi);
  };
  granularity::spguard([&] { return merge_comp(ins.cbegin(), ins.cend()); }, [&] {
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
      if (Merge_comp_is_linear::value) {
        parallel_for(0l, m, b1);
      } else {
        parallel_for(0l, m, loop_comp, b1);
      }
      parray<Result> scans;
      if (std::is_fundamental<Result>::value) {
        scans.prefix_tabulate(m, 0);
      } else {
        scans.prefix_tabulate(m, m);
      }
      auto st2 = (is_backward_scan(st)) ? backward_exclusive_scan : forward_exclusive_scan;
      scan_rec(partials, scans.begin(), out, id, merge_comp, merge_comp_is_linear, st2);
      auto b2 = [&] (size_t i) {
        auto ins_beg = ins.cbegin();
        size_t lo = get_rng(k, n, i).first;
        size_t hi = get_rng(k, n, i).second;
        scan_seq(ins_beg+lo, ins_beg+hi, outs_lo+lo, out, scans[i], st);
      };
      if (Merge_comp_is_linear::value) {
        parallel_for(0l, m, b2);
      } else {
        parallel_for(0l, m, loop_comp, b2);
      }
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
  class Merge_comp,
  class Merge_comp_is_linear,
  class Convert_reduce_comp,
  class Convert_reduce,
  class Convert_scan,
  class Seq_convert_scan
>
void scan(Input& in,
          const Output& out,
          Result& id,
          Output_iter outs_lo,
          const Merge_comp& merge_comp,
          const Merge_comp_is_linear& merge_comp_is_linear,
          const Convert_reduce_comp& convert_reduce_comp,
          const Convert_reduce& convert_reduce,
          const Convert_scan& convert_scan,
          const Seq_convert_scan& seq_convert_scan,
          scan_type st) {
  const size_t k = Scan_branching_factor;
  size_t n = in.size();
  size_t m = get_nb_blocks(k, n);
  auto loop_comp = [&] (size_t i) {
    size_t lo = get_rng(k, n, i).first;
    size_t hi = get_rng(k, n, i).second;
    return convert_reduce_comp(lo, hi);
  };
  granularity::spguard([&] { return convert_reduce_comp(0l, n); }, [&] {
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
      if (Merge_comp_is_linear::value) {
        parallel_for(0l, m, b1);
      } else {
        parallel_for(0l, m, loop_comp, b1);
      }
      parray<Result> scans;
      if (std::is_fundamental<Result>::value) {
        scans.prefix_tabulate(m, 0);
      } else {
        scans.prefix_tabulate(m, m);
      }
      auto st2 = (is_backward_scan(st)) ? backward_exclusive_scan : forward_exclusive_scan;
      scan_rec(partials, scans.begin(), out, id, merge_comp, merge_comp_is_linear, st2);
      auto b2 = [&] (size_t i) {
        size_t lo = get_rng(k, n, i).first;
        size_t hi = get_rng(k, n, i).second;
        Input in2 = in.slice(splits, lo, hi);
        scan(in2, out, scans[i], outs_lo+lo, merge_comp, convert_reduce_comp, convert_reduce, convert_scan, seq_convert_scan, st);
      };
      if (Merge_comp_is_linear::value) {
        parallel_for(0l, m, b2);
      } else {
        parallel_for(0l, m, loop_comp, b2);
      }
    }
  }, [&] {
    seq_convert_scan(id, in, outs_lo);
  });
}
  
} // end namespace
  
} // end namespace

#endif

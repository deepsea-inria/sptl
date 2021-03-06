
#include "spreduce.hpp"
#include "spsums.hpp"

#ifndef _SPTL_PCHUNKEDSEQ_H_
#define _SPTL_PCHUNKEDSEQ_H_

namespace sptl {
namespace chunked {

template <
  class Input_iter,
  class Output,
  class Result,
  class Lift_comp_rng,
  class Lift_rng_dst,
  class Seq_reduce_rng_dst
>
void reduce(Input_iter lo,
            Input_iter hi,
            Output& out,
            Result& id,
            Result& dst,
            const Lift_comp_rng& lift_comp_rng,
            const Lift_rng_dst& lift_rng_dst,
            const Seq_reduce_rng_dst& seq_reduce_rng_dst) {
  using input_type = level4::random_access_iterator_input<Input_iter>;
  using pointer = pointer_of<Input_iter>;
  input_type in(lo, hi);
  auto convert_reduce_comp = [&] (input_type& in) {
    return lift_comp_rng(in.lo, in.hi);
  };
  auto convert_reduce = [&] (input_type& in, Result& dst) {
    size_type i = in.lo - lo;
    pasl::data::chunkedseq::extras::for_each_segment(in.lo, in.hi, [&] (pointer lo, pointer hi) {
      Result tmp;
      lift_rng_dst(i, lo, hi, tmp);
      out.merge(tmp, dst);
      i += hi - lo;
    });
  };
  auto seq_convert_reduce = [&] (input_type& in, Result& dst) {
    size_type i = in.lo - lo;
    pasl::data::chunkedseq::extras::for_each_segment(in.lo, in.hi, [&] (pointer lo, pointer hi) {
      Result tmp;
      seq_reduce_rng_dst(i, lo, hi, tmp);
      out.merge(tmp, dst);
      i += hi - lo;
    });
  };
  level4::reduce(in, out, id, dst, convert_reduce_comp, convert_reduce, seq_convert_reduce);
}
  
template <class Iter, class Chunkedseq>
void copy_dst(Iter lo, Iter hi, Chunkedseq& dst) {
  using value_type = value_type_of<Iter>;
  using pointer = pointer_of<Iter>;
  using output_type = level3::chunkedseq_output<Chunkedseq>;
  Chunkedseq id;
  output_type out;
  auto lift_comp_rng = [&] (Iter lo, Iter hi) {
    return hi - lo;
  };
  auto lift_rng_dst = [&] (size_type i, pointer lo, pointer hi, Chunkedseq& dst) {
    dst.pushn_back(lo, hi - lo);
  };
  auto seq_rng_dst = lift_rng_dst;
  reduce(lo, hi, out, id, dst, lift_comp_rng, lift_rng_dst, seq_rng_dst);
}
  
template <class Item, class Chunkedseq>
void fill_dst(size_type n, const Item& x, Chunkedseq& dst) {
  using value_type = Item;
  using output_type = level3::chunkedseq_output<Chunkedseq>;
  using input_type = level4::tabulate_input;
  Chunkedseq result;
  Chunkedseq id;
  output_type out;
  input_type in(0, n);
  auto convert_reduce = [&] (input_type& in, Chunkedseq& dst) {
    for (auto i = in.lo; i != in.hi; i++) {
      dst.push_back(x);
    }
  };
  auto seq_convert_reduce = convert_reduce;
  level4::reduce(in, out, id, dst, convert_reduce, seq_convert_reduce);
}
 
template <class Iter, class Visit_segment_idx>
void for_each_segmenti(Iter lo, Iter hi, const Visit_segment_idx& visit_segment_idx) {
  using output_type = level3::trivial_output<int>;
  output_type out;
  int id = 0;
  int result = id;
  auto lift_rng_dst = [&] (size_type i, pointer_of<Iter> lo, pointer_of<Iter> hi, int&) {
    visit_segment_idx(i, lo, hi);
  };
  auto seq_rng_dst = lift_rng_dst;
  reduce(lo, hi, out, id, result, lift_rng_dst, seq_rng_dst);
}
 
template <class Iter, class Visit_segment>
void for_each_segment(Iter lo, Iter hi, const Visit_segment& visit_segment) {
  using pointer = pointer_of<Iter>;
  for_each_segmenti(lo, hi, [&] (size_type, pointer lo, pointer hi) {
    visit_segment(lo, hi);
  });
}

template <class Iter, class Visit_item>
void for_each(Iter lo, Iter hi, const Visit_item& visit_item) {
  using pointer = pointer_of<Iter>;
  for_each_segment(lo, hi, [&] (pointer lo, pointer hi) {
    for (auto it = lo; it != hi; it++) {
      visit_item(*it);
    }
  });
}

template <class Chunkedseq>
void clear(Chunkedseq& seq) {
  using input_type = level4::chunkedseq_input<Chunkedseq>;
  using output_type = level3::trivial_output<int>;
  input_type in(seq);
  output_type out;
  auto convert =  [&] (input_type& in, int&) {
    in.seq.clear();
  };
  int dummy;
  level4::reduce(in, out, dummy, dummy, convert, convert);
}
  
template <class Chunkedseq, class Body_comp_rng, class Body_idx_dst>
void tabulate_rng_dst(size_type n,
                      const Body_comp_rng& body_comp_rng,
                      Chunkedseq& dst,
                      const Body_idx_dst& body_idx_dst) {
  using input_type = level4::tabulate_input;
  using output_type = level3::chunkedseq_output<Chunkedseq>;
  using value_type = typename Chunkedseq::value_type;
  input_type in(0, n);
  output_type out;
  Chunkedseq id;
  auto convert_comp = [&] (input_type& in) {
    return body_comp_rng(in.lo, in.hi);
  };
  size_type chunk_capacity = dst.chunk_capacity;
  parray<value_type> tmp(chunk_capacity);
  auto convert = [&] (input_type& in, Chunkedseq& dst) {
    dst.stream_pushn_back([&] (size_type i, size_type n) {
      for (size_type k = 0; k < n; k++) {
        body_idx_dst(k + in.lo, tmp[k]);
      }
      const value_type* lo = &tmp[0];
      const value_type* hi = &tmp[n-1]+1;
      return std::make_pair(lo, hi);
    }, in.hi - in.lo);
  };
  level4::reduce(in, out, id, dst, convert_comp, convert, convert);
}

template <class Chunkedseq, class Body_comp, class Body_idx_dst>
void tabulate_dst(size_type n,
                  const Body_comp& body_comp,
                  Chunkedseq& dst,
                  const Body_idx_dst& body_idx_dst) {
  parray<size_type> w = sums(n, [&] (size_type i) {
    return body_comp(i);
  });
  auto body_comp_rng = [&] (size_type lo, size_type hi) {
    return w[hi] - w[lo];
  };
  tabulate_rng_dst(n, body_comp_rng, dst, body_idx_dst);
}

template <class Chunkedseq, class Body_idx_dst>
void tabulate_dst(size_type n,
                  Chunkedseq& dst,
                  const Body_idx_dst& body_idx_dst) {
  auto body_comp_rng = [&] (size_type lo, size_type hi) {
    return hi - lo;
  };
  tabulate_rng_dst(n, body_comp_rng, dst, body_idx_dst);
}
  
template <class Pred, class Chunkedseq>
void keep_if(const Pred& p, Chunkedseq& xs, Chunkedseq& dst) {
  using input_type = level4::chunkedseq_input<Chunkedseq>;
  using output_type = level3::chunkedseq_output<Chunkedseq>;
  using value_type = typename Chunkedseq::value_type;
  input_type in(xs);
  output_type out;
  Chunkedseq id;
  auto convert_reduce = [&] (input_type& in, Chunkedseq& dst) {
    while (! in.seq.empty()) {
      value_type v = in.seq.pop_back();
      if (p(v)) {
        dst.push_front(v);
      }
    }
  };
  auto seq_convert_reduce = convert_reduce;
  level4::reduce(in, out, id, dst, convert_reduce, seq_convert_reduce);
}

} // end namespace  
} // end namespace

#endif

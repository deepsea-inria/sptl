
#include "spparray.hpp"
#include "chunkedseq.hpp"

#ifndef _SPTL_PCHUNKEDSEQBASE_H_
#define _SPTL_PCHUNKEDSEQBASE_H_

namespace sptl {

/*---------------------------------------------------------------------*/
/* Forward declarations */
  
template <class Item /*, class Alloc = std::allocator<Item>*/ >
class pchunkedseq;

namespace chunked {
  
template <class Iter, class Chunkedseq>
void copy_dst(Iter lo, Iter hi, Chunkedseq& dst);
  
template <class Iter, class Visit_segment_idx>
void for_each_segmenti(Iter lo, Iter hi, const Visit_segment_idx& visit_segment_idx);
  
template <class Chunkedseq>
void clear(Chunkedseq& seq);
  
template <class Item, class Chunkedseq>
void fill_dst(size_t n, const Item& x, Chunkedseq& dst);
  
template <class Chunkedseq, class Body_comp_rng, class Body_idx_dst>
void tabulate_rng_dst(size_t n,
                      const Body_comp_rng& body_comp_rng,
                      Chunkedseq& dst,
                      const Body_idx_dst& body_idx_dst);

template <class Chunkedseq, class Body_comp, class Body_idx_dst>
void tabulate_dst(size_t n,
                  const Body_comp& body_comp,
                  Chunkedseq& dst,
                  const Body_idx_dst& body_idx_dst);

template <class Chunkedseq, class Body_idx_dst>
void tabulate_dst(size_t n,
                  Chunkedseq& dst,
                  const Body_idx_dst& body_idx_dst);
  
template <class Pred, class Chunkedseq>
void keep_if(const Pred& p, Chunkedseq& xs, Chunkedseq& dst);
  
} // end namespace
  
/*---------------------------------------------------------------------*/
/* Parallel chunked sequence */
  
template <class Item /*, class Alloc = std::allocator<Item>*/ >
class pchunkedseq {
public:
  
  using seq_type = pasl::data::chunkedseq::bootstrapped::deque<Item>;
  
  using value_type = Item;
  using allocator_type = std::allocator<Item>; //Alloc;
  using size_type = std::size_t;
  using ptr_diff = std::ptrdiff_t;
  using reference = value_type&;
  using const_reference = const value_type&;
  using pointer = value_type*;
  using const_pointer = const value_type*;
  using iterator = typename seq_type::iterator;
  using const_iterator = typename seq_type::const_iterator;
  using segment_type = typename seq_type::segment_type;
  using const_segment_type = typename seq_type::const_segment_type;
  
  seq_type seq;
  
private:
  
  void fill(size_t n, const value_type& val) {
    if (n == 0) {
      return;
    }
    chunked::fill_dst(n, val, seq);
  }
  
public:
  
  pchunkedseq(size_t n = 0) {
    value_type val;
    fill(n, val);
  }
  
  pchunkedseq(size_t n, const value_type& val) {
    fill(n, val);
  }
  
  template <class Body>
  pchunkedseq(size_t sz, const Body& body) {
    chunked::tabulate_dst(sz, seq, [&] (size_t i, reference dst) {
      dst = body(i);
    });
  }
  
  template <class Body_cmp, class Body>
  pchunkedseq(size_t sz,
              const Body_cmp& body_comp,
              const Body& body) {
    chunked::tabulate_dst(sz, seq, body_comp, [&] (size_t i, reference dst) {
      dst = body(i);
    });
  }
  
  pchunkedseq(std::initializer_list<value_type> xs)
  : seq(xs) { }
  
  ~pchunkedseq() {
    clear();
  }
  
  pchunkedseq(const pchunkedseq& other) {
    chunked::copy_dst(other.cbegin(), other.cend(), seq);
  }
  
  pchunkedseq(pchunkedseq&& other)
  : seq(std::move(other.seq)) { }
  
  pchunkedseq& operator=(pchunkedseq&& other) {
    new (&seq) seq_type(std::move(other.seq));
    return *this;
  }
  
  void clear() {
    chunked::clear(seq);
  }
  
  iterator begin() const {
    return seq.begin();
  }
  
  const_iterator cbegin() const {
    return seq.cbegin();
  }
  
  iterator end() const {
    return seq.end();
  }
  
  const_iterator cend() const {
    return seq.cend();
  }
  
  void swap(pchunkedseq<Item>& other) {
    seq.swap(other.seq);
  }
  
  template <class Pred>
  void keep_if(const Pred& p) {
    chunked::keep_if(p, seq, seq);
  }
  
};
  
} // end namespace

#endif

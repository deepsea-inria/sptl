
#include <ostream>

#include "spparray.hpp"
#include "sppstring.hpp"
#include "sppchunkedseqbase.hpp"

#ifndef _SPTL_IO_H_
#define _SPTL_IO_H_

namespace sptl {

template <class Item>
std::ostream& operator<<(std::ostream& out, const parray<Item>& xs) {
  out << "{ ";
  size_type sz = xs.size();
  for (size_type i = 0; i < sz; i++) {
    out << xs[i];
    if (i+1 < sz)
      out << ", ";
  }
  out << " }";
  return out;
}

std::ostream& operator<<(std::ostream& out, const pstring& xs) {
  for (auto it = xs.cbegin(); it != xs.cend(); it++) {
    out << *it;
  }
  return out;
}

template <class value_type, int chunk_capacity, class cache_type>
std::ostream& operator<<(std::ostream& out, const pasl::data::chunkedseq::bootstrapped::deque<value_type, chunk_capacity, cache_type>& xs) {
  out << "{ ";
  for (auto it = xs.cbegin(); it != xs.cend(); it++) {
    auto x = *it;
    out << x;
    if (it+1 != xs.cend())
      out << ", ";
  }
  out << " }";
  return out;
}
  
} // end namespace

#endif

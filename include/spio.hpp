
#include <ostream>

#include "spparray.hpp"
#include "sppstring.hpp"

#ifndef _SPTL_IO_H_
#define _SPTL_IO_H_

namespace sptl {

template <class Item>
std::ostream& operator<<(std::ostream& out, const parray<Item>& xs) {
  out << "{ ";
  size_t sz = xs.size();
  for (size_t i = 0; i < sz; i++) {
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

} // end namespace

#endif

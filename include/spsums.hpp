
#include "sploop.hpp"
#include "spparray.hpp"

#ifndef _SPTL_SUMS_H_
#define _SPTL_SUMS_H_

namespace sptl {

namespace {

static inline
size_type seq(const size_type* lo, const size_type* hi, size_type id, size_type* dst) {
  size_type x = id;
  for (const size_type* i = lo; i != hi; i++) {
    *dst = x;
    dst++;
    x += *i;
  }
  return x;
}
  
static inline
parray<size_type> rec(const parray<size_type>& xs) {
  static constexpr
  size_type k = 1024;
  size_type n = xs.size();
  size_type m = 1 + ((n - 1) / k);
  parray<size_type> rs(n);
  spguard([&] { return n; }, [&] {
    if (n <= k) {
      seq(xs.cbegin(), xs.cend(), 0, rs.begin());
    } else {
      parray<size_type> sums(m);
      parallel_for((size_type)0, m, [&] (size_type i) {
        size_type lo = i * k;
        size_type hi = std::min(lo + k, n);
        sums[i] = 0;
        for (size_type j = lo; j < hi; j++) {
          sums[i] += xs[j];
        }
      });
      parray<size_type> scans = rec(sums);
      parallel_for((size_type)0, m, [&] (size_type i) {
        size_type lo = i * k;
        size_type hi = std::min(lo + k, n);
        seq(xs.cbegin()+lo, xs.cbegin()+hi, scans[i], rs.begin()+lo);
      });
    }
  }, [&] {
    seq(xs.cbegin(), xs.cend(), 0, rs.begin());
  });
  return rs;
}
  
template <class Size>
size_type sums_seq(const Size& size, size_type lo, size_type hi, size_type id, size_type* dst) {
  size_type x = id;
  for (size_type i = lo; i != hi; i++) {
    *dst = x;
    dst++;
    x += size(i);
  }
  return x;
}

} // end namespace

template <class Size>
parray<size_type> sums(size_type n, const Size& size) {
  static constexpr
  size_type k = 1024;
  size_type m = 1 + ((n - 1) / k);
  size_type tot;
  parray<size_type> rs(n + 1);
  spguard([&] { return n; }, [&] {
    if (n <= k) {
      tot = sums_seq(size, 0, n, 0, rs.begin());
    } else {
      parray<size_type> sums(m);
      parallel_for((size_type)0, m, [&] (size_type i) {
        size_type lo = i * k;
        size_type hi = std::min(lo + k, n);
        sums[i] = 0;
        for (size_type j = lo; j < hi; j++) {
          sums[i] += size(j);
        }
      });
      parray<size_type> scans = rec(sums);
      parallel_for((size_type)0, m, [&] (size_type i) {
        size_type lo = i * k;
        size_type hi = std::min(lo + k, n);
        sums_seq(size, lo, hi, scans[i], rs.begin()+lo);
      });
      tot = rs[n-1] + size(n-1);
    }
  }, [&] {
    tot = sums_seq(size, 0, n, 0, rs.begin());
  });
  rs[n] = tot;
  return rs;
}

} // end namespace

#endif

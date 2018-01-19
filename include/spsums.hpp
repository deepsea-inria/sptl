
#include "sploop.hpp"
#include "spparray.hpp"

#ifndef _SPTL_SUMS_H_
#define _SPTL_SUMS_H_

namespace sptl {

namespace {

static inline
size_t seq(const size_t* lo, const size_t* hi, size_t id, size_t* dst) {
  size_t x = id;
  for (const size_t* i = lo; i != hi; i++) {
    *dst = x;
    dst++;
    x += *i;
  }
  return x;
}
  
static inline
parray<size_t> rec(const parray<size_t>& xs) {
  const size_t k = 1024;
  size_t n = xs.size();
  size_t m = 1 + ((n - 1) / k);
  parray<size_t> rs(n);
  spguard([&] { return n; }, [&] {
    if (n <= k) {
      seq(xs.cbegin(), xs.cend(), 0, rs.begin());
    } else {
      parray<size_t> sums(m);
      parallel_for((size_t)0, m, [&] (size_t i) {
        size_t lo = i * k;
        size_t hi = std::min(lo + k, n);
        sums[i] = 0;
        for (size_t j = lo; j < hi; j++) {
          sums[i] += xs[j];
        }
      });
      parray<size_t> scans = rec(sums);
      parallel_for((size_t)0, m, [&] (size_t i) {
        size_t lo = i * k;
        size_t hi = std::min(lo + k, n);
        seq(xs.cbegin()+lo, xs.cbegin()+hi, scans[i], rs.begin()+lo);
      });
    }
  }, [&] {
    seq(xs.cbegin(), xs.cend(), 0, rs.begin());
  });
  return rs;
}
  
template <class Size>
size_t sums_seq(const Size& size, size_t lo, size_t hi, size_t id, size_t* dst) {
  size_t x = id;
  for (size_t i = lo; i != hi; i++) {
    *dst = x;
    dst++;
    x += size(i);
  }
  return x;
}

} // end namespace

template <class Size>
parray<size_t> sums(size_t n, const Size& size) {
  const size_t k = 1024;
  size_t m = 1 + ((n - 1) / k);
  size_t tot;
  parray<size_t> rs(n + 1);
  spguard([&] { return n; }, [&] {
    if (n <= k) {
      tot = sums_seq(size, 0, n, 0, rs.begin());
    } else {
      parray<size_t> sums(m);
      parallel_for((size_t)0, m, [&] (size_t i) {
        size_t lo = i * k;
        size_t hi = std::min(lo + k, n);
        sums[i] = 0;
        for (size_t j = lo; j < hi; j++) {
          sums[i] += size(j);
        }
      });
      parray<size_t> scans = rec(sums);
      parallel_for((size_t)0, m, [&] (size_t i) {
        size_t lo = i * k;
        size_t hi = std::min(lo + k, n);
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

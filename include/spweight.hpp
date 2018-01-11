
#include "spparray.hpp"

#ifndef _SPTL_WEIGHT_H_
#define _SPTL_WEIGHT_H_

namespace sptl {

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
  granularity::spguard([&] { return n; }, [&] {
    if (n <= k) {
      seq(xs.cbegin(), xs.cend(), 0, rs.begin());
    } else {
      parray<size_t> sums(m);
      parallel_for(0l, m, [&] (size_t i) {
        size_t lo = i * k;
        size_t hi = std::min(lo + k, n);
        sums[i] = 0;
        for (size_t j = lo; j < hi; j++) {
          sums[i] += xs[j];
        }
      });
      parray<size_t> scans = rec(sums);
      parallel_for(0l, m, [&] (size_t i) {
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
  
template <class Weight>
size_t weights_seq(const Weight& weight, size_t lo, size_t hi, size_t id, size_t* dst) {
  size_t x = id;
  for (size_t i = lo; i != hi; i++) {
    *dst = x;
    dst++;
    x += weight(i);
  }
  return x;
}

template <class Weight>
parray<size_t> weights(size_t n, const Weight& weight) {
  const size_t k = 1024;
  size_t m = 1 + ((n - 1) / k);
  size_t tot;
  parray<size_t> rs(n + 1);
  granularity::spguard([&] { return n; }, [&] {
    if (n <= k) {
      tot = weights_seq(weight, 0, n, 0, rs.begin());
    } else {
      parray<size_t> sums(m);
      parallel_for(0l, m, [&] (size_t i) {
        size_t lo = i * k;
        size_t hi = std::min(lo + k, n);
        sums[i] = 0;
        for (size_t j = lo; j < hi; j++) {
          sums[i] += weight(j);
        }
      });
      parray<size_t> scans = rec(sums);
      parallel_for(0l, m, [&] (size_t i) {
        size_t lo = i * k;
        size_t hi = std::min(lo + k, n);
        weights_seq(weight, lo, hi, scans[i], rs.begin()+lo);
      });
      tot = rs[n-1] + weight(n-1);
    }
  }, [&] {
    tot = weights_seq(weight, 0, n, 0, rs.begin());
  });
  rs[n] = tot;
  return rs;
}

template <class Iter, class Body, class Comp>
void parallel_for(Iter lo, Iter hi, const Comp& comp, const Body& body) {
  parray<size_t> w = weights(hi - lo, comp);
  auto comp_rng = [&] (Iter lo2, Iter hi2) {
    auto lo3 = lo2 - lo;
    auto hi3 = hi2 - lo;
    return w[hi3] - w[lo3];
  };
  range::parallel_for(lo, hi, comp_rng, body);
}
  
} // end namespace

#endif

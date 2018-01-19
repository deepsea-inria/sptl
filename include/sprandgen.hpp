
#include "sppchunkedseq.hpp"

#ifndef _SPTL_RANDGEN_H_
#define _SPTL_RANDGEN_H_

namespace sptl {

static inline
unsigned int hashu(unsigned int a) {
  a = (a+0x7ed55d16) + (a<<12);
  a = (a^0xc761c23c) ^ (a>>19);
  a = (a+0x165667b1) + (a<<5);
  a = (a+0xd3a2646c) ^ (a<<9);
  a = (a+0xfd7046c5) + (a<<3);
  a = (a^0xb55a4f09) ^ (a>>16);
  return a;
}
  
static constexpr
int hash_max_int = ((unsigned) 1 << 31);

static inline
int hashi(int i) {
  return hashu(i) & (hash_max_int-1);
}

static inline
double hashd(int i) {
  return ((double) hashi(i)/((double) hash_max_int));
}

template <class T> T hash(int i) {
  if (typeid(T) == typeid(int) || typeid(T) == typeid(long) || typeid(T) == typeid(size_t)) {
    return hashi(i);
  } else if (typeid(T) == typeid(unsigned int)) {
    return hashu(i);
  } else if (typeid(T) == typeid(double)) {
    return hashd(i);
  } else {
    //pasl::util::atomic::die("bogus");
    return 0;
  }
}
  
/*---------------------------------------------------------------------*/
/* General-purpose container generators */
  
template <class Item, class Generator>
parray<Item> gen_parray(size_t n, const Generator& g) {
  parray<Item> tmp(n, [&] (size_t i) {
    return g(i, hashu((unsigned int)i));
  });
  return tmp;
}
  
template <class Item, class Generator>
pchunkedseq<Item> gen_pchunkedseq(size_t n, const Generator& g) {
  pchunkedseq<Item> tmp(n, [&] (size_t i) {
    return g(i, hashu((unsigned int)i));
  });
  return tmp;
}

/*---------------------------------------------------------------------*/
/* Generators for containers of integral values */
  
template <class Integ>
Integ in_range(Integ val, Integ lo, Integ hi) {
  Integ n = hi - lo;
  Integ val2 = (val % n + n) % n;
  return val2 + lo;
}

template <class Integ>
parray<Integ> gen_integ_parray(size_t n, Integ lo, Integ hi) {
  return gen_parray<Integ>(n, [&] (size_t, int hash) {
    return in_range((Integ)hash, lo, hi);
  });
}

template <class Integ>
pchunkedseq<Integ> gen_integ_pchunkedseq(size_t n, Integ lo, Integ hi) {
  return gen_pchunkedseq<Integ>(n, [&] (size_t, int hash) {
    return in_range((Integ)hash, lo, hi);
  });
}

} // end namespace

#endif

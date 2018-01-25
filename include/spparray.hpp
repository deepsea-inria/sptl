
#include <memory>

#include "spmemory.hpp"

#ifndef _SPTL_PARRAY_H_
#define _SPTL_PARRAY_H_

namespace sptl {

template <class Item, class Alloc = std::allocator<Item>>
class parray {
public:
  
  using value_type = Item;
  using allocator_type = Alloc;
  using size_type = std::size_t;
  using ptr_diff = std::ptrdiff_t;
  using reference = value_type&;
  using const_reference = const value_type&;
  using pointer = value_type*;
  using const_pointer = const value_type*;
  using iterator = pointer;
  using const_iterator = const_pointer;
  
private:
  
  class Deleter {
  public:
    void operator()(value_type* ptr) {
      free(ptr);
    }
  };
  
  std::unique_ptr<value_type[], Deleter> ptr;
  
  size_t sz = 0;
  
  void alloc(size_t n) {
    sz = n;
    assert(sz >= 0);
    value_type* p = (sz == 0) ? nullptr : (value_type*)malloc(sz * sizeof(value_type));
    ptr.reset(p);
  }
  
  void destroy() {
    if (! ptr) {
      pdelete<Item, Alloc>(begin(), end());
    }
    sz = 0;
  }
  
  void realloc(size_t n) {
    destroy();
    alloc(n);
  }
  
  void fill(size_t n, const value_type& val) {
    realloc(n);
    if (n == 0) {
      return;
    }
    sptl::fill(begin(), end(), val);
  }
  
  void check(size_t i) const {
    assert(ptr != nullptr);
    assert(i >= 0);
    assert(i < sz);
  }
  
public:
  
  parray(size_t sz = 0) {
    value_type val;
    fill(sz, val);
  }

  parray(size_t sz, const value_type& val) {
    fill(sz, val);
  }

  template <class Body>
  parray(size_t sz, const Body& body)
  : sz(0) {
    tabulate(sz, body);
  }
    
  parray(std::initializer_list<value_type> xs) {
    alloc(xs.size());
    if (sz == 0) {
      return;
    }
    size_t i = 0;
    for (auto it = xs.begin(); it != xs.end(); it++) {
      new (&ptr[i++]) value_type(*it);
    }
  }
  
  parray(const parray& other) {
    alloc(other.size());
    if (sz == 0) {
      return;
    }
    copy(other.cbegin(), other.cend(), begin());
  }
  
  parray(iterator lo, iterator hi) {
    size_t n = hi - lo;
    alloc(n);
    if (n == 0) {
      return;
    }
    copy(lo, hi, begin());
  }
  
  ~parray() {
    destroy();
  }
  
  parray& operator=(const parray& other) {
    if (&other == this) {
      return *this;
    }
    realloc(other.size());
    if (sz != 0) {
      copy(other.cbegin(), other.cend(), begin());
    }
    return *this;
  }
  
  parray& operator=(parray&& other) {
    ptr = std::move(other.ptr);
    sz = std::move(other.sz);
    other.sz = 0;
    return *this;
  }
  
  value_type& operator[](size_t i) {
    check(i);
    return ptr[i];
  }
  
  const value_type& operator[](size_t i) const {
    check(i);
    return ptr[i];
  }
  
  size_t size() const {
    return sz;
  }
  
  void swap(parray& other) {
    ptr.swap(other.ptr);
    std::swap(sz, other.sz);
  }
  
  void resize(size_t n, size_t init_sz, const value_type& val) {
    if (n == sz) {
      return;
    }
    init_sz = std::min(n, init_sz);
    parray<Item> tmp;
    tmp.prefix_tabulate(n, 0);
    swap(tmp);
    if (n == 0) {
      return;
    }
    copy(tmp.begin(), tmp.begin() + std::min(n, sz), begin());
    if (init_sz != n) {
      sptl::fill(begin() + std::min(n, sz), begin() + init_sz, val);
    }
  }

  void resize(size_t n, const value_type& val) {
    if (n == sz) {
      return;
    }
    parray<Item> tmp(n, val);
    swap(tmp);
    if (n == 0) {
      return;
    }
    size_t m = std::min(tmp.size(), size());
    assert(size() >= m);
    copy(tmp.cbegin(), tmp.cbegin()+m, begin());
  }
  
  void resize(size_t n) {
    value_type val;
    resize(n, val);
  }

  void clear() {
    realloc(0);
  }
  
  template <class Body>
  void tabulate(size_t n, const Body& body) {
    realloc(n);
    if (n == 0) {
      return;
    }
    auto ptr = this->ptr.get();
    auto body_comp = [&] (size_t lo, size_t hi) {
      return hi - lo;
    };
    parallel_for((size_t)0, n, body_comp, [&, ptr] (size_t i) {
      ptr[i] = body(i);
    }, [&, ptr] (size_t lo, size_t hi) {
      for (size_t i = lo; i < hi; i++) {
        ptr[i] = body(i);
      }
    });
  }
    
  iterator begin() const {
    return &ptr[0];
  }
  
  const_iterator cbegin() const {
    return &ptr[0];
  }
  
  iterator end() const {
    return &ptr[size()];
  }
  
  const_iterator cend() const {
    return &ptr[size()];
  }
  
};
  
} // end namespace

#endif

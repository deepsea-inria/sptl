
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
  using size_type = sptl::size_type;
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
  
  size_type sz = 0;
  
  void alloc(size_type n) {
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
  
  void realloc(size_type n) {
    destroy();
    alloc(n);
  }
  
  void fill(size_type n, const value_type& val) {
    realloc(n);
    if (n == 0) {
      return;
    }
    sptl::fill(begin(), end(), val);
  }
  
  void check(size_type i) const {
    assert(ptr != nullptr);
    assert(i >= 0);
    assert(i < sz);
  }
  
public:
  
  parray(size_type sz = 0) {
    value_type val;
    fill(sz, val);
  }

  parray(size_type sz, const value_type& val) {
    fill(sz, val);
  }

  template <class Body>
  parray(size_type sz, const Body& body)
  : sz(0) {
    tabulate(sz, body);
  }
    
  parray(std::initializer_list<value_type> xs) {
    alloc(xs.size());
    if (sz == 0) {
      return;
    }
    size_type i = 0;
    for (auto it = xs.begin(); it != xs.end(); it++) {
      new (&ptr[i++]) value_type(*it);
    }
  }
  
  parray(const parray& other) {
    alloc(other.size());
    if (sz == 0) {
      return;
    }
    sptl::copy(other.cbegin(), other.cend(), begin());
  }
  
  parray(iterator lo, iterator hi) {
    size_type n = hi - lo;
    alloc(n);
    if (n == 0) {
      return;
    }
    sptl::copy(lo, hi, begin());
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
      sptl::copy(other.cbegin(), other.cend(), begin());
    }
    return *this;
  }
  
  parray& operator=(parray&& other) {
    ptr = std::move(other.ptr);
    sz = std::move(other.sz);
    other.sz = 0;
    return *this;
  }
  
  value_type& operator[](size_type i) {
    check(i);
    return ptr[i];
  }
  
  const value_type& operator[](size_type i) const {
    check(i);
    return ptr[i];
  }
  
  size_type size() const {
    return sz;
  }
  
  void swap(parray& other) {
    ptr.swap(other.ptr);
    std::swap(sz, other.sz);
  }
  
  void resize(size_type n, const value_type& val) {
    if (n == sz) {
      return;
    }
    parray<Item> tmp(n, val);
    swap(tmp);
    if (n == 0) {
      return;
    }
    size_type m = std::min(tmp.size(), size());
    assert(size() >= m);
    sptl::copy(tmp.cbegin(), tmp.cbegin()+m, begin());
  }
  
  void resize(size_type n) {
    value_type val;
    resize(n, val);
  }

  void clear() {
    realloc(0);
  }
  
  template <class Body>
  void tabulate(size_type n, const Body& body) {
    realloc(n);
    if (n == 0) {
      return;
    }
    auto ptr = this->ptr.get();
    auto body_comp = [&] (size_type lo, size_type hi) {
      return hi - lo;
    };
    parallel_for((size_type)0, n, body_comp, [&, ptr] (size_type i) {
      ptr[i] = body(i);
    }, [&, ptr] (size_type lo, size_type hi) {
      for (size_type i = lo; i < hi; i++) {
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

  void reset(size_type n, pointer p) {
    if (std::is_fundamental<value_type>::value) {
      sz = n;
      ptr.reset(p);
    } else {
      free(p);
      resize(n);
    }
  }

  void reset(size_type n) {
    pointer p = (pointer)malloc(n * sizeof(value_type));
    reset(n, p);
  }
  
};
  
} // end namespace

#endif

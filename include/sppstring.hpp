
#include "spparray.hpp"

#ifndef _SPTL_PSTRING_H_
#define _SPTL_PSTRING_H_

namespace sptl {

class pstring {
private:
  
  using parray_type = parray<char>;
  
public:
  
  using value_type = char;
  using allocator_type = std::allocator<char>;
  using size_type = sptl::size_type;
  using ptr_diff = std::ptrdiff_t;
  using reference = value_type&;
  using const_reference = const value_type&;
  using pointer = value_type*;
  using const_pointer = const value_type*;
  using iterator = typename parray_type::iterator;
  using const_iterator = typename parray_type::const_iterator;
  
  parray_type chars;
  
private:
  
  void make_null_terminated() {
    chars[size()] = '\0';
  }
  
  void init() {
    make_null_terminated();
  }
  
  void check(size_type i) const {
    assert(i < size());
  }
  
public:
  
  pstring(size_type sz = 0)
  : chars(sz+1, ' ') {
    make_null_terminated();
  }
  
  pstring(size_type sz, const value_type& val)
  : chars(sz+1, val) {
    make_null_terminated();
  }
  
  pstring(size_type sz, const std::function<value_type(size_type)>& body) {
    auto body2 = [&] (size_type i) {
      if (i == sz) {
        return '\0';
      } else {
        return body(i);
      }
    };
    chars.tabulate(sz + 1, body2);
    make_null_terminated();
  }
  
  pstring(const char* s)
  : chars((char*)s, (char*)s + strlen(s) + 1) { }
  
  pstring(size_type sz,
          const std::function<size_type(size_type)>& body_comp,
          const std::function<value_type(size_type)>& body) {
    assert(false);
  }
  
  pstring(size_type sz,
         const std::function<size_type(size_type,size_type)>& body_comp_rng,
         const std::function<value_type(size_type)>& body) {
    assert(false);
  }
  
  pstring(std::initializer_list<value_type> xs)
  : chars(xs.size()+1) {
    size_type i = 0;
    for (auto it = xs.begin(); it != xs.end(); it++) {
      new (&chars[i++]) value_type(*it);
    }
    make_null_terminated();
  }
  
  pstring(const pstring& other)
  : chars(other.chars) { }
  
  pstring(iterator lo, iterator hi) {
    size_type n = hi - lo;
    chars.resize(n + 1);
    copy(lo, hi, begin());
  }
  
  pstring& operator=(const pstring& other) {
    chars = other.chars;
    return *this;
  }
  
  pstring& operator=(pstring&& other) {
    chars = std::move(other.chars);
    return *this;
  }
  
  value_type& operator[](size_type i) {
    check(i);
    return chars[i];
  }
  
  const value_type& operator[](size_type i) const {
    check(i);
    return chars[i];
  }
  
  size_type size() const {
    return chars.size() - 1;
  }
  
  size_type length() const {
    return size();
  }
  
  void swap(pstring& other) {
    chars.swap(other.chars);
  }
  
  void resize(size_type n, const value_type& val) {
    chars.resize(n+1, val);
    make_null_terminated();
  }
  
  void resize(size_type n) {
    value_type val;
    resize(n, val);
  }
  
  void clear() {
    resize(0);
  }
  
  iterator begin() const {
    return chars.begin();
  }
  
  const_iterator cbegin() const {
    return chars.cbegin();
  }
  
  iterator end() const {
    return chars.end();
  }
  
  const_iterator cend() const {
    return chars.cend();
  }
  
  pstring& operator+=(const pstring& str) {
    size_type n1 = size();
    size_type n2 = str.size();
    size_type n = n1 + n2;
    parray_type chars2;
    chars2.swap(chars);
    chars.tabulate(n + 1, [&] (size_type i) {
      if (i < n1) {
        return chars2[i];
      } else if (i == n) {
        return '\0';
      } else {
        i -= n1;
        return str[i];
      }
    });
    return *this;
  }
  
  pstring operator+(const pstring& rhs) {
    pstring result(*this);;
    result += rhs;
    return result;
  }
  
  const char* c_str() {
    return cbegin();
  }
  
};
  
} // end namespace

#endif

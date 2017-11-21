
#include <assert.h>
#include <initializer_list>
#include <atomic>

#ifndef _SPTL_PERWORKER_H_
#define _SPTL_PERWORKER_H_

namespace sptl {
namespace perworker {

/*---------------------------------------------------------------------*/
/* Worker ID number */
  
static constexpr
int invalid_worker_id = -1;

__thread
int my_id = invalid_worker_id;

class get_my_id {
private:

  static
  std::atomic<int> counter;

public:

  int operator()() {
    if (my_id == invalid_worker_id) {
      my_id = counter++;
    }
    assert(my_id != invalid_worker_id);
    return my_id;
  }

};

std::atomic<int> get_my_id::counter(0);

/*---------------------------------------------------------------------*/
/* Cache-aligned fixed-capacity array */

template <class Item, int capacity>
class cache_aligned_fixed_capacity_array {
private:
  
  static constexpr
  int cache_align_szb = 128;
  
  static constexpr
  int item_szb = sizeof(Item);
  
  using aligned_item_type = typename std::aligned_storage<item_szb, cache_align_szb>::type;
  
  aligned_item_type items[capacity];
  
  Item& at(std::size_t i) {
    assert(i >= 0);
    assert(i < capacity);
    return *reinterpret_cast<Item*>(items + i);
  }
  
public:
  
  cache_aligned_fixed_capacity_array() { }
  
  cache_aligned_fixed_capacity_array(const Item& x) {
    init(x);
  }
  
  Item& operator[](std::size_t i) {
    return at(i);
  }
  
  inline std::size_t size() const {
    return capacity;
  }
  
  void init(const Item& x) {
    for (int i = 0; i < size(); i++) {
      at(i) = x;
    }
  }

  template <class Body_fct>
  void iterate(Body_fct& f, int m = -1) {
    int n = (m == -1) ? size() : nb_workers;
    for (int i = 0; i < n; i++) {
      f(at(i));
    }
  }
  
};

/*---------------------------------------------------------------------*/
/* Per-worker array */

static constexpr
int default_max_nb_workers = 128;
  
template <class Item,
          class My_id=get_my_id,
          int max_nb_workers=default_max_nb_workers>
class array {
private:

  cache_aligned_fixed_capacity_array<Item, max_nb_workers> items;

public:
  
  int get_my_id() {
    My_id my_id;
    int id = my_id();
    assert(id >= 0);
    assert(id < max_nb_workers);
    return id;
  }
  
  array() { }

  array(const Item& x)
  : items(x) { }
  
  array(std::initializer_list<Item> l) {
    assert(l.size() == 1);
    items.init(*(l.begin()));
  }
  
  Item& mine() {
    return items[get_my_id()];
  }
  
  Item& operator[](std::size_t i) {
    assert(i >= 0);
    assert(i < max_nb_workers);
    return items[i];
  }
  
  void init(const Item& x) {
    items.init(x);
  }

  template <class Body_fct>
  void iterate(const Body_fct& body, int nb_workers = -1) {
    items.iterate(body, nb_workers);
  }

};
    
} // end namespace
} // end namespace

#endif

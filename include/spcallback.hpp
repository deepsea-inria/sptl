
#include <assert.h>

#ifndef _SPTL_CALLBACK_H_
#define _SPTL_CALLBACK_H_

namespace sptl {
namespace callback {

/*---------------------------------------------------------------------*/
/* Client-supplied callbacks to implement this interface */
  
class client {
public:
  
  virtual
  void init() = 0;
  
  virtual
  void destroy() = 0;
  
  virtual
  void output() = 0;
  
};

/*---------------------------------------------------------------------*/
/* Buffer in which to store callbacks */

/* It's important that the callback storage be simple, because the
 * structure is initialized during link-load time. Use of malloc/free
 * may crash the program. As such, we store callback objects in a
 * fixed-capacity array.
 */

static constexpr
int nb_max_callbacks = 2048;
  
template <class Item, int max_sz=nb_max_callbacks>
class myset {
private:
  
  int i = 0;
  
  Item inits[max_sz];
  
public:
  
  int size() {
    return i;
  }
  
  void push(Item init) {
    assert(size() >= 0);
    assert(size() < max_size);
    inits[i] = init;
    i++;
  }
  
  Item peek(size_t i) {
    assert(i < size());
    return inits[i];
  }
  
  Item pop() {
    i--;
    Item x = inits[i];
    inits[i] = Item();
    return x;
  }

};
    
// all callbacks to be stored in this object
    
myset<client*> callbacks;

/*---------------------------------------------------------------------*/
/* Client interface to callback mechanism */
  
void init() {
  for (int i = 0; i < callbacks.size(); i++) {
    client* callback = callbacks.peek(i);
    callback->init();
  }
}

void output() {
  for (int i = 0; i < callbacks.size(); i++) {
    client* callback = callbacks.peek(i);
    callback->output();
  }
}

void destroy() {
  while (callbacks.size() > 0) {
    client* callback = callbacks.pop();
    callback->destroy();
  }
}

void register_client(client* c) {
  callbacks.push(c);
}

} // end namespace
} // end namespace

#endif

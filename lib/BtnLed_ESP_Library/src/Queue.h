#ifndef __QUEUE_H
#define __QUEUE_H

#include <inttypes.h>
#include <string.h>

template <class T, uint8_t MAX_SIZE = 32>
class Queue {
public:
  Queue() : _tail(0), _depth(0) {}

  uint8_t depth() const {
    return _depth;
  }
  void clear() {
    _tail = _depth = 0;
  }
  bool put(const T *t, bool overwrite = false);
  const T *peek();
  const T *get();

protected:
  struct __packed {
    uint8_t _tail, _depth;
    T _items[MAX_SIZE];
  };
};

template <class T, uint8_t MAX_SIZE>
bool Queue<T, MAX_SIZE>::put(const T *t, bool overwrite) {
  if ((_depth >= MAX_SIZE) && (! overwrite))
    return false;
  memcpy(&_items[_tail], t, sizeof(T));
  if (++_tail >= MAX_SIZE)
    _tail = 0;
  if (_depth < MAX_SIZE)
    ++_depth;

  return true;
}

template <class T, uint8_t MAX_SIZE>
const T *Queue<T, MAX_SIZE>::peek() {
  if (! _depth)
    return NULL;

  if (_tail >= _depth)
    return &_items[_tail - _depth];
  else
    return &_items[_tail + MAX_SIZE - _depth];
}

template <class T, uint8_t MAX_SIZE>
const T *Queue<T, MAX_SIZE>::get() {
  if (! _depth)
    return NULL;

  if (_tail >= _depth)
    return &_items[_tail - _depth--];
  else
    return &_items[_tail + MAX_SIZE - _depth--];
}

#endif

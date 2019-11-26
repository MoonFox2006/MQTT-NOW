#ifndef __LIST_H
#define __LIST_H

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

template <class T, uint8_t MAX_SIZE = 255>
class List {
public:
  static const uint8_t ERR_INDEX = 0xFF;

  List() : _count(0), _items(NULL) {}
  virtual ~List() {
    clear();
  }

  uint8_t count() const {
    return _count;
  }
  void clear();
  uint8_t add(const T &t);
  void remove(uint8_t index);
  uint8_t find(const T &t);
  T &operator[](uint8_t index) {
    return _items[index];
  }
  const T &operator[](uint8_t index) const {
    return _items[index];
  }

protected:
  virtual void cleanup(void *ptr) {}
  virtual bool match(uint8_t index, const void *t);

  struct __packed {
    uint8_t _count;
    T *_items;
  };
};

template <class T, uint8_t MAX_SIZE = 255>
class StaticList {
public:
  static const uint8_t ERR_INDEX = 0xFF;

  StaticList() : _count(0) {}
  virtual ~StaticList() {
    clear();
  }

  uint8_t count() const {
    return _count;
  }
  void clear();
  uint8_t add(const T &t);
  void remove(uint8_t index);
  uint8_t find(const T &t);
  T &operator[](uint8_t index) {
    return _items[index];
  }
  const T &operator[](uint8_t index) const {
    return _items[index];
  }

protected:
  virtual void cleanup(void *ptr) {}
  virtual bool match(uint8_t index, const void *t);

  struct __packed {
    uint8_t _count;
    T _items[MAX_SIZE];
  };
};

/***
 * List class implementation
 ***/

template <class T, uint8_t MAX_SIZE>
void List<T, MAX_SIZE>::clear() {
  if (_items) {
    for (int16_t i = _count - 1; i >= 0; --i) {
      cleanup(&_items[i]);
    }
    free(_items);
    _items = NULL;
  }
  _count = 0;
}

template <class T, uint8_t MAX_SIZE>
uint8_t List<T, MAX_SIZE>::add(const T &t) {
  if (_count >= MAX_SIZE)
    return ERR_INDEX;
  if (_items) {
    void *ptr = realloc(_items, sizeof(T) * (_count + 1));
    if (! ptr)
      return ERR_INDEX;
    _items = (T*)ptr;
    ++_count;
  } else {
    _items = (T*)malloc(sizeof(T));
    if (! _items)
      return ERR_INDEX;
    _count = 1;
  }
  memcpy(&_items[_count - 1], &t, sizeof(T));

  return _count - 1;
}

template <class T, uint8_t MAX_SIZE>
void List<T, MAX_SIZE>::remove(uint8_t index) {
  if (_items && (index < _count)) {
    cleanup(&_items[index]);
    if ((_count > 1) && (index < _count - 1))
      memmove(&_items[index], &_items[index + 1], sizeof(T) * (_count - index - 1));
    --_count;
    if (_count)
      _items = (T*)realloc(_items, sizeof(T) * _count);
    else {
      free(_items);
      _items = NULL;
    }
  }
}

template <class T, uint8_t MAX_SIZE>
uint8_t List<T, MAX_SIZE>::find(const T &t) {
  if (_items) {
    for (uint8_t i = 0; i < _count; ++i) {
      if (match(i, t))
        return i;
    }
  }

  return ERR_INDEX;
}

template <class T, uint8_t MAX_SIZE>
bool List<T, MAX_SIZE>::match(uint8_t index, const void *t) {
  if (index < _count) {
    return (memcmp(&_items[index], t, sizeof(T)) == 0);
  }

  return false;
}

/***
 * StaticList class implementation
 ***/

template <class T, uint8_t MAX_SIZE>
void StaticList<T, MAX_SIZE>::clear() {
  for (int16_t i = _count - 1; i >= 0; --i) {
    cleanup(&_items[i]);
  }
  _count = 0;
}

template <class T, uint8_t MAX_SIZE>
uint8_t StaticList<T, MAX_SIZE>::add(const T &t) {
  if (_count >= MAX_SIZE)
    return ERR_INDEX;
  memcpy(&_items[_count], &t, sizeof(T));

  return _count++;
}

template <class T, uint8_t MAX_SIZE>
void StaticList<T, MAX_SIZE>::remove(uint8_t index) {
  if (index < _count) {
    cleanup(&_items[index]);
    if ((_count > 1) && (index < _count - 1))
      memmove(&_items[index], &_items[index + 1], sizeof(T) * (_count - index - 1));
    --_count;
  }
}

template <class T, uint8_t MAX_SIZE>
uint8_t StaticList<T, MAX_SIZE>::find(const T &t) {
  for (uint8_t i = 0; i < _count; ++i) {
    if (match(i, t))
      return i;
  }

  return ERR_INDEX;
}

template <class T, uint8_t MAX_SIZE>
bool StaticList<T, MAX_SIZE>::match(uint8_t index, const void *t) {
  if (index < _count) {
    return (memcmp(&_items[index], t, sizeof(T)) == 0);
  }

  return false;
}

#endif

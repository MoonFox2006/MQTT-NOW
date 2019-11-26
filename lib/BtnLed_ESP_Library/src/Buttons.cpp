#include <Arduino.h>
#include <functional>
#include <FunctionalInterrupt.h>
#include "Buttons.h"

Button::Button(uint8_t pin, bool level, const EventQueue *events, bool paused) : _isrtime(0), _events((EventQueue*)events) {
  _item.pin = pin;
  _item.level = level;
  _item.paused = paused;
  _item.pressed = false;
  _item.dblclickable = false;
  _item.duration = 0;
  pinMode(pin, level ? INPUT : INPUT_PULLUP);
  if (! paused)
    attachInterrupt(pin, [this]() { this->_isr(this); }, CHANGE);
}

void Button::pause() {
  _item.paused = true;
  detachInterrupt(_item.pin);
}

void Button::resume() {
  _item.paused = false;
  _item.pressed = false;
  _item.dblclickable = false;
  _item.duration = 0;
  attachInterrupt(_item.pin, [this]() { this->_isr(this); }, CHANGE);
}

void ICACHE_RAM_ATTR Button::_isr(Button *_this) {
  if (! _this->_item.paused) {
    uint32_t time = millis() - _this->_isrtime;

    if (_this->_item.duration) {
      if (time + _this->_item.duration < 0xFFFF)
        _this->_item.duration += time;
      else
        _this->_item.duration = 0xFFFF;
    }
    if (digitalRead(_this->_item.pin) == _this->_item.level) { // Button pressed
      if (! _this->_item.pressed) {
        _this->_item.dblclickable = (_this->_item.duration > 0) && (_this->_item.duration <= DBLCLICK_TIME);
        _this->_item.pressed = true;
        _this->_item.duration = 1;
#ifndef ONLY_COMPLEX_STATES
        _this->onChange(BTN_PRESSED);
#endif
      }
    } else { // Button released
      if (_this->_item.pressed) { // Was pressed
        if (_this->_item.duration >= LONGCLICK_TIME) {
          _this->onChange(BTN_LONGCLICK);
        } else if (_this->_item.duration >= CLICK_TIME) {
          if (_this->_item.dblclickable)
            _this->onChange(BTN_DBLCLICK);
          else
            _this->onChange(BTN_CLICK);
        } else {
#ifndef ONLY_COMPLEX_STATES
          _this->onChange(BTN_RELEASED);
#endif
        }
        _this->_item.pressed = false;
        if ((_this->_item.duration >= CLICK_TIME) && (_this->_item.duration < LONGCLICK_TIME))
          _this->_item.duration = 1;
        else
          _this->_item.duration = 0;
      }
    }
  }
  _this->_isrtime = millis();
}

void Button::onChange(buttonstate_t state) {
  if (_events) {
    event_t e;

    e.id = EVT_BTNBASE + state;
    e.data = 0;
    _events->put(&e, true);
  }
}

uint8_t Buttons::add(uint8_t pin, bool level, bool paused) {
#ifdef ESP32
  if (pin > 39)
#else
  if (pin > 15) // Pin 16 not supports pullup
#endif
    return ERR_INDEX;

  uint8_t result;
  _button_t b;

  b.pin = pin;
  b.level = level;
  b.paused = paused;
  b.pressed = false;
  b.dblclickable = false;
  b.duration = 0;
  result = List<_button_t, MAX_BUTTONS>::add(b);
  if (result != ERR_INDEX) {
    pinMode(pin, level ? INPUT : INPUT_PULLUP);
    if (! paused)
      attachInterrupt(pin, [this]() { this->_isr(this); }, CHANGE);
  }

  return result;
}

void Buttons::pause(uint8_t index) {
  if (_items && (index < _count)) {
    _items[index].paused = true;
    detachInterrupt(_items[index].pin);
  }
}

void Buttons::pause() {
  if (_items && _count) {
    for (uint8_t i = 0; i < _count; ++i) {
      if (! _items[i].paused)
        detachInterrupt(_items[i].pin);
    }
  }
}

void Buttons::resume(uint8_t index) {
  if (_items && (index < _count)) {
    _items[index].paused = false;
    _items[index].pressed = false;
    _items[index].dblclickable = false;
    _items[index].duration = 0;
    attachInterrupt(_items[index].pin, [this]() { this->_isr(this); }, CHANGE);
  }
}

void Buttons::resume() {
  if (_items && _count) {
    for (uint8_t i = 0; i < _count; ++i) {
      if (! _items[i].paused) {
        _items[i].pressed = false;
        _items[i].dblclickable = false;
        _items[i].duration = 0;
        attachInterrupt(_items[i].pin, [this]() { this->_isr(this); }, CHANGE);
      }
    }
  }
}

void ICACHE_RAM_ATTR Buttons::_isr(Buttons *_this) {
  if (_this->_items && _this->_count) {
    uint32_t time = millis() - _this->_isrtime;

    for (uint8_t i = 0; i < _this->_count; ++i) {
      if (_this->_items[i].paused)
        continue;
      if (_this->_items[i].duration) {
        if (time + _this->_items[i].duration < 0xFFFF)
          _this->_items[i].duration += time;
        else
          _this->_items[i].duration = 0xFFFF;
      }
      if (digitalRead(_this->_items[i].pin) == _this->_items[i].level) { // Button pressed
        if (! _this->_items[i].pressed) {
          _this->_items[i].dblclickable = (_this->_items[i].duration > 0) && (_this->_items[i].duration <= DBLCLICK_TIME);
          _this->_items[i].pressed = true;
          _this->_items[i].duration = 1;
#ifndef ONLY_COMPLEX_STATES
          _this->onChange(BTN_PRESSED, i);
#endif
        }
      } else { // Button released
        if (_this->_items[i].pressed) { // Was pressed
          if (_this->_items[i].duration >= LONGCLICK_TIME) {
            _this->onChange(BTN_LONGCLICK, i);
          } else if (_this->_items[i].duration >= CLICK_TIME) {
            if (_this->_items[i].dblclickable)
              _this->onChange(BTN_DBLCLICK, i);
            else
              _this->onChange(BTN_CLICK, i);
          } else {
#ifndef ONLY_COMPLEX_STATES
            _this->onChange(BTN_RELEASED, i);
#endif
          }
          _this->_items[i].pressed = false;
          if ((_this->_items[i].duration >= CLICK_TIME) && (_this->_items[i].duration < LONGCLICK_TIME))
            _this->_items[i].duration = 1;
          else
            _this->_items[i].duration = 0;
        }
      }
    }
  }
  _this->_isrtime = millis();
}

void Buttons::cleanup(void *ptr) {
  detachInterrupt(((_button_t*)ptr)->pin);
}

bool Buttons::match(uint8_t index, const void *t) {
  if (_items && (index < _count)) {
    return (_items[index].pin == ((_button_t*)t)->pin);
  }

  return false;
}

void Buttons::onChange(buttonstate_t state, uint8_t button) {
  if (_events) {
    event_t e;

    e.id = EVT_BTNBASE + state;
    e.data = button;
    _events->put(&e, true);
  }
}

#include <Arduino.h>
#ifdef ESP32
#include <esp32-hal-ledc.h>
#endif
#include "Leds.h"

Led::Led(uint8_t pin, bool level) {
#ifdef ESP32
  ledcSetup(0, 4096, 10);
#endif
  _item.pin = pin;
  _item.level = level;
  _item.mode = LED_OFF;
  pinMode(pin, OUTPUT);
  off();
}

#ifdef ESP32
void Led::setMode(ledmode_t mode) {
  if (_item.mode != mode) {
    if ((_item.mode >= LED_FADEIN) && (mode < LED_FADEIN)) {
      ledcDetachPin(_item.pin);
    } else if ((_item.mode < LED_FADEIN) && (mode >= LED_FADEIN)) {
      ledcAttachPin(_item.pin, 0);
    }
    _item.mode = mode;
  }
  update(true);
}
#endif

void Led::update(bool force) {
  if (force || (_item.mode > LED_ON)) {
    if (_item.mode == LED_OFF) {
      off();
    } else if (_item.mode == LED_ON) {
      on();
    } else {
      uint16_t subsec;

      if (_item.mode == LED_FADEINOUT)
        subsec = millis() % 2000;
      else
        subsec = millis() % 1000;
      if (_item.mode == LED_1HZ) {
        if (subsec < BLINK_TIME)
          on();
        else
          off();
      } else if (_item.mode == LED_2HZ) {
        if (subsec % 500 < BLINK_TIME)
          on();
        else
          off();
      } else if (_item.mode == LED_4HZ) {
        if (subsec % 250 < BLINK_TIME)
          on();
        else
          off();
      } else if (_item.mode == LED_FADEIN) {
        if (_item.level) {
#ifdef ESP32
          ledcWrite(0, (1 << ((subsec + 50) / 100)) - 1);
#else
          analogWrite(_item.pin, (1 << ((subsec + 50) / 100)) - 1);
#endif
        } else {
#ifdef ESP32
          ledcWrite(0, 1023 - ((1 << ((subsec + 50) / 100)) - 1));
#else
          analogWrite(_item.pin, 1023 - ((1 << ((subsec + 50) / 100)) - 1));
#endif
        }
      } else if (_item.mode == LED_FADEOUT) {
        if (_item.level) {
#ifdef ESP32
          ledcWrite(0, (1 << (10 - (subsec + 50) / 100)) - 1);
#else
          analogWrite(_item.pin, (1 << (10 - (subsec + 50) / 100)) - 1);
#endif
        } else {
#ifdef ESP32
          ledcWrite(0, 1023 - ((1 << (10 - (subsec + 50) / 100)) - 1));
#else
          analogWrite(_item.pin, 1023 - ((1 << (10 - (subsec + 50) / 100)) - 1));
#endif
        }
      } else if (_item.mode == LED_FADEINOUT) {
        if (subsec < 1000) {
          if (_item.level) {
#ifdef ESP32
            ledcWrite(0, (1 << ((subsec + 50) / 100)) - 1);
#else
            analogWrite(_item.pin, (1 << ((subsec + 50) / 100)) - 1);
#endif
          } else {
#ifdef ESP32
            ledcWrite(0, 1023 - ((1 << ((subsec + 50) / 100)) - 1));
#else
            analogWrite(_item.pin, 1023 - ((1 << ((subsec + 50) / 100)) - 1));
#endif
          }
        } else {
          if (_item.level) {
#ifdef ESP32
            ledcWrite(0, (1 << (10 - (subsec - 1000 + 50) / 100)) - 1);
#else
            analogWrite(_item.pin, (1 << (10 - (subsec - 1000 + 50) / 100)) - 1);
#endif
          } else {
#ifdef ESP32
            ledcWrite(0, 1023 - ((1 << (10 - (subsec - 1000 + 50) / 100)) - 1));
#else
            analogWrite(_item.pin, 1023 - ((1 << (10 - (subsec - 1000 + 50) / 100)) - 1));
#endif
          }
        }
      }
    }
  }
}

void Led::delay(uint32_t ms, uint32_t step) {
  if (_item.mode <= LED_ON) {
    ::delay(ms);
  } else {
    while (ms >= step) {
      update();
      ::delay(step);
      ms -= step;
    }
    if (ms) {
      update();
      ::delay(ms);
    }
  }
}

inline void Led::off() {
  digitalWrite(_item.pin, ! _item.level);
}

inline void Led::on() {
  digitalWrite(_item.pin, _item.level);
}

uint8_t Leds::add(uint8_t pin, bool level, ledmode_t mode) {
#ifdef ESP32
  if (pin > 33) // Pins 34-39 for input only!
#else
  if (pin > 15) // Pin 16 not supports PWM
#endif
    return ERR_INDEX;

  uint8_t result;
  _led_t l;

  l.pin = pin;
  l.level = level;
  l.mode = mode;
  result = List<_led_t, MAX_LEDS>::add(l);
  if (result != ERR_INDEX) {
#ifdef ESP32
    if (result < 16)
      ledcSetup(result, 4096, 10);
#endif
    pinMode(pin, OUTPUT);
    setMode(result, mode);
  }

  return result;
}

ledmode_t Leds::getMode(uint8_t index) const {
  if (_items && (index < _count)) {
    return _items[index].mode;
  }
}

void Leds::setMode(uint8_t index, ledmode_t mode) {
  if (_items && (index < _count)) {
    if (_items[index].mode != mode) {
#ifdef ESP32
      if (index < 16) {
        if ((_items[index].mode >= LED_FADEIN) && (mode < LED_FADEIN)) {
          ledcDetachPin(_items[index].pin);
        } else if ((_items[index].mode < LED_FADEIN) && (mode >= LED_FADEIN)) {
          ledcAttachPin(_items[index].pin, index);
        }
      }
#endif
      _items[index].mode = mode;
    }
    update(index, true);
  }
}

void Leds::update(uint8_t index, bool force) {
  if (_items) {
    uint8_t i;

    if (index < _count)
      i = index;
    else if (index == ERR_INDEX)
      i = 0;
    else
      return;
    while (i < _count) {
      if (force || (_items[i].mode > LED_ON)) {
        if (_items[i].mode == LED_OFF) {
          off(i);
        } else if (_items[i].mode == LED_ON) {
          on(i);
        } else {
          uint16_t subsec;

          if (_items[i].mode == LED_FADEINOUT)
            subsec = millis() % 2000;
          else
            subsec = millis() % 1000;
          if (_items[i].mode == LED_1HZ) {
            if (subsec < BLINK_TIME)
              on(i);
            else
              off(i);
          } else if (_items[i].mode == LED_2HZ) {
            if (subsec % 500 < BLINK_TIME)
              on(i);
            else
              off(i);
          } else if (_items[i].mode == LED_4HZ) {
            if (subsec % 250 < BLINK_TIME)
              on(i);
            else
              off(i);
          } else if (_items[i].mode == LED_FADEIN) {
            if (_items[i].level) {
#ifdef ESP32
              if (i < 16)
                ledcWrite(i, (1 << ((subsec + 50) / 100)) - 1);
#else
              analogWrite(_items[i].pin, (1 << ((subsec + 50) / 100)) - 1);
#endif
            } else {
#ifdef ESP32
              if (i < 16)
                ledcWrite(i, 1023 - ((1 << ((subsec + 50) / 100)) - 1));
#else
              analogWrite(_items[i].pin, 1023 - ((1 << ((subsec + 50) / 100)) - 1));
#endif
            }
          } else if (_items[i].mode == LED_FADEOUT) {
            if (_items[i].level) {
#ifdef ESP32
              if (i < 16)
                ledcWrite(i, (1 << (10 - (subsec + 50) / 100)) - 1);
#else
              analogWrite(_items[i].pin, (1 << (10 - (subsec + 50) / 100)) - 1);
#endif
            } else {
#ifdef ESP32
              if (i < 16)
                ledcWrite(i, 1023 - ((1 << (10 - (subsec + 50) / 100)) - 1));
#else
              analogWrite(_items[i].pin, 1023 - ((1 << (10 - (subsec + 50) / 100)) - 1));
#endif
            }
          } else if (_items[i].mode == LED_FADEINOUT) {
            if (subsec < 1000) {
              if (_items[i].level) {
#ifdef ESP32
                if (i < 16)
                  ledcWrite(i, (1 << ((subsec + 50) / 100)) - 1);
#else
                analogWrite(_items[i].pin, (1 << ((subsec + 50) / 100)) - 1);
#endif
              } else {
#ifdef ESP32
                if (i < 16)
                  ledcWrite(i, 1023 - ((1 << ((subsec + 50) / 100)) - 1));
#else
                analogWrite(_items[i].pin, 1023 - ((1 << ((subsec + 50) / 100)) - 1));
#endif
              }
            } else {
              if (_items[i].level) {
#ifdef ESP32
                if (i < 16)
                  ledcWrite(i, (1 << (10 - (subsec - 1000 + 50) / 100)) - 1);
#else
                analogWrite(_items[i].pin, (1 << (10 - (subsec - 1000 + 50) / 100)) - 1);
#endif
              } else {
#ifdef ESP32
                if (i < 16)
                  ledcWrite(i, 1023 - ((1 << (10 - (subsec - 1000 + 50) / 100)) - 1));
#else
                analogWrite(_items[i].pin, 1023 - ((1 << (10 - (subsec - 1000 + 50) / 100)) - 1));
#endif
              }
            }
          }
        }
      }
      if (index == ERR_INDEX)
        ++i;
      else
        break;
    }
  }
}

void Leds::delay(uint32_t ms, uint32_t step) {
  bool updating = false;

  if (_items) {
    for (uint8_t i = 0; i < _count; ++i) {
      if (_items[i].mode > LED_ON) {
        updating = true;
        break;
      }
    }
  }
  if (updating) {
    while (ms >= step) {
      update();
      ::delay(step);
      ms -= step;
    }
    if (ms) {
      update();
      ::delay(ms);
    }
  } else {
    ::delay(ms);
  }
}

bool Leds::match(uint8_t index, const void *t) {
  if (_items && (index < _count)) {
    return (_items[index].pin == ((_led_t*)t)->pin);
  }

  return false;
}

void Leds::on(uint8_t index) {
  if (_items && (index < _count)) {
    digitalWrite(_items[index].pin, _items[index].level);
  }
}

void Leds::off(uint8_t index) {
  if (_items && (index < _count)) {
    digitalWrite(_items[index].pin, ! _items[index].level);
  }
}

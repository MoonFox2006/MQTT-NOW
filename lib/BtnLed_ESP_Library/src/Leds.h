#ifndef __LEDS_H
#define __LEDS_H

#include "List.h"

enum ledmode_t { LED_OFF, LED_ON, LED_1HZ, LED_2HZ, LED_4HZ, LED_FADEIN, LED_FADEOUT, LED_FADEINOUT };

struct __packed _led_t {
#ifdef ESP32
  uint8_t pin;
#else
  uint8_t pin : 4;
#endif
  bool level : 1;
  ledmode_t mode : 3;
};

class Led {
public:
  Led(uint8_t pin, bool level = false);

  ledmode_t getMode() const {
    return _item.mode;
  }
#ifdef ESP32
  void setMode(ledmode_t mode);
#else
  void setMode(ledmode_t mode) {
    _item.mode = mode;
    update(true);
  }
#endif
  void update(bool force = false);
  void delay(uint32_t ms, uint32_t step = 1);

protected:
  static const uint32_t BLINK_TIME = 25; // 25 ms.

  inline void off();
  inline void on();

  _led_t _item;
};

#ifdef ESP32
#define MAX_LEDS 16
#else
#define MAX_LEDS 10
#endif

class Leds : public List<_led_t, MAX_LEDS> {
public:
  Leds() : List<_led_t, MAX_LEDS>() {};

  uint8_t add(uint8_t pin, bool level, ledmode_t mode = LED_OFF);
  ledmode_t getMode(uint8_t index) const;
  void setMode(uint8_t index, ledmode_t mode);
  void update(uint8_t index = ERR_INDEX, bool force = false);
  void delay(uint32_t ms, uint32_t step = 1);

protected:
  static const uint32_t BLINK_TIME = 25; // 25 ms.

  bool match(uint8_t index, const void *t);

  void on(uint8_t index);
  void off(uint8_t index);
};

#endif

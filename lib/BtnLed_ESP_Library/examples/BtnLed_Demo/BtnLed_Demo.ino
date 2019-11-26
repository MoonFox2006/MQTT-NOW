#include <Arduino.h>
#include "Leds.h"
#include "Events.h"
#include "Buttons.h"

#define ONE_LED

#ifdef ONE_LED
const uint8_t LED_PIN = 2;
const uint8_t BTN_PIN = 0;
#else
const uint8_t LED_PINS[] PROGMEM = { 2, 14, 12, 13 };
const uint8_t BTN_PINS[] PROGMEM = { 3, 5, 4, 0 };
#endif
const bool LED_LEVEL = LOW;
const bool BTN_LEVEL = LOW;

EventQueue *events;
#ifdef ONE_LED
Led *led;
Button *btn;
#else
Leds *leds;
Buttons *btns;
#endif

void setup() {
#ifdef ESP32
  Serial.begin(115200, SERIAL_8N1, -1, 1);
#else
  Serial.begin(115200, SERIAL_8N1, SERIAL_TX_ONLY);
  Serial.println();
#endif

  events = new EventQueue();
#ifdef ONE_LED
  led = new Led(LED_PIN, LED_LEVEL);
  btn = new Button(BTN_PIN, BTN_LEVEL, events);
#else
  leds = new Leds();
  for (uint8_t i = 0; i < sizeof(LED_PINS) / sizeof(LED_PINS[0]); ++i) {
    leds->add(pgm_read_byte(&LED_PINS[i]), LED_LEVEL);
  }
  btns = new Buttons(events);
  for (uint8_t i = 0; i < sizeof(BTN_PINS) / sizeof(BTN_PINS[0]); ++i) {
    btns->add(pgm_read_byte(&BTN_PINS[i]), BTN_LEVEL);
  }
#endif
}

void loop() {
  event_t *event;

  while ((event = (event_t*)events->get()) != NULL) {
    if (event->id >= EVT_BTNCLICK) {
      Serial.print(F("Button #"));
      Serial.print(event->data + 1);
    }
    if (event->id == EVT_BTNCLICK) { // Next led mode
      ledmode_t ledmode;

#ifdef ONE_LED
      ledmode = led->getMode();
#else
      ledmode = leds->getMode(event->data);
#endif
      if (ledmode < LED_FADEINOUT)
        *((uint8_t*)&ledmode) += 1;
      else
        ledmode = LED_OFF;
#ifdef ONE_LED
      led->setMode(ledmode);
#else
      leds->setMode(event->data, ledmode);
#endif
      Serial.println(F(" clicked"));
    } else if (event->id == EVT_BTNLONGCLICK) { // Previous led mode
      ledmode_t ledmode;

#ifdef ONE_LED
      ledmode = led->getMode();
#else
      ledmode = leds->getMode(event->data);
#endif
      if (ledmode > LED_OFF)
        *((uint8_t*)&ledmode) -= 1;
      else
        ledmode = LED_FADEINOUT;
#ifdef ONE_LED
      led->setMode(ledmode);
#else
      leds->setMode(event->data, ledmode);
#endif
      Serial.println(F(" long clicked"));
    } else if (event->id == EVT_BTNDBLCLICK) { // First led mode (off)
#ifdef ONE_LED
      led->setMode(LED_OFF);
#else
      leds->setMode(event->data, LED_OFF);
#endif
      Serial.println(F(" double clicked"));
    }
  }
#ifdef ONE_LED
  led->delay(1);
#else
  leds->delay(1);
#endif
}

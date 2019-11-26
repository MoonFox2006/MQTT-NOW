#ifndef __EVENTS_H
#define __EVENTS_H

#include "Queue.h"

typedef uint8_t payload_t;

struct __packed event_t {
  uint8_t id;
  payload_t data;
};

typedef Queue<event_t, 32> EventQueue;

#endif

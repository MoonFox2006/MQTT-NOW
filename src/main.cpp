#define SERVER
#define ASYNC_MQTT

#include <Arduino.h>
#include <ESP8266WiFi.h>
#ifdef SERVER
#ifdef ASYNC_MQTT
#include <AsyncMqttClient.h>
#else
#include <PubSubClient.h>
#endif
#endif
#include "EspNowHelper.h"
#include "Leds.h"

const uint8_t LED_PIN = 2;
const bool LED_LEVEL = LOW;

#ifdef SERVER
static const char WIFI_SSID[] PROGMEM = "******";
static const char WIFI_PSWD[] PROGMEM = "******";

static const char MQTT_SERVER[] = "******";
static const uint16_t MQTT_PORT = 1883;
static const char MQTT_CLIENT[] = "MQTT-NOW";
static const char MQTT_PREFIX[] PROGMEM = "/MQTT-NOW";
static const uint8_t MQTT_QOS = 0;
static const bool MQTT_RETAIN = false;

static const char MQTT_UPTIME_TOPIC[] PROGMEM = "/uptime";
#endif

static const uint8_t ESPNOW_MAGIC = 0xA5;

enum espnow_type_t : uint8_t { ESPNOW_ACK, ESPNOW_DATA };

struct __packed espnow_header_t {
  uint8_t magic; // Must be == ESPNOW_MAGIC (0xA5)
  espnow_type_t type;
  uint16_t num;
};

struct __packed payload_t {
  uint32_t id;
  uint32_t uptime;
};

struct __packed espnow_data_t {
  espnow_header_t header;
  payload_t payload;
};

#ifdef SERVER
class EspNowServerPlus : public EspNowServer {
public:
  EspNowServerPlus() : EspNowServer(), _peer_count(0) {}

  void end();

protected:
  struct __packed peer_t {
    uint8_t mac[6];
    uint16_t num;
    payload_t payload;
    bool acknowledged;
  };

  static const uint8_t MAX_PEERS = 10;

  void onReceive(const uint8_t *mac, const uint8_t *data, uint8_t len);

  bool isDataPacket(const uint8_t *data, uint8_t len);
  bool sendAck(const uint8_t *mac);

  peer_t *peerByMac(const uint8_t *mac);

  peer_t _peers[MAX_PEERS];
  uint8_t _peer_count;

  friend void loop();
};

#else
class EspNowClientPlus : public EspNowClient {
public:
  EspNowClientPlus(uint8_t channel, const uint8_t *mac) : EspNowClient(channel, mac), _num(0) {}

protected:
  void onReceive(const uint8_t *mac, const uint8_t *data, uint8_t len);

  bool isAckPacket(const uint8_t *data, uint8_t len);
  bool sendData();

  uint16_t _num;

  friend void loop();
};
#endif

Led *led = NULL;
EspNowGeneric *esp_now = NULL;
#ifdef SERVER
WiFiEventHandler wifiConnectHandler;
WiFiEventHandler wifiDisconnectHandler;
#ifdef ASYNC_MQTT
AsyncMqttClient *mqtt = NULL;
#else
PubSubClient *mqtt;
#endif

volatile uint32_t wifiLastConnecting = 0;
volatile uint32_t mqttLastConnecting = 0;
#endif

static String macToString(const uint8_t mac[]);

static void dumpPacket(const uint8_t *data, uint8_t len) {
  bool error = true;

  if (len == sizeof(espnow_header_t)) {
    if ((((espnow_header_t*)data)->magic == ESPNOW_MAGIC) && (((espnow_header_t*)data)->type == ESPNOW_ACK)) {
      Serial.print(F("ESP-NOW ACK packet (#"));
      Serial.print(((espnow_header_t*)data)->num);
      Serial.println(')');
      error = false;
    }
  } else if (len == sizeof(espnow_data_t)) {
    if ((((espnow_data_t*)data)->header.magic == ESPNOW_MAGIC) && (((espnow_data_t*)data)->header.type == ESPNOW_DATA)) {
      Serial.print(F("ESP-NOW DATA packet (#"));
      Serial.print(((espnow_data_t*)data)->header.num);
      Serial.println(')');
      error = false;
    }
  }
  if (error) {
    Serial.println(F("Wrong ESP-NOW packet!"));
  }
}

#ifdef SERVER
void EspNowServerPlus::end() {
  EspNowServer::end();
  _peer_count = 0;
}

void EspNowServerPlus::onReceive(const uint8_t *mac, const uint8_t *data, uint8_t len) {
  Serial.print(F("\nESP-NOW packet received from "));
  Serial.println(macToString(mac));
  dumpPacket(data, len);
  if (isDataPacket(data, len)) {
    if (! findPeer(mac)) {
      Serial.print(F("Add new peer "));
      if (addPeer(mac, ESP_NOW_ROLE_CONTROLLER))
        Serial.println(F("successful"));
      else
        Serial.println(F("fail!"));
    }

    peer_t *peer = peerByMac(mac);

    if ((! peer) || (peer->num != ((espnow_data_t*)data)->header.num)) {
      if (! peer) {
        if (_peer_count < MAX_PEERS) {
          peer = &_peers[_peer_count++];
        } else {
          Serial.println(F("Too many peers!"));
          return;
        }
      }
      memcpy(peer->mac, mac, sizeof(peer->mac));
      peer->num = ((espnow_data_t*)data)->header.num;
      memcpy(&peer->payload, &((espnow_data_t*)data)->payload, sizeof(peer->payload));
      peer->acknowledged = false;
      Serial.println(F("Packet from peer cached"));
      _received = true;
    }
  }
}

bool EspNowServerPlus::isDataPacket(const uint8_t *data, uint8_t len) {
  return (len == sizeof(espnow_data_t)) && (((espnow_data_t*)data)->header.magic == ESPNOW_MAGIC) &&
    (((espnow_data_t*)data)->header.type == ESPNOW_DATA);
}

bool EspNowServerPlus::sendAck(const uint8_t *mac) {
  const uint8_t REPEAT = 2;
  const uint32_t GAP = 1; // 1 ms.

  peer_t *peer = peerByMac(mac);

  if (peer) {
    espnow_header_t header;

    header.magic = ESPNOW_MAGIC;
    header.type = ESPNOW_ACK;
    header.num = peer->num;
    if (esp_now->sendReliable(peer->mac, (uint8_t*)&header, sizeof(header), REPEAT, GAP)) {
      peer->acknowledged = true;
      return true;
    }
  }

  return false;
}

EspNowServerPlus::peer_t *EspNowServerPlus::peerByMac(const uint8_t *mac) {
  for (uint8_t i = 0; i < _peer_count; ++i) {
    if (! memcmp(_peers[i].mac, mac, sizeof(_peers[i].mac)))
      return &_peers[i];
  }

  return NULL;
}

#else
void EspNowClientPlus::onReceive(const uint8_t *mac, const uint8_t *data, uint8_t len) {
  Serial.print(F("\nESP-NOW packet received from "));
  Serial.println(macToString(mac));
  dumpPacket(data, len);
  if (isAckPacket(data, len)) {
    if (((espnow_header_t*)data)->num == _num) {
      _received = true;
    } else {
      Serial.println(F("Wrong num in header!"));
    }
  }
}

bool EspNowClientPlus::isAckPacket(const uint8_t *data, uint8_t len) {
  return (len == sizeof(espnow_header_t)) && (((espnow_header_t*)data)->magic == ESPNOW_MAGIC) &&
    (((espnow_header_t*)data)->type == ESPNOW_ACK);
}

bool EspNowClientPlus::sendData() {
  const uint8_t REPEAT = 5;
  const uint32_t GAP = 1; // 1 ms.

  uint8_t repeat = REPEAT;
  espnow_data_t data;
  uint32_t start;

  data.header.magic = ESPNOW_MAGIC;
  data.header.type = ESPNOW_DATA;
  data.header.num = ++_num;
  data.payload.id = ESP.getChipId();
  data.payload.uptime = millis();
  _received = false;
  while ((! _received) && repeat--) {
    if (esp_now->sendReliable(_server_mac, (uint8_t*)&data, sizeof(data), 1, GAP)) {
      start = millis();
      while ((! _received) && (millis() - start < GAP + 1)) {
        delay(1);
      }
    }
  }

  return _received;
}
#endif

static void halt(const __FlashStringHelper *msg) {
  Serial.println(msg);
  Serial.println(F("System halted!"));
  Serial.flush();
  if (led)
    led->setMode(LED_OFF);

  ESP.deepSleep(0);
}

static void reboot(const __FlashStringHelper *msg) {
  Serial.println(msg);
  Serial.println(F("System restarted!"));
  Serial.flush();
  if (led)
    led->setMode(LED_OFF);

  ESP.restart();
}

#ifdef SERVER
static void wifiConnect() {
  const uint32_t WIFI_CONNECT_TIMEOUT = 60000; // 60 sec.

  if ((! wifiLastConnecting) || (millis() - wifiLastConnecting >= WIFI_CONNECT_TIMEOUT)) {
    char wifi_ssid[sizeof(WIFI_SSID)];
    char wifi_pswd[sizeof(WIFI_PSWD)];

    strcpy_P(wifi_ssid, WIFI_SSID);
    strcpy_P(wifi_pswd, WIFI_PSWD);
    Serial.print(F("Connecting to SSID \""));
    Serial.print(wifi_ssid);
    Serial.println(F("\"..."));
    WiFi.disconnect();
    WiFi.begin(wifi_ssid, wifi_pswd);
    led->setMode(LED_FADEIN);
    wifiLastConnecting = millis();
  }
}

static void mqttConnect();

static void onWifiConnected(const WiFiEventStationModeGotIP &event) {
  Serial.print(F("\nConnected to WiFi (IP: "));
  Serial.print(event.ip);
  Serial.println(')');
  led->setMode(LED_FADEOUT);
#ifdef ASYNC_MQTT
  if (mqtt)
    mqttConnect();
#endif
  Serial.print(F("Starting ESP-NOW server "));
  esp_now = new EspNowServerPlus();
  if (esp_now->begin()) {
    Serial.println(F("successful"));
  } else {
    reboot(F("FAIL!"));
  }
  wifiLastConnecting = 0;
}

static void onWifiDisconnected(const WiFiEventStationModeDisconnected &event) {
  Serial.println(F("\nDisconnected from WiFi"));
  led->setMode(LED_OFF);
  if (esp_now) {
    delete esp_now;
    esp_now = NULL;
    Serial.println(F("ESP-NOW server stopped"));
  }
}

static void mqttConnect() {
  const uint32_t MQTT_CONNECT_TIMEOUT = 60000; // 60 sec.

  if ((! mqttLastConnecting) || (millis() - mqttLastConnecting >= MQTT_CONNECT_TIMEOUT)) {
#ifdef ASYNC_MQTT
    Serial.println(F("Connecting to MQTT broker..."));
    mqtt->disconnect();
    mqtt->connect();
    led->setMode(LED_FADEOUT);
    mqttLastConnecting = millis();
#else
    Serial.print(F("Connecting to MQTT broker..."));
    if (mqtt->connect(MQTT_CLIENT)) {
      Serial.println(F(" successful"));
      led->setMode(LED_FADEINOUT);
      mqttLastConnecting = 0;
    } else {
      Serial.print(F(" FAIL ("));
      Serial.print(mqtt->state());
      Serial.println(F(")!"));
      led->setMode(LED_FADEOUT);
      mqttLastConnecting = millis();
    }
#endif
  }
}

#ifdef ASYNC_MQTT
static void onMqttConnected(bool sessionPresent) {
  Serial.println(F("\nConnected to MQTT broker"));
  led->setMode(LED_FADEINOUT);
  mqttLastConnecting = 0;
}
#endif

static bool mqttPublish(const char *topic, const char *value, uint32_t id) {
  bool result = false;

  if (mqtt->connected()) {
    char *_topic;

    _topic = new char[sizeof(MQTT_PREFIX) + strlen(topic) + 1 + 8];
    if (_topic) {
      strcpy_P(_topic, MQTT_PREFIX);
      strcat(_topic, topic);
      sprintf_P(&_topic[strlen(_topic)], PSTR("/%08X"), id);
      Serial.print(F("Publishing MQTT topic \""));
      Serial.print(_topic);
      Serial.print(F("\" with value \""));
      Serial.print(value);
      Serial.println('"');
#ifdef ASYNC_MQTT
      result = mqtt->publish(_topic, MQTT_QOS, MQTT_RETAIN, value);
#else
      result = mqtt->publish(_topic, value, MQTT_RETAIN);
#endif
      delete[] _topic;
    } else {
      Serial.println(F("Not enough memory!"));
    }
  }

  return result;
}
#endif

static String macToString(const uint8_t mac[]) {
  char str[18];

  sprintf_P(str, PSTR("%02X:%02X:%02X:%02X:%02X:%02X"), mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  return String(str);
}

void setup() {
  Serial.begin(115200, SERIAL_8N1, SERIAL_TX_ONLY);
  Serial.println();

  Serial.print(F("SDK version: "));
  Serial.println(ESP.getSdkVersion());

  led = new Led(LED_PIN, LED_LEVEL);

  WiFi.persistent(false);
#ifdef SERVER
  WiFi.mode(WIFI_AP_STA);
  WiFi.disconnect();
  WiFi.softAPdisconnect();
  wifiConnectHandler = WiFi.onStationModeGotIP(onWifiConnected);
  wifiDisconnectHandler = WiFi.onStationModeDisconnected(onWifiDisconnected);

#ifdef ASYNC_MQTT
  mqtt = new AsyncMqttClient();
  mqtt->setServer(MQTT_SERVER, MQTT_PORT);
  mqtt->setClientId(MQTT_CLIENT);
  mqtt->onConnect(onMqttConnected);
#else
  mqtt = new PubSubClient(*new WiFiClient());
  mqtt->setServer(MQTT_SERVER, MQTT_PORT);
#endif
#else
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  {
    int8_t channel;
    uint8_t mac[6];
    int8_t rssi;
    uint32_t start;

    Serial.print(F("Waiting for ESP-NOW server"));
    channel = espNowFindServer(mac, &rssi);
    if (! channel) {
      const uint32_t WAIT_SERVER_TIMEOUT = 15000; // 15 sec.

      led->setMode(LED_4HZ);
      start = millis();
      while ((! channel) && (millis() - start <= WAIT_SERVER_TIMEOUT)) {
        led->delay(1000);
        Serial.print('.');
        channel = espNowFindServer(mac, &rssi);
      }
    }
    if (channel) {
      Serial.print(F(" OK (on channel "));
      Serial.print(channel);
      Serial.print(F(", mac: "));
      Serial.print(macToString(mac));
      Serial.print(F(", rssi: "));
      Serial.print(rssi);
      Serial.println(F(" dB)"));
      esp_now = new EspNowClientPlus(channel, mac);
      Serial.print(F("ESP-NOW client "));
      if (esp_now->begin()) {
        Serial.println(F("started"));
        led->setMode(LED_1HZ);
      } else {
        reboot(F("FAIL!"));
      }
    } else {
      reboot(F(" FAIL!"));
    }
  }
#endif
}

void loop() {
#ifdef SERVER
  if (! WiFi.isConnected())
    wifiConnect();
  if (WiFi.isConnected()) {
    if (mqtt && (! mqtt->connected()))
      mqttConnect();
#ifndef ASYNC_MQTT
    if (mqtt && mqtt->connected())
      mqtt->loop();
#endif
    if (esp_now && ((EspNowServerPlus*)esp_now)->_received) {
      for (uint8_t i = 0; i < ((EspNowServerPlus*)esp_now)->_peer_count; ++i) {
        EspNowServerPlus::peer_t *peer = &((EspNowServerPlus*)esp_now)->_peers[i];

        if (! peer->acknowledged) {
          Serial.print(F("Sending ACK "));
          if (((EspNowServerPlus*)esp_now)->sendAck(peer->mac)) {
            Serial.println("OK");
          } else {
            Serial.println("FAIL!");
          }
          if (mqtt && mqtt->connected()) {
            char mqtt_topic[sizeof(MQTT_UPTIME_TOPIC)];
            char value[11];

            strcpy_P(mqtt_topic, MQTT_UPTIME_TOPIC);
            mqttPublish(mqtt_topic, ultoa(peer->payload.uptime, value, 10), peer->payload.id);
          }
        }
      }
      ((EspNowServerPlus*)esp_now)->_received = false;
    }
  }
#else
  const uint32_t SEND_PERIOD = 5000; // 5 sec.
  const uint8_t MAX_ERRORS = 5;

  static uint32_t lastSend = 0;
  static uint8_t errors = 0;

  if ((! lastSend) || (millis() - lastSend >= SEND_PERIOD)) {
    Serial.print(F("Sending DATA packet "));
    if (((EspNowClientPlus*)esp_now)->sendData()) {
      Serial.println(F("OK"));
      errors = 0;
    } else {
      Serial.println(F("FAIL!"));
      if (++errors >= MAX_ERRORS)
        reboot(F("Too many errors (connection lost)!"));
    }
    lastSend = millis();
  }
#endif
  led->delay(1);
}

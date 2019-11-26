#include <Arduino.h>
#include <pgmspace.h>
#include <ESP8266WiFi.h>
#include "EspNowHelper.h"

static const uint8_t ESPNOW_OK = 0;

static const char ESPNOW_SERVER_AP[] PROGMEM = "ESP-NOW$";

bool EspNowGeneric::begin() {
  if (esp_now_init() != ESPNOW_OK)
    return false;
  if (esp_now_set_self_role(_role) != ESPNOW_OK) {
    esp_now_deinit();
    return false;
  }
  esp_now_register_recv_cb(&EspNowGeneric::_onReceive);
  esp_now_register_send_cb(&EspNowGeneric::_onSend);
  if (_channel) {
    if (! wifi_set_channel(_channel)) {
      esp_now_unregister_send_cb();
      esp_now_unregister_recv_cb();
      esp_now_deinit();
      return false;
    }
  }

  return true;
}

void EspNowGeneric::end() {
  clearPeers();
  esp_now_unregister_send_cb();
  esp_now_unregister_recv_cb();
  esp_now_deinit();
}

bool EspNowGeneric::setMasterKey(const uint8_t *key, uint8_t keylen) {
  return (esp_now_set_kok((uint8_t*)key, keylen) == ESPNOW_OK);
}

uint8_t EspNowGeneric::peerCount() const {
  uint8_t result, dummy;

  if (esp_now_get_cnt_info(&result, &dummy) != ESPNOW_OK)
    result = 0;

  return result;
}

void EspNowGeneric::clearPeers() {
  uint8_t *mac;

  mac = esp_now_fetch_peer(true);
  while (mac) {
    esp_now_del_peer(mac);
    mac = esp_now_fetch_peer(false);
  }
}

bool EspNowGeneric::findPeer(const uint8_t *mac) {
  return (esp_now_is_peer_exist((uint8_t*)mac) > 0);
}

bool EspNowGeneric::addPeer(const uint8_t *mac, esp_now_role role) {
  return (esp_now_add_peer((uint8_t*)mac, role, _channel, NULL, 0) == ESPNOW_OK);
}

bool EspNowGeneric::addPeerSecure(const uint8_t *mac, const uint8_t *key, uint8_t keylen, esp_now_role role) {
  return (esp_now_add_peer((uint8_t*)mac, role, _channel, (uint8_t*)key, keylen) == ESPNOW_OK);
}

bool EspNowGeneric::removePeer(const uint8_t *mac) {
  return (esp_now_del_peer((uint8_t*)mac) == ESPNOW_OK);
}

bool EspNowGeneric::send(const uint8_t *mac, const uint8_t *data, uint8_t len) {
  _sended = false;
  _sendError = esp_now_send((uint8_t*)mac, (uint8_t*)data, len) != ESPNOW_OK;

  return (! _sendError);
}

bool EspNowGeneric::sendBroadcast(const uint8_t *data, uint8_t len) {
  uint8_t mac[6];

  for (uint8_t i = 0; i < sizeof(mac); ++i) {
    mac[i] = 0xFF;
  }

  return send(mac, data, len);
}

bool EspNowGeneric::sendReliable(const uint8_t *mac, const uint8_t *data, uint8_t len, uint8_t repeat, uint32_t timeout) {
  uint32_t start;

  do {
    _sended = false;
    _sendError = esp_now_send((uint8_t*)mac, (uint8_t*)data, len) != ESPNOW_OK;
    start = millis();
    while ((! _sended) && (millis() - start < timeout)) {
      delay(1);
    }
  } while (((! _sended) || _sendError) && repeat--);

  return ((! _sendError) && _sended);
}

bool EspNowServer::begin() {
  char ssid[sizeof(ESPNOW_SERVER_AP)];

  WiFi.persistent(false);
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAPdisconnect();
  if (WiFi.isConnected()) {
    _channel = WiFi.channel();
  } else {
    if (! _channel)
      _channel = 1;
  }
  strcpy_P(ssid, ESPNOW_SERVER_AP);
  if (WiFi.softAP(ssid, NULL, _channel, true, 0)) {
    if (EspNowGeneric::begin())
      return true;
    WiFi.softAPdisconnect();
  }

  return false;
}

void EspNowServer::end() {
  WiFi.softAPdisconnect();
  EspNowGeneric::end();
}

bool EspNowClient::begin() {
  if (! _channel)
    _channel = espNowFindServer(_server_mac);
  if (_channel) {
    if (EspNowGeneric::begin()) {
      if (_server_mac[0] || _server_mac[1] || _server_mac[2] || _server_mac[3] || _server_mac[4] || _server_mac[5]) {
        if (! addPeer(_server_mac)) {
          end();
          return false;
        }
      }
      return true;
    }
  }

  return false;
}

int8_t espNowFindServer(uint8_t *mac, int8_t *rssi) {
  int8_t result;
  char ssid[sizeof(ESPNOW_SERVER_AP)];

  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  strcpy_P(ssid, ESPNOW_SERVER_AP);
  result = WiFi.scanNetworks(false, true, 0, (uint8_t*)ssid);
  if (result == 1) {
    result = WiFi.channel(0);
    if (mac)
      memcpy(mac, WiFi.BSSID(0), 6);
    if (rssi)
      *rssi = WiFi.RSSI(0);
  } else
    result = 0;
  WiFi.scanDelete();

  return result;
}

EspNowGeneric *EspNowGeneric::_this;

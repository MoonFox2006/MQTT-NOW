#ifndef __ESPNOWHELPER_H
#define __ESPNOWHELPER_H

#include <inttypes.h>
#include <espnow.h>

class EspNowGeneric {
public:
  EspNowGeneric(uint8_t channel = 0, esp_now_role role = ESP_NOW_ROLE_COMBO) : _channel(channel), _role(role), _sendError(false), _sended(false), _received(false) {
    _this = this;
  }
  virtual ~EspNowGeneric() {
    end();
  }

  virtual bool begin();
  virtual void end();

  bool setMasterKey(const uint8_t *key, uint8_t keylen);

  uint8_t peerCount() const;
  void clearPeers();
  bool findPeer(const uint8_t *mac);
  bool addPeer(const uint8_t *mac, esp_now_role role = ESP_NOW_ROLE_COMBO);
  bool addPeerSecure(const uint8_t *mac, const uint8_t *key, uint8_t keylen, esp_now_role role = ESP_NOW_ROLE_COMBO);
  bool removePeer(const uint8_t *mac);

  bool send(const uint8_t *mac, const uint8_t *data, uint8_t len);
  bool sendAll(const uint8_t *data, uint8_t len) {
    return send(NULL, data, len);
  }
  bool sendBroadcast(const uint8_t *data, uint8_t len);
  bool sendReliable(const uint8_t *mac, const uint8_t *data, uint8_t len, uint8_t repeat = 1, uint32_t timeout = 1);

  bool sendError() const {
    return _sendError;
  }
  bool sended() const {
    return _sended;
  }
  bool received() const {
    return _received;
  }

protected:
  virtual void onReceive(const uint8_t *mac, const uint8_t *data, uint8_t len) {
    _received = true;
  }
  virtual void onSend(const uint8_t *mac, bool error) {
    _sendError = error;
    _sended = true;
  }

  static void _onReceive(uint8_t *mac, uint8_t *data, uint8_t len) {
    _this->onReceive(mac, data, len);
  }
  static void _onSend(uint8_t *mac, uint8_t status) {
    _this->onSend(mac, status != 0);
  }

  static EspNowGeneric *_this;
  uint8_t _channel : 4;
  esp_now_role _role : 2;
  volatile bool _sendError : 1;
  volatile bool _sended : 1;
  volatile bool _received : 1;
};

class EspNowServer : public EspNowGeneric {
public:
  EspNowServer(uint8_t channel = 0) : EspNowGeneric(channel) {}

  bool begin();
  void end();
};

class EspNowClient : public EspNowGeneric {
public:
  EspNowClient(uint8_t channel = 0, const uint8_t *server_mac = NULL) : EspNowGeneric(channel, ESP_NOW_ROLE_CONTROLLER) {
    if (server_mac)
      memcpy(_server_mac, server_mac, sizeof(_server_mac));
  }

  bool begin();

protected:
  uint8_t _server_mac[6];
};

int8_t espNowFindServer(uint8_t *mac = NULL, int8_t *rssi = NULL);

#endif

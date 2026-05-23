#include "EthernetService.h"
#include <Arduino.h>

EthernetService* EthernetService::s_self = nullptr;

EthernetService::EthernetService()
: _spi(&SPI),
  _pinCS(-1), _pinRST(-1), _pinSCK(-1), _pinMISO(-1), _pinMOSI(-1), _pinIRQ(-1),
  _spiHz(8000000),
  _phyAddr(1),
  _mac{0},
  _configured(false),
  _linkUp(false),
  _gotIP(false) {}

bool EthernetService::configure(int8_t pinCS, int8_t pinRST, int8_t pinSCK, int8_t pinMISO,
                                int8_t pinMOSI, int8_t pinIRQ, uint32_t spiHz,
                                const std::array<uint8_t,6>& chosenMac,
                                SPIClass* spi, uint8_t phyAddr) {
  _pinCS = pinCS;
  _pinRST = pinRST;
  _pinSCK = pinSCK;
  _pinMISO = pinMISO;
  _pinMOSI = pinMOSI;
  _pinIRQ = pinIRQ;
  _spiHz = spiHz;
  _spi = spi ? spi : &SPI;
  _phyAddr = phyAddr;
  _mac = chosenMac;

  // Router events
  s_self = this;
  Network.onEvent(&EthernetService::onNetEvent);

  if (_configured) {
    ETH.end();
    _configured = false;
  }

  // Try to set custom MAC
  if (_mac[0] || _mac[1] || _mac[2] || _mac[3] || _mac[4] || _mac[5]) {
    esp_eth_handle_t h = ETH.handle();
    if (h) {
      esp_err_t e = esp_eth_ioctl(h, ETH_CMD_S_MAC_ADDR, (void*)_mac.data());
      (void)e;
    }
  }

  // reset the bus with explicit custom pins immediately
  // before the driver performs its first transaction
  _spi->end();
  delay(1);
  _spi->begin(_pinSCK, _pinMISO, _pinMOSI, _pinCS);
  delay(1);
  pinMode(_pinCS, OUTPUT);
  digitalWrite(_pinCS, HIGH);
  _spi->setFrequency(_spiHz);

  // Start W5500 driver
  if (!ETH.begin(ETH_PHY_W5500, _phyAddr, _pinCS, _pinIRQ, _pinRST, *_spi, (uint8_t)(_spiHz / 1000000))) {
    return false;
  }

  // Hostname
  ETH.setHostname("esp32-bitpirate-eth");

  _configured = true;
  return true;
}

bool EthernetService::beginDHCP(unsigned long timeoutMs) {
  if (!_configured) return false;

  const unsigned long t0 = millis();
  while (millis() - t0 < timeoutMs) {
    if (_linkUp && _gotIP) return true;
    delay(25);
  }
  return false;
}

void EthernetService::onNetEvent(arduino_event_id_t event, arduino_event_info_t info) {
  if (!s_self) return;

  switch (event) {
    case ARDUINO_EVENT_ETH_START:
      break;

    case ARDUINO_EVENT_ETH_CONNECTED:
      s_self->_linkUp = true;
      break;

    case ARDUINO_EVENT_ETH_GOT_IP:
      s_self->_gotIP = true;
      break;

    case ARDUINO_EVENT_ETH_LOST_IP:
      s_self->_gotIP = false;
      break;

    case ARDUINO_EVENT_ETH_DISCONNECTED:
      s_self->_linkUp = false;
      s_self->_gotIP = false;
      break;

    case ARDUINO_EVENT_ETH_STOP:
      s_self->_linkUp = false;
      s_self->_gotIP = false;
      break;

    default:
      break;
  }
}

std::string EthernetService::getMac() const {
  uint8_t m[6] = {0};
  ETH.macAddress(m);
  char buf[18];
  snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X", m[0],m[1],m[2],m[3],m[4],m[5]);
  return std::string(buf);
}

void EthernetService::hardReset() {
    // Reset
    _linkUp = false;
    _gotIP  = false;

    // HW reset pulse
    if (_pinRST >= 0) {
        pinMode(_pinRST, OUTPUT);
        digitalWrite(_pinRST, LOW);
        delay(10);
        digitalWrite(_pinRST, HIGH);
        delay(200);
    }

    if (!_configured) {
        return;
    }
    
    // Restart ETH 
    ETH.end();
    _spi->begin(_pinSCK, _pinMISO, _pinMOSI);
    _spi->setFrequency(_spiHz);
    bool ok = ETH.begin(ETH_PHY_W5500, _phyAddr, _pinCS, _pinIRQ, _pinRST, *_spi, (uint8_t)(_spiHz / 1000000));
    if (ok) {
        ETH.setHostname("esp32-bitpirate-eth");
    }
}

bool EthernetService::isConnected() const { return _linkUp && _gotIP; }
bool EthernetService::linkUp() const { return _linkUp; }
std::string EthernetService::getLocalIP() const   { return ETH.localIP().toString().c_str(); }
std::string EthernetService::getSubnetMask() const{ return ETH.subnetMask().toString().c_str(); }
std::string EthernetService::getGatewayIp() const { return ETH.gatewayIP().toString().c_str(); }
std::string EthernetService::getDns() const       { return ETH.dnsIP().toString().c_str(); }

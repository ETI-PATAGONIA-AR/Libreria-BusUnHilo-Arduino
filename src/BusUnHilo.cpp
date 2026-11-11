/*
  BusUnHilo - Bus casero de un solo hilo, bidireccional, de 8 bits.
  Autor: torres.electronico (Forosdeelectronica)  -  Licencia MIT
*/
#include "BusUnHilo.h"

/* ====================== Base ====================== */

uint8_t BusUnHiloBase::crc8Update(uint8_t crc, uint8_t b) {
  // CRC8 Dallas/Maxim (poly 0x31 reflejado = 0x8C)
  for (uint8_t i = 0; i < 8; i++) {
    uint8_t mix = (crc ^ b) & 1;
    crc >>= 1;
    if (mix) crc ^= 0x8C;
    b >>= 1;
  }
  return crc;
}

uint8_t BusUnHiloBase::crc8(const uint8_t *data, uint8_t len, uint8_t crc) {
  while (len--) crc = crc8Update(crc, *data++);
  return crc;
}

void BusUnHiloBase::pinBegin(uint8_t pin, bool pullup) {
  _pullup = pullup;
#if defined(BUH_SIM)
  (void)pin;
#elif defined(__AVR__)
  uint8_t port = digitalPinToPort(pin);
  _rDDR  = portModeRegister(port);
  _rPORT = portOutputRegister(port);
  _rPIN  = portInputRegister(port);
  _mask  = digitalPinToBitMask(pin);
#elif defined(BUH_USA_REG_ESP)
  _bit = 1UL << pin;
  #if defined(ESP32)
  pinMode(pin, OUTPUT);                       // deja el pin enrutado como GPIO simple
  #endif
  pinMode(pin, pullup ? INPUT_PULLUP : INPUT);
  #if defined(ESP8266)
  GPOC = _bit;                                // latch de salida en 0
  #else
  REG_WRITE(GPIO_OUT_W1TC_REG, _bit);
  #endif
#else
  _num = pin;
#endif
  busRelease();
}

bool BusUnHiloBase::waitLevel(bool level, uint16_t us) {
#if defined(BUH_SIM)
  uint32_t t = micros();
  while ((uint32_t)(micros() - t) < us) {
    if (busRead() == level) return true;
  }
  return false;
#else
  // Solo es un timeout aproximado. AVR: ~12 ciclos por iteracion (contador de 32 bits).
  // Otras placas: iteraciones mas cortas, se divide por menos para no quedarse cortos.
  #if defined(__AVR__)
  const uint32_t ciclosPorIter = 12UL;
  #else
  const uint32_t ciclosPorIter = 4UL;
  #endif
  uint32_t n = ((uint32_t)us * (F_CPU / 1000000UL)) / ciclosPorIter;
  while (n--) {
    if (busRead() == level) return true;
  }
  return false;
#endif
}

/* ====================== Maestro ====================== */

void BusUnHiloMaster::begin(uint8_t pin, bool internalPullup) {
  pinBegin(pin, internalPullup);
  _retries = 3;
  _err = BUH_OK;
}

bool BusUnHiloMaster::reset() {
  busRelease();
  delayMicroseconds(5);
  if (!busRead()) return false;          // linea atascada en bajo
  bool presence;
  {
    BUH_SAVE_IRQ();
    busLow();
    delayMicroseconds(BUH_RESET_LOW_US);
    busRelease();
    delayMicroseconds(BUH_PRESENCE_SAMPLE_US);
    presence = !busRead();
    BUH_RESTORE_IRQ();
  }
  delayMicroseconds(BUH_RESET_REST_US);
  return presence;
}

void BusUnHiloMaster::writeBit(bool b) {
  BUH_SAVE_IRQ();
  busLow();
  delayMicroseconds(b ? BUH_W1_LOW_US : BUH_W0_LOW_US);
  busRelease();
  delayMicroseconds(b ? BUH_W1_REST_US : BUH_W0_REST_US);
  BUH_RESTORE_IRQ();
}

bool BusUnHiloMaster::readBit() {
  bool v;
  BUH_SAVE_IRQ();
  busLow();
  delayMicroseconds(BUH_R_LOW_US);
  busRelease();
  delayMicroseconds(BUH_R_SAMPLE_US);
  v = busRead();
  delayMicroseconds(BUH_R_REST_US);
  BUH_RESTORE_IRQ();
  return v;
}

void BusUnHiloMaster::writeByte(uint8_t v) {
  for (uint8_t i = 0; i < 8; i++) { writeBit(v & 1); v >>= 1; }
}

uint8_t BusUnHiloMaster::readByte() {
  uint8_t v = 0;
  for (uint8_t i = 0; i < 8; i++) { if (readBit()) v |= (1 << i); }
  return v;
}

// Trama: [DIR][CMD][REG]([N] si bloque)([DATOS] si escritura)[CRC8]  -> ACK -> ([DATOS][CRC8] si lectura)
bool BusUnHiloMaster::transact(uint8_t addr, uint8_t cmd, uint8_t reg, uint8_t *buf, uint8_t n) {
  bool isBlock = (cmd == BUH_CMD_WRITE_BLOCK || cmd == BUH_CMD_READ_BLOCK);
  bool isWrite = (cmd == BUH_CMD_WRITE || cmd == BUH_CMD_WRITE_BLOCK);
  bool isRead  = (cmd == BUH_CMD_READ  || cmd == BUH_CMD_READ_BLOCK);
  if (!isBlock) n = (cmd == BUH_CMD_PING) ? 0 : 1;

  if (addr == 255 || (addr == 0 && !isWrite) ||
      (isBlock && (n < 1 || n > BUH_MAX_BLOCK))) { _err = BUH_ERR_ADDR; return false; }

  for (uint8_t attempt = 0; attempt <= _retries; attempt++) {
    if (!reset()) { _err = busRead() ? BUH_ERR_NO_PRESENCE : BUH_ERR_BUS_STUCK; continue; }

    uint8_t crc = 0;
    uint8_t hdr[4];
    uint8_t hl = 0;
    hdr[hl++] = addr; hdr[hl++] = cmd; hdr[hl++] = reg;
    if (isBlock) hdr[hl++] = n;
    for (uint8_t i = 0; i < hl; i++) { writeByte(hdr[i]); crc = crc8Update(crc, hdr[i]); }
    if (isWrite) {
      for (uint8_t i = 0; i < n; i++) { writeByte(buf[i]); crc = crc8Update(crc, buf[i]); }
    }
    writeByte(crc);

    if (addr != 0) {                                  // difusion (0): sin ACK
      delayMicroseconds(BUH_TURNAROUND_US);           // el esclavo valida CRC y prepara datos
      if (readBit()) { _err = BUH_ERR_NO_ACK; continue; }   // ACK = linea en bajo
    }
    if (isRead) {
      uint8_t tmp[BUH_MAX_BLOCK];
      uint8_t c = 0;
      for (uint8_t i = 0; i < n; i++) { tmp[i] = readByte(); c = crc8Update(c, tmp[i]); }
      uint8_t rc = readByte();
      if (rc != c) { _err = BUH_ERR_CRC; continue; }
      for (uint8_t i = 0; i < n; i++) buf[i] = tmp[i];
    }
    _err = BUH_OK;
    return true;
  }
  return false;
}

bool BusUnHiloMaster::ping(uint8_t addr) { return transact(addr, BUH_CMD_PING, 0, NULL, 0); }

bool BusUnHiloMaster::write(uint8_t addr, uint8_t reg, uint8_t data) {
  return transact(addr, BUH_CMD_WRITE, reg, &data, 1);
}

bool BusUnHiloMaster::read(uint8_t addr, uint8_t reg, uint8_t &data) {
  return transact(addr, BUH_CMD_READ, reg, &data, 1);
}

bool BusUnHiloMaster::writeBlock(uint8_t addr, uint8_t reg, const uint8_t *buf, uint8_t n) {
  if (n < 1 || n > BUH_MAX_BLOCK) { _err = BUH_ERR_ADDR; return false; }
  uint8_t tmp[BUH_MAX_BLOCK];
  for (uint8_t i = 0; i < n; i++) tmp[i] = buf[i];
  return transact(addr, BUH_CMD_WRITE_BLOCK, reg, tmp, n);
}

bool BusUnHiloMaster::readBlock(uint8_t addr, uint8_t reg, uint8_t *buf, uint8_t n) {
  return transact(addr, BUH_CMD_READ_BLOCK, reg, buf, n);
}

bool BusUnHiloMaster::identify(uint8_t addr, uint8_t &deviceType) {
  return read(addr, BUH_REG_ID, deviceType);
}

uint8_t BusUnHiloMaster::scan(uint8_t *found, uint8_t maxFound, uint8_t first, uint8_t last) {
  uint8_t savedRetries = _retries;
  _retries = 0;                                      // el escaneo no reintenta
  uint8_t count = 0;
  if (first < 1) first = 1;
  if (last > 254) last = 254;
  for (uint16_t a = first; a <= last && count < maxFound; a++) {
    if (ping((uint8_t)a)) found[count++] = (uint8_t)a;
  }
  _retries = savedRetries;
  return count;
}

const __FlashStringHelper *BusUnHiloMaster::errorText(uint8_t code) {
  switch (code) {
    case BUH_OK:              return F("OK");
    case BUH_ERR_NO_PRESENCE: return F("Sin presencia (nadie respondio al reset)");
    case BUH_ERR_NO_ACK:      return F("Sin ACK (direccion inexistente o trama danada)");
    case BUH_ERR_CRC:         return F("CRC invalido");
    case BUH_ERR_BUS_STUCK:   return F("Bus atascado en bajo");
    case BUH_ERR_ADDR:        return F("Parametro invalido");
  }
  return F("?");
}

/* ====================== Esclavo ====================== */

void BusUnHiloSlave::begin(uint8_t pin, uint8_t address, bool internalPullup) {
  pinBegin(pin, internalPullup);
  _addr = (address == 0 || address == 255) ? 1 : address;
  _devType = BUH_DEV_GENERIC;
  _wreg = 0;
  _wn = 0;
  _onWrite = NULL;
  _onRead = NULL;
}

int8_t BusUnHiloSlave::rxBit(uint16_t timeoutUs) {
  if (!waitLevel(false, timeoutUs)) return -1;       // flanco de bajada = inicio de slot
  delayMicroseconds(BUH_SLV_SAMPLE_US);
  bool v = busRead();
  if (!v) {                                           // era un 0: esperar a que termine
    if (!waitLevel(true, BUH_SLV_BIT_END_US)) return -1;
  }
  return v ? 1 : 0;
}

int16_t BusUnHiloSlave::rxByte(bool first) {
  uint8_t v = 0;
  for (uint8_t i = 0; i < 8; i++) {
    int8_t b = rxBit((first && i == 0) ? BUH_SLV_FIRST_TIMEOUT_US : BUH_SLV_BIT_TIMEOUT_US);
    if (b < 0) return -1;
    if (b) v |= (1 << i);
  }
  return v;
}

bool BusUnHiloSlave::txBit(bool v) {
  if (!waitLevel(false, BUH_SLV_BIT_TIMEOUT_US)) return false;  // el maestro abre el slot
  if (!v) {
    busLow();
    delayMicroseconds(BUH_SLV_HOLD_US);
    busRelease();
  } else {
    waitLevel(true, BUH_SLV_BIT_END_US);   // sincronizar: esperar fin del pulso del maestro
  }
  return true;
}

bool BusUnHiloSlave::txByte(uint8_t v) {
  for (uint8_t i = 0; i < 8; i++) {
    if (!txBit(v & 1)) return false;
    v >>= 1;
  }
  return true;
}

bool BusUnHiloSlave::handleFrame() {
  int16_t a = rxByte(true);   if (a < 0) return false;
  int16_t c = rxByte(false);  if (c < 0) return false;
  if (c < BUH_CMD_WRITE || c > BUH_CMD_READ_BLOCK) return false;
  int16_t rg = rxByte(false); if (rg < 0) return false;
  uint8_t crc = crc8Update(crc8Update(crc8Update(0, (uint8_t)a), (uint8_t)c), (uint8_t)rg);

  bool isBlock = (c == BUH_CMD_WRITE_BLOCK || c == BUH_CMD_READ_BLOCK);
  bool isWrite = (c == BUH_CMD_WRITE || c == BUH_CMD_WRITE_BLOCK);
  bool isRead  = (c == BUH_CMD_READ  || c == BUH_CMD_READ_BLOCK);

  uint8_t n = (c == BUH_CMD_PING) ? 0 : 1;
  if (isBlock) {
    int16_t nn = rxByte(false); if (nn < 0) return false;
    crc = crc8Update(crc, (uint8_t)nn);
    if (nn < 1 || nn > BUH_MAX_BLOCK) return false;
    n = (uint8_t)nn;
  }
  uint8_t buf[BUH_MAX_BLOCK];
  if (isWrite) {
    for (uint8_t i = 0; i < n; i++) {
      int16_t d = rxByte(false); if (d < 0) return false;
      buf[i] = (uint8_t)d;
      crc = crc8Update(crc, buf[i]);
    }
  }
  int16_t r = rxByte(false);  if (r < 0) return false;
  if ((uint8_t)r != crc) return false;                 // CRC malo: silencio -> el maestro reintenta

  bool mine  = ((uint8_t)a == _addr);
  bool bcast = (a == 0 && isWrite);
  if (!mine && !bcast) return false;

  uint8_t out[BUH_MAX_BLOCK];
  if (isRead) {                                        // preparar datos ANTES del ACK (onRead debe ser rapido)
    for (uint8_t i = 0; i < n; i++) {
      uint8_t rr = (uint8_t)(rg + i);
      if (rr == BUH_REG_ID)  out[i] = _devType;
      else if (_onRead)      out[i] = _onRead(rr);
      else                   out[i] = 0xFF;
    }
  }
  if (mine) { if (!txBit(false)) return false; }       // ACK
  if (isRead) {
    uint8_t oc = 0;
    for (uint8_t i = 0; i < n; i++) { if (!txByte(out[i])) return false; oc = crc8Update(oc, out[i]); }
    if (!txByte(oc)) return false;
  }
  if (isWrite) {
    for (uint8_t i = 0; i < n; i++) _wbuf[i] = buf[i];
    _wreg = (uint8_t)rg;
    _wn = n;
  }
  return true;
}

bool BusUnHiloSlave::poll() {
  if (busRead()) return false;                         // linea en reposo
  uint32_t t0 = micros();
  while (!busRead()) {
    if ((uint32_t)(micros() - t0) > BUH_SLV_MAX_RESET_US) return false;   // atascado / muy largo
  }
  if ((uint32_t)(micros() - t0) < BUH_SLV_MIN_RESET_US) return false;     // pulso de bit, no reset

  bool ok;
  _wn = 0;
  {
    BUH_SAVE_IRQ();
    delayMicroseconds(BUH_SLV_PRESENCE_DELAY_US);
    busLow();                                           // pulso de presencia
    delayMicroseconds(BUH_SLV_PRESENCE_US);
    busRelease();
    ok = handleFrame();
    BUH_RESTORE_IRQ();
  }
  if (ok && _wn && _onWrite) {                          // callbacks de escritura, con interrupciones activas
    for (uint8_t i = 0; i < _wn; i++) {
      uint8_t rr = (uint8_t)(_wreg + i);
      if (rr != BUH_REG_ID) _onWrite(rr, _wbuf[i]);
    }
  }
  _wn = 0;
  return ok;
}

/*
  ETI PATAGONIA - prof.martintorres@educ.ar
  web: https://eti-patagonia-ar.github.io/ETI-Patagonia-ARG/index.html
  repositorios: https://github.com/ETI-PATAGONIA-AR
  BusUnHilo - Escaner del bus
  Busca todos los esclavos (direcciones 1..254) y muestra su tipo de dispositivo.
*/
#include <BusUnHilo.h>

BusUnHiloMaster bus;

const __FlashStringHelper *nombreTipo(uint8_t t) {
  switch (t & 0xF0) {
    case BUH_DEV_SENSOR:  return F("sensor");
    case BUH_DEV_DISPLAY: return F("display");
    case BUH_DEV_OUTPUT:  return F("salidas");
  }
  return F("generico");
}

void setup() {
  Serial.begin(57600);
  bus.begin(8);
  Serial.println(F("Escaneando bus..."));

  uint8_t encontrados[16];
  uint8_t n = bus.scan(encontrados, 16);
  if (n == 0) {
    Serial.print(F("Nada encontrado: "));
    Serial.println(BusUnHiloMaster::errorText(bus.lastError()));
  }
  for (uint8_t i = 0; i < n; i++) {
    uint8_t tipo = 0;
    bus.identify(encontrados[i], tipo);
    Serial.print(F("Dir "));  Serial.print(encontrados[i]);
    Serial.print(F(" -> tipo 0x")); Serial.print(tipo, HEX);
    Serial.print(F(" (")); Serial.print(nombreTipo(tipo)); Serial.println(F(")"));
  }
}

void loop() {}

/*
  ETI PATAGONIA - prof.martintorres@educ.ar
  web: https://eti-patagonia-ar.github.io/ETI-Patagonia-ARG/index.html
  repositorios: https://github.com/ETI-PATAGONIA-AR
  BusUnHilo - Maestro basico
  Escribe un registro del esclavo 1 (reg 0x01 -> LED) y lee su sensor (reg 0x02).

  Cableado: pin 8 del maestro y pin 8 del esclavo unidos por UN cable,
  resistencia de 4,7 k desde ese cable a +5V, y GND comun.
*/
#include <BusUnHilo.h>

BusUnHiloMaster bus;
const uint8_t DIR_ESCLAVO = 1;

void setup() {
  Serial.begin(115200);
  bus.begin(8);              // pin 8; pull-up EXTERNA de 4,7k (begin(8, true) usa la interna, mas debil)
}

void loop() {
  static uint8_t estado = 0;
  estado ^= 1;

  if (!bus.write(DIR_ESCLAVO, 0x01, estado)) {
    Serial.print(F("Error al escribir: "));
    Serial.println(BusUnHiloMaster::errorText(bus.lastError()));
  }

  uint8_t valor;
  if (bus.read(DIR_ESCLAVO, 0x02, valor)) {
    Serial.print(F("Sensor = "));
    Serial.println(valor);
  } else {
    Serial.print(F("Error al leer: "));
    Serial.println(BusUnHiloMaster::errorText(bus.lastError()));
  }
  delay(500);
}

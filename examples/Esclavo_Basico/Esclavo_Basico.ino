/*
  ETI PATAGONIA - prof.martintorres@educ.ar
  web: https://eti-patagonia-ar.github.io/ETI-Patagonia-ARG/index.html
  repositorios: https://github.com/ETI-PATAGONIA-AR
  BusUnHilo - Esclavo basico (sensor + LED)
  Direccion 1. Registros:
    0x01 (escritura) : bit0 = enciende/apaga el LED del pin 13
    0x02 (lectura)   : lectura de A0 (0..255)
    0xFF (lectura)   : tipo de dispositivo (automatico)

  REGLAS DEL ESCLAVO:
   - Llamar a bus.poll() lo mas seguido posible (al menos cada ~300 us). No uses delay() largos, implementa millis.
   - onRead() debe devolver ENSEGUIDA (lee variables ya guardadas, no hagas analogRead ahi).
   - Durante una trama (unos ms) las interrupciones estan deshabilitadas.
*/
#include <BusUnHilo.h>

BusUnHiloSlave bus;
uint8_t lectura = 0;

void alEscribir(uint8_t reg, uint8_t valor) {
  if (reg == 0x01) digitalWrite(13, valor & 1);
}

uint8_t alLeer(uint8_t reg) {
  if (reg == 0x02) return lectura;
  return 0xFF;
}

void setup() {
  pinMode(13, OUTPUT);
  bus.begin(8, 1);                    // pin 8, direccion 1
  bus.setDeviceType(BUH_DEV_SENSOR);
  bus.onWrite(alEscribir);
  bus.onRead(alLeer);
}

void loop() {
  bus.poll();

  static unsigned long t = 0;
  if (millis() - t >= 200) {          // el analogRead tarda ~110 us: si justo coincide con una trama,
    t = millis();                     // el maestro reintenta solo
    lectura = analogRead(A0) >> 2;
  }
}

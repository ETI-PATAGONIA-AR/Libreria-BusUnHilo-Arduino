/*
  BusUnHilo - ESCLAVO 2 (direccion 2): potenciometro + LED

  Cableado:
    Bus de datos ..... pin 8  (el mismo cable unico que el esclavo 1 y el maestro)
    Potenciometro .... punta a +5V, punta a GND, cursor a A0
    LED .............. pin 4 -> resistencia 330 ohm -> LED -> GND
    GND comun con el maestro.

  Registros:
    0x01 (escritura) : bit0 = enciende/apaga el LED de este esclavo
    0x02 (lectura)   : valor del pote, 0..255
*/
#include <BusUnHilo.h>

const uint8_t PIN_BUS = 8;
const uint8_t PIN_LED = 4;
const uint8_t PIN_POTE = A0;

BusUnHiloSlave bus;
volatile uint8_t valorPote = 0;

void alEscribir(uint8_t reg, uint8_t valor) {
  if (reg == 0x01) digitalWrite(PIN_LED, valor & 1);
}

uint8_t alLeer(uint8_t reg) {
  if (reg == 0x02) return valorPote;
  return 0xFF;
}

void setup() {
  pinMode(PIN_LED, OUTPUT);
  bus.begin(PIN_BUS, 2);                 // direccion 2
  bus.setDeviceType(BUH_DEV_SENSOR);
  bus.onWrite(alEscribir);
  bus.onRead(alLeer);
}

void loop() {
  bus.poll();

  static unsigned long t = 0;
  if (millis() - t >= 20) {              // analogRead tarda ~110 us: si coincide con una trama,
    t = millis();                        // el maestro reintenta solo
    valorPote = analogRead(PIN_POTE) >> 2;
  }
}

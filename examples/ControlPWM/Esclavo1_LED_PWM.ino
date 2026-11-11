/*
  ETI PATAGONIA - prof.martintorres@educ.ar
  web: https://eti-patagonia-ar.github.io/ETI-Patagonia-ARG/index.html
  repositorios: https://github.com/ETI-PATAGONIA-AR
  BusUnHilo - ESCLAVO 1 (direccion 1): LED con brillo regulable (PWM)

  Cableado:
    Bus de datos ..... pin 8 (el mismo cable unico que el maestro y el esclavo 2)
    LED .............. pin 3 (tiene PWM) -> resistencia 330 ohm -> LED -> GND
    GND comun con el maestro.

  Mapa de registros:
    0x03 (escritura) : brillo del LED, 0 (apagado) a 255 (maximo)
*/
#include <BusUnHilo.h>

const uint8_t PIN_BUS = 8;
const uint8_t PIN_LED_PWM = 3;           // Uno/Nano: PWM en 3, 5, 6, 9, 10, 11
const uint8_t REG_BRILLO = 0x03;

BusUnHiloSlave bus;

void alEscribir(uint8_t reg, uint8_t valor) {
  if (reg == REG_BRILLO) analogWrite(PIN_LED_PWM, valor);   // corto y rapido: no molesta al bus
}

uint8_t alLeer(uint8_t reg) {
  return 0xFF;                           // este esclavo no tiene nada para leer
}

void setup() {
  pinMode(PIN_LED_PWM, OUTPUT);
  bus.begin(PIN_BUS, 1);                 // direccion 1
  bus.setDeviceType(BUH_DEV_OUTPUT);
  bus.onWrite(alEscribir);
  bus.onRead(alLeer);
}

void loop() {
  bus.poll();                            // lo mas seguido posible
}

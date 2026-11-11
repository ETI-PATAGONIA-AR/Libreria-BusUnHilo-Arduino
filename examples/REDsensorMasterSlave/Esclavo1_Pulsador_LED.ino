

/*
  ETI PATAGONIA - prof.martintorres@educ.ar
  web: https://eti-patagonia-ar.github.io/ETI-Patagonia-ARG/index.html
  repositorios: https://github.com/ETI-PATAGONIA-AR
  BusUnHilo - ESCLAVO 1 (direccion 1): pulsador + LED

  Cableado:
    Bus de datos ..... pin 8  (cable unico al maestro y al esclavo 2, pull-up 4,7k a +5V)
    Pulsador ......... pin 2 a GND (usa pull-up interna)
    LED .............. pin 4 -> resistencia 330 ohm -> LED -> GND
    GND comun con el maestro.

  Registros:
    0x01 (escritura) : bit0 = enciende/apaga el LED de este esclavo
    0x02 (lectura)   : 1 = pulsador apretado, 0 = suelto
*/
#include <BusUnHilo.h>

const uint8_t PIN_BUS = 8;
const uint8_t PIN_PULSADOR = 2;
const uint8_t PIN_LED = 4;

BusUnHiloSlave bus;
volatile uint8_t estadoPulsador = 0;     // valor ya medido (onRead debe responder al instante)

void alEscribir(uint8_t reg, uint8_t valor) {
  if (reg == 0x01) digitalWrite(PIN_LED, valor & 1);
}

uint8_t alLeer(uint8_t reg) {
  if (reg == 0x02) return estadoPulsador;
  return 0xFF;
}

void setup() {
  pinMode(PIN_PULSADOR, INPUT_PULLUP);
  pinMode(PIN_LED, OUTPUT);
  bus.begin(PIN_BUS, 1);                 // direccion 1
  bus.setDeviceType(BUH_DEV_GENERIC);
  bus.onWrite(alEscribir);
  bus.onRead(alLeer);
}

void loop() {
  bus.poll();                            // lo mas seguido posible
  estadoPulsador = (digitalRead(PIN_PULSADOR) == LOW) ? 1 : 0;
}

/*
  Sketch para ATtiny85 (Digispark) - Esclavo Sensor Magnetico de Puerta/Ventana
  ID de Esclavo: 0x01
  BusUnHilo en Pin: PB0 (Pin Fisico 5)
  Sensor Magnetico (reed) en Pin: PB3 (Pin Fisico 2)

  Registro 0x00 (lectura): 1 = puerta/ventana ABIERTA, 0 = cerrada.
*/

#include <Arduino.h>
#include <BusUnHilo.h>

const uint8_t PIN_BUS = 0;       // Pin fisico 5 (PB0) para el bus de 1 hilo
const uint8_t PIN_MAGNETICO = 3; // Pin fisico 2 (PB3) para el sensor magnetico
const uint8_t SLAVE_ID = 0x01;

BusUnHiloSlave bus;

uint8_t estadoPuerta = 0;

// Se ejecuta cuando el Maestro pide leer un registro (tiene que responder al instante)
uint8_t handleRead(uint8_t reg) {
  if (reg == 0x00) return estadoPuerta;
  return 0xFF;                   // registro desconocido
}

void setup() {
  pinMode(PIN_MAGNETICO, INPUT_PULLUP);

  // IMPORTANTE: pull-up interna en FALSO. El bus lleva UNA sola pull-up externa (a 3,3 V).
  // Si este esclavo (que esta a 5 V) activa la suya, sube la linea por encima de 3,3 V
  // y puede danar el GPIO del ESP32.
  bus.begin(PIN_BUS, SLAVE_ID, false);

  bus.setDeviceType(BUH_DEV_SENSOR);
  bus.onRead(handleRead);
}

void loop() {
  // reed + imán cerca (puerta cerrada) = contra GND = LOW; sin imán = HIGH = abierta
  estadoPuerta = (digitalRead(PIN_MAGNETICO) == HIGH) ? 1 : 0;
  bus.poll();                    // lo mas seguido posible
}

/*
  Sketch para ATtiny85 (Digispark) - Esclavo Sensor PIR de Movimiento
  ID de Esclavo: 0x02
  BusUnHilo en Pin: PB0 (Pin Físico 5)
  Sensor PIR en Pin: PB4 (Pin Físico 3)
*/

#include <Arduino.h>
#include <BusUnHilo.h>

const uint8_t PIN_BUS = 0;       // Pin físico 5 (PB0) para el bus de 1 hilo
const uint8_t PIN_PIR = 4;       // Pin físico 3 (PB4) para la salida digital del PIR
const uint8_t SLAVE_ID = 0x02;

BusUnHiloSlave bus;

// Variable de estado local
uint8_t estadoPIR = 0;

// Función Callback: Se ejecuta cuando el Maestro pide leer un registro a este esclavo
uint8_t handleRead(uint8_t reg) {
  if (reg == 0x00) {
    // Registro 0x00 devuelve 1 si hay movimiento, 0 en reposo
    return estadoPIR;
  }
  return 0xFF; // Registro desconocido
}

void setup() {
  pinMode(PIN_PIR, INPUT);
  
  // Inicializamos el bus como esclavo (Pin, ID, Pull-up interna en falso porque el PIR suele manejar niveles activos propios)
  bus.begin(PIN_BUS, SLAVE_ID, false);
  
  // Definimos el tipo de dispositivo para el registro de identificación (0xFF)
  bus.setDeviceType(BUH_DEV_SENSOR);
  
  // Asociamos el callback de lectura
  bus.onRead(handleRead);
}

void loop() {
  // Leemos el estado físico del sensor PIR continuamente
  estadoPIR = (digitalRead(PIN_PIR) == HIGH) ? 1 : 0;

  // poll() obligatorio en el loop para atender al Maestro al instante
  bus.poll();
}
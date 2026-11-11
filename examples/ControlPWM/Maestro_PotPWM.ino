/*
  ETI PATAGONIA - prof.martintorres@educ.ar
  web: https://eti-patagonia-ar.github.io/ETI-Patagonia-ARG/index.html
  repositorios: https://github.com/ETI-PATAGONIA-AR
  BusUnHilo - MAESTRO: el pote de un esclavo controla un PWM en dos lugares

  Flujo del dato:
     pote del ESCLAVO 2  --(lee)-->  MAESTRO  --(analogWrite)-->  LED del maestro (pin 9)
                                        |
                                        +--(escribe)-->  ESCLAVO 1  -->  LED con PWM (pin 3 de ese esclavo)

  Usa los sketches: Esclavo2_Pote_LED (del ejemplo anterior, direccion 2) y Esclavo1_LED_PWM (direccion 1).

  Cableado del maestro:
    Bus de datos ..... pin 8 (cable unico) + pull-up 4,7k a +5V
    LED .............. pin 9 (tiene PWM) -> resistencia 330 ohm -> LED -> GND
    GND comun con los dos esclavos.
*/
#include <BusUnHilo.h>

const uint8_t PIN_BUS = 8;
const uint8_t PIN_LED_PWM = 9;           // Uno/Nano: PWM en 3, 5, 6, 9, 10, 11

const uint8_t DIR_LED_PWM = 1, DIR_POTE = 2;
const uint8_t REG_POTE = 0x02;           // lectura en el esclavo 2
const uint8_t REG_BRILLO = 0x03;         // escritura en el esclavo 1

const unsigned long PERIODO_MS = 40;     // cada cuanto se lee el pote
const uint8_t UMBRAL = 2;                // no reenviar si el pote se movio menos que esto

BusUnHiloMaster bus;

int enviado = -1;                        // ultimo brillo que logro llegar al esclavo 1
unsigned long tLectura = 0, tError = 0;

void avisarError(const __FlashStringHelper *quien) {
  if (millis() - tError < 1000) return;  // no saturar el monitor serie
  tError = millis();
  Serial.print(quien);
  Serial.print(F(": "));
  Serial.println(BusUnHiloMaster::errorText(bus.lastError()));
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_LED_PWM, OUTPUT);
  bus.begin(PIN_BUS);                    // pull-up EXTERNA de 4,7k
}

void loop() {
  if (millis() - tLectura < PERIODO_MS) return;
  tLectura = millis();

  uint8_t pote;                          // 1) leer el pote del esclavo 2
  if (!bus.read(DIR_POTE, REG_POTE, pote)) { avisarError(F("Leyendo pote")); return; }

  analogWrite(PIN_LED_PWM, pote);        // 2) usarlo aca mismo: brillo del LED del maestro

  if (enviado < 0 || abs((int)pote - enviado) >= UMBRAL) {   // 3) y mandarlo al esclavo 1 (solo si cambio)
    if (bus.write(DIR_LED_PWM, REG_BRILLO, pote)) enviado = pote;
    else avisarError(F("Escribiendo brillo"));
  }
}

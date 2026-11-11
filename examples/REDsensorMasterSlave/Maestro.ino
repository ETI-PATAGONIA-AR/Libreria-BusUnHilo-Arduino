/*
  ETI PATAGONIA - prof.martintorres@educ.ar
  web: https://eti-patagonia-ar.github.io/ETI-Patagonia-ARG/index.html
  repositorios: https://github.com/ETI-PATAGONIA-AR
  BusUnHilo - MAESTRO con 2 esclavos

  Funcionamiento:
    - Pulsador A (pin 5) -> mientras lo apretas, se enciende el LED del ESCLAVO 1.
    - Pulsador B (pin 6) -> mientras lo apretas, se enciende el LED del ESCLAVO 2.
    - LED 1 del maestro (pin 9)  = se enciende si el pulsador del ESCLAVO 1 esta apretado.
    - LED 2 del maestro (pin 10) = se enciende si el pote del ESCLAVO 2 pasa de la mitad.

  Cableado del maestro:
    Bus de datos ..... pin 8 (cable unico a los dos esclavos) + pull-up 4,7k a +5V
    Pulsador A ....... pin 5 a GND   |   Pulsador B ....... pin 6 a GND   (pull-up interna)
    LED 1 ............ pin 9  -> 330 ohm -> LED -> GND
    LED 2 ............ pin 10 -> 330 ohm -> LED -> GND
    GND comun con los dos esclavos.

  Monitor serie a 115200 para ver el estado y los errores del bus.
*/
#include <BusUnHilo.h>

const uint8_t PIN_BUS = 8;
const uint8_t PIN_BTN_A = 5, PIN_BTN_B = 6;
const uint8_t PIN_LED_1 = 9, PIN_LED_2 = 10;

const uint8_t DIR_ESCLAVO1 = 1, DIR_ESCLAVO2 = 2;
const uint8_t REG_LED = 0x01;            // escritura en el esclavo
const uint8_t REG_DATO = 0x02;           // lectura: pulsador (esclavo 1) / pote (esclavo 2)

const uint8_t POTE_ON = 140, POTE_OFF = 115;   // histeresis para que no parpadee
const unsigned long PERIODO_LECTURA_MS = 40;

BusUnHiloMaster bus;

// lo ultimo que se logro enviar a cada esclavo (255 = nada enviado todavia)
uint8_t enviado1 = 255, enviado2 = 255;
bool led2Encendido = false;
unsigned long tLectura = 0, tError = 0;

void avisarError(const __FlashStringHelper *quien) 
    {
      if (millis() - tError < 1000) return;  // no saturar el monitor serie
      tError = millis();
      Serial.print(quien);
      Serial.print(F(": "));
      Serial.println(BusUnHiloMaster::errorText(bus.lastError()));
    }

void setup() 
    {
      Serial.begin(57600); //Serial.begin(115200);
      pinMode(PIN_BTN_A, INPUT_PULLUP);
      pinMode(PIN_BTN_B, INPUT_PULLUP);
      pinMode(PIN_LED_1, OUTPUT);
      pinMode(PIN_LED_2, OUTPUT);
      bus.begin(PIN_BUS);  // usar pull-up EXTERNA de 4,7k
      Serial.println(F("Maestro listo"));
    }

void loop() 
    {
      // 1) Pulsadores del maestro -> LED del esclavo correspondiente (solo si cambia, o si fallo el envio)
      uint8_t a = (digitalRead(PIN_BTN_A) == LOW) ? 1 : 0;
      uint8_t b = (digitalRead(PIN_BTN_B) == LOW) ? 1 : 0;
      if (a != enviado1) 
        {
          if (bus.write(DIR_ESCLAVO1, REG_LED, a)) enviado1 = a;
          else avisarError(F("Esclavo 1 (escritura)"));
        }
      if (b != enviado2) 
        {
          if (bus.write(DIR_ESCLAVO2, REG_LED, b)) enviado2 = b;
          else avisarError(F("Esclavo 2 (escritura)"));
        }
      // 2) Leer los esclavos cada tanto -> LEDs del maestro
      if (millis() - tLectura >= PERIODO_LECTURA_MS) 
        {
          tLectura = millis();
          uint8_t pulsador;
          if (bus.read(DIR_ESCLAVO1, REG_DATO, pulsador)) digitalWrite(PIN_LED_1, pulsador ? HIGH : LOW);
          else avisarError(F("Esclavo 1 (lectura)"));
          uint8_t pote;
          if (bus.read(DIR_ESCLAVO2, REG_DATO, pote)) 
            {
            if (!led2Encendido && pote > POTE_ON)  led2Encendido = true;
            if (led2Encendido  && pote < POTE_OFF) led2Encendido = false;
            digitalWrite(PIN_LED_2, led2Encendido ? HIGH : LOW);
            } 
            else avisarError(F("Esclavo 2 (lectura)"));
        }
    }

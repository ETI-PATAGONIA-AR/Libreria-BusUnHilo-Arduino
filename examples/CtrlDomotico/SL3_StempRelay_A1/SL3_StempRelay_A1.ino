/*
  Sketch para Arduino UNO - Esclavo (DHT11 + 4 reles + 4 pulsadores locales)
  ID de Esclavo: 0x03
  BusUnHilo en Pin: Digital 2
  Sensor DHT11 en Pin: Digital 4
  Reles: Pines 5, 6, 7, 8        Pulsadores (a GND): Pines 9, 10, 11, 12

  Registros:
    0x00 (escritura) : mascara de reles (bit0..bit3), el maestro ordena el estado completo
    0x00 (lectura)   : temperatura en grados C (0xFF = sin lectura todavia)
    0x01 (lectura)   : humedad en %           (0xFF = sin lectura todavia)
    0x02 (lectura)   : estado real de los reles (mascara)
    0x03 (lectura)   : pulsadores locales apretados (mascara)
*/

#include <Arduino.h>
#include <BusUnHilo.h>
#include <DHT.h>

// --- Pines ---
const uint8_t PIN_BUS = 2;
const uint8_t PIN_DHT = 4;
const uint8_t PINES_RELES[4] = {5, 6, 7, 8};
const uint8_t PINES_BOTONES[4] = {9, 10, 11, 12};

const bool RELE_ACTIVO_ALTO = false;      // tu shield activa con LOW. Poner true si activa con HIGH

#define DHTTYPE DHT11
DHT dht(PIN_DHT, DHTTYPE);

const uint8_t SLAVE_ID = 0x03;
BusUnHiloSlave bus;

const uint8_t DATO_INVALIDO = 0xFF;
const unsigned long PERIODO_DHT_MS = 2500;   // el DHT11 no da mas de 1 lectura cada ~2 s
const unsigned long DEBOUNCE_MS = 50;

// Todo lo que lee el maestro esta en variables: los callbacks del bus tienen que ser instantaneos
uint8_t mascaraReles = 0;
uint8_t mascaraBotones = 0;
uint8_t tempByte = DATO_INVALIDO;
uint8_t humByte = DATO_INVALIDO;

bool btnLectura[4] = {false, false, false, false};
bool btnEstable[4] = {false, false, false, false};
unsigned long btnT[4] = {0, 0, 0, 0};
unsigned long tDHT = 0;

void aplicarReles(uint8_t mascara) {
  mascaraReles = mascara & 0x0F;
  for (uint8_t i = 0; i < 4; i++) {
    bool encendido = (mascaraReles >> i) & 1;
    digitalWrite(PINES_RELES[i], (encendido == RELE_ACTIVO_ALTO) ? HIGH : LOW);
  }
}

// El maestro escribe el registro 0x00: estado completo de los reles
void handleWrite(uint8_t reg, uint8_t val) {
  if (reg == 0x00) aplicarReles(val);
}

// El maestro lee: se responde con lo ya calculado (sin tocar pines ni el DHT)
uint8_t handleRead(uint8_t reg) {
  switch (reg) {
    case 0x00: return tempByte;
    case 0x01: return humByte;
    case 0x02: return mascaraReles;
    case 0x03: return mascaraBotones;
  }
  return 0xFF;
}

// Pulsadores locales: antirrebote y UNA conmutacion por pulsacion (no se repite si lo mantenes)
void leerBotones() {
  for (uint8_t i = 0; i < 4; i++) {
    bool p = (digitalRead(PINES_BOTONES[i]) == LOW);
    if (p != btnLectura[i]) { btnLectura[i] = p; btnT[i] = millis(); }
    if (millis() - btnT[i] >= DEBOUNCE_MS && p != btnEstable[i]) {
      btnEstable[i] = p;
      if (p) aplicarReles(mascaraReles ^ (1 << i));
    }
  }
  uint8_t m = 0;
  for (uint8_t i = 0; i < 4; i++) if (btnEstable[i]) m |= (1 << i);
  mascaraBotones = m;
}

// El DHT11 tarda ~25 ms en leerse y mientras tanto no se atiende el bus: por eso solo cada 2,5 s.
// El maestro reintenta, asi que a lo sumo pierde un intento.
void leerDHT() {
  if (millis() - tDHT < PERIODO_DHT_MS) return;
  tDHT = millis();
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (!isnan(t)) tempByte = (uint8_t)t;     // si falla, se conserva el ultimo valor bueno
  if (!isnan(h)) humByte = (uint8_t)h;
}

void setup() {
  dht.begin();

  for (uint8_t i = 0; i < 4; i++) {
    digitalWrite(PINES_RELES[i], RELE_ACTIVO_ALTO ? LOW : HIGH);   // apagado ANTES de ser salida
    pinMode(PINES_RELES[i], OUTPUT);
    pinMode(PINES_BOTONES[i], INPUT_PULLUP);
  }

  // IMPORTANTE: pull-up interna en FALSO (el bus lleva una sola pull-up externa a 3,3 V)
  bus.begin(PIN_BUS, SLAVE_ID, false);
  bus.setDeviceType(BUH_DEV_GENERIC);
  bus.onWrite(handleWrite);
  bus.onRead(handleRead);
}

void loop() {
  bus.poll();               // lo mas seguido posible
  leerBotones();
  leerDHT();
}

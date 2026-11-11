/*
  ETI PATAGONIA - prof.martintorres@educ.ar
  web: https://eti-patagonia-ar.github.io/ETI-Patagonia-ARG/index.html
  repositorios: https://github.com/ETI-PATAGONIA-AR
  PORTERO ELECTRICO con BusUnHilo  -  ESCLAVO (un Arduino UNO/NANO por departamento)

  Hace lo siguiente:
   - Cuando el maestro lo ordena, suena un "ding dong" dos veces (buzzer pasivo en D9).
   - Informa al maestro el estado de la llave (D3) y del pulsador de abrir (D2).
   - Cierra el rele del intercomunicador (D13) SOLO si el maestro le da permiso Y la llave esta
     cerrada. El permiso hay que renovarlo: si el maestro deja de renovarlo (bus caido, maestro
     colgado) el rele se suelta solo al segundo. Si se abre la llave, el rele se suelta al instante.

  Cableado (UNO/NANO):
    Bus de datos ........ D8 (cable unico, pull-up 4,7k a +5V en todo el bus)
    Pulsador de abrir ... D2 contra GND (pull-up interna)
    Llave (atender) ..... D3 contra GND (pull-up interna): CERRADA = contra GND
    Buzzer pasivo ....... D9 (tono por hardware con el Timer1, no se corta con el bus)
    Rele intercom ....... D13
    GND comun con el maestro.

  IMPORTANTE: es el MISMO sketch para todos los departamentos. Solo se cambia DIR_DEPTO.
  El Timer1 queda ocupado por el tono (no usar analogWrite en D9/D10 ni la libreria Servo).
*/
#include <BusUnHilo.h>

// ============================ CONFIGURACION ============================

// --- Logica de el RELE del intercom (D13): dejar SIN comentar una sola de las dos lineas ---
//#define RELE_EN_ALTO      // se activa con nivel ALTO (HIGH)
#define RELE_EN_BAJO    // se activa con nivel BAJO (LOW)

const uint8_t DIR_DEPTO = 2;                  // <<< numero de este departamento (1..10), distinto en cada placa
const uint8_t PIN_BUS = 8;
const uint8_t PIN_PULSADOR = 2;
const uint8_t PIN_LLAVE = 3;
const uint8_t PIN_TONO = 9;                   // OC1A: no cambiar (es el pin del Timer1)
const uint8_t PIN_RELAY = 13;

const unsigned long WATCHDOG_MS = 1000;       // sin renovacion del maestro => se suelta el rele
// ======================================================================

#if defined(RELE_EN_ALTO) && defined(RELE_EN_BAJO)
#error "Rele (D13): hay dos logicas activas, comenta una de las dos lineas"
#elif !defined(RELE_EN_ALTO) && !defined(RELE_EN_BAJO)
#error "Rele (D13): falta elegir la logica, descomenta una de las dos lineas"
#endif
#ifdef RELE_EN_ALTO
const bool RELE_ACTIVO_ALTO = true;
#else
const bool RELE_ACTIVO_ALTO = false;
#endif

// Mapa de registros (tiene que coincidir con Portero_Master.ino)
const uint8_t REG_ESTADO = 0x01;   // lectura : bit0 = llave cerrada, bit1 = pulsador de abrir apretado
const uint8_t REG_TIMBRE = 0x02;   // escritura: 1 = sonar ding dong
const uint8_t REG_LINEA  = 0x03;   // escritura: 1 = linea permitida (se renueva), 0 = soltar
const uint8_t BIT_LLAVE = 0x01, BIT_ABRIR = 0x02;

BusUnHiloSlave bus;

volatile uint8_t estadoBits = 0;              // lo que se informa al maestro
bool permiso = false;                         // el maestro dio permiso de linea
unsigned long tPermiso = 0;
bool llaveAnterior = false;

// ---------------- timbre: ding dong x2 sin bloquear el loop ----------------
struct Nota { uint16_t hz; uint16_t ms; };    // hz = 0 es silencio
const Nota DING_DONG[] = 
    {
     {659, 450}, {0, 60}, {523, 700}, {0, 500},  // ding ... dong
     {659, 450}, {0, 60}, {523, 700}, {0, 300}   // ding ... dong
    };

const uint8_t N_NOTAS = sizeof(DING_DONG) / sizeof(DING_DONG[0]);
bool timbreActivo = false;
uint8_t notaActual = 0;
unsigned long tNota = 0;

void tonoOff() 
  {
   TCCR1B = 0;
   TCCR1A = 0;
   digitalWrite(PIN_TONO, LOW);
  }

void tonoOn(uint16_t hz)        // Timer1 en CTC, toggle de OC1A, prescaler 8
  {                    
   TCCR1B = 0;
   TCCR1A = 0;
   TCNT1 = 0;
   OCR1A = (uint16_t)(F_CPU / (16UL * hz) - 1);
   TCCR1A = _BV(COM1A0);
   TCCR1B = _BV(WGM12) | _BV(CS11);
  }

void aplicarNota() 
   {
    tNota = millis();
    if (DING_DONG[notaActual].hz) tonoOn(DING_DONG[notaActual].hz);
    else tonoOff();
   }

void iniciarTimbre() 
   {
    notaActual = 0;
    timbreActivo = true;
    aplicarNota();
   }

void pararTimbre() 
   {
    timbreActivo = false;
    tonoOff();
   }

void timbreLoop() 
   {
    if (!timbreActivo) return;
    if (millis() - tNota >= DING_DONG[notaActual].ms) 
     {
      if (++notaActual >= N_NOTAS) { pararTimbre(); return; }
      aplicarNota();
     }
   }

// ---------------- bus ----------------
void alEscribir(uint8_t reg, uint8_t valor) 
   {
    if (reg == REG_TIMBRE && (valor & 1)) 
     {
      iniciarTimbre();
     }
    else if (reg == REG_LINEA) 
     {
      if (valor & 1) { permiso = true; tPermiso = millis(); }
      else permiso = false;
     }
   }

uint8_t alLeer(uint8_t reg) 
      {
       if (reg == REG_ESTADO) return estadoBits;
       return 0xFF;
      }

// ---------------- entradas con antirrebote (20 ms) ----------------
bool entrada(uint8_t pin, bool &ultima, bool &estable, unsigned long &t) 
   {
     bool lectura = (digitalRead(pin) == LOW);   // "activo" = contra GND
     if (lectura != ultima) { ultima = lectura; t = millis(); }
     if (millis() - t >= 20) estable = lectura;
     return estable;
   }

void setRele(bool encendido) 
   {
     digitalWrite(PIN_RELAY, (encendido == RELE_ACTIVO_ALTO) ? HIGH : LOW);
   }
///////////////////////////////////////////////////////////////
void setup() 
   {
     setRele(false);                             // apagado ANTES de ser salida
     pinMode(PIN_RELAY, OUTPUT);
     pinMode(PIN_TONO, OUTPUT);
     digitalWrite(PIN_TONO, LOW);
     pinMode(PIN_PULSADOR, INPUT_PULLUP);
     pinMode(PIN_LLAVE, INPUT_PULLUP);
     bus.begin(PIN_BUS, DIR_DEPTO);
     bus.setDeviceType(BUH_DEV_GENERIC);
     bus.onWrite(alEscribir);
     bus.onRead(alLeer);
   }
///////////////////////////////////////////////////////////////
void loop() 
    {
      bus.poll();                                 // lo mas seguido posible
      static bool uL = false, eL = false, uP = false, eP = false;
      static unsigned long tL = 0, tP = 0;
      bool llave = entrada(PIN_LLAVE, uL, eL, tL);
      bool abrir = entrada(PIN_PULSADOR, uP, eP, tP);
      estadoBits = (llave ? BIT_LLAVE : 0) | (abrir ? BIT_ABRIR : 0);

      if (!llave) permiso = false;                // llave abierta: se corta todo
      if (llave && !llaveAnterior) pararTimbre(); // recien atendio: se calla el timbre
      llaveAnterior = llave;

      bool lineaOn = permiso && llave && (millis() - tPermiso < WATCHDOG_MS);
      setRele(lineaOn);
      timbreLoop();
    }
///////////////////////////////////////////////////////////////
/*
  ETI PATAGONIA - prof.martintorres@educ.ar
  web: https://eti-patagonia-ar.github.io/ETI-Patagonia-ARG/index.html
  repositorios: https://github.com/ETI-PATAGONIA-AR
  PORTERO ELECTRICO con BusUnHilo  -  MAESTRO (panel de la calle)  -  v2

  Funcionamiento:
   1) En reposo, el maestro espera una llamada (pulsador de un depto) o que algun depto
      CIERRE su llave (pasa de abierta a cerrada). En reposo recorre los deptos leyendo su llave.
   2) Llamada: le ordena al esclavo de ese depto que suene el "ding dong" (2 veces). Si ese depto
      cierra la llave dentro de T_VENTANA_MS, atiende.
   3) Atender (con o sin llamada previa, siempre que el bus este libre): el maestro le concede la
      linea de audio al depto, activa la salida PIN_AUDIO_GND y se la renueva cada 200 ms.
      Mientras dure, ningun otro depto puede usar la linea ni se aceptan llamadas (privacidad).
   4) CERRADURA: el rele se energiza SOLO mientras el depto tenga la llave cerrada Y el pulsador
      apretado. Al soltar el pulsador (o abrir la llave, o fallar el bus) se desenergiza enseguida.
   5) Cuando el depto ABRE la llave (o el bus falla), se corta todo (audio, linea, cerradura)
      y se vuelve a reposo.

  Cableado (UNO/NANO):
    Bus de datos ........ D8 (cable unico a todos los esclavos, pull-up 4,7k a +5V)
    Pulsadores deptos ... D2..D7, D9..D12 contra GND (pull-up interna). Depto 1 = D2, depto 2 = D3...
    Rele de la cerradura. D13
    GND de audio ........ A0 (usado como salida digital): activa mientras algun depto atiende
    GND comun con todos los esclavos.

  NOTA: al reiniciar el Arduino, el bootloader hace parpadear D13 un instante: la cerradura
  podria hacer un "click". Si molesta, se cambia de pin.

  Si una llave ya esta cerrada al encender (o mientras otro depto habla), no cuenta: hay que
  abrirla y volver a cerrarla para atender.

  Para cambiar la cantidad de departamentos: editar NUM_DEPTOS aqui abajo (no la libreria).
*/
#include <BusUnHilo.h>

// ============================ CONFIGURACION ============================

// --- Logica de la CERRADURA (rele en D13): dejar SIN comentar una sola de las dos lineas ---
#define CERRADURA_EN_ALTO      // se activa con nivel ALTO (HIGH)
//#define CERRADURA_EN_BAJO    // se activa con nivel BAJO (LOW)

// --- Logica del GND de AUDIO (A0): dejar SIN comentar una sola de las dos lineas ---
//#define AUDIO_EN_ALTO      // se activa con nivel ALTO (HIGH)
#define AUDIO_EN_BAJO    // se activa con nivel BAJO (LOW)

const uint8_t NUM_DEPTOS = 2;                 // <<< cantidad de departamentos (1 a 10)

// Pin del pulsador de cada depto: depto 1 = primer pin, depto 2 = segundo...
// D8 queda para el bus. Se usan solo los primeros NUM_DEPTOS.
const uint8_t PIN_PULSADORES[] = {2, 3, 4, 5, 6, 7, 9, 10, 11, 12};

const uint8_t PIN_BUS = 8;
const uint8_t PIN_RELAY_CERRADURA = 13;

const uint8_t PIN_AUDIO_GND = A0;             // habilita el GND del audio del maestro mientras alguien atiende

const unsigned long T_VENTANA_MS = 30000;     // tiempo para atender despues del llamado
const long BAUDIOS = 57600;                   //en fisico: 115200;    // en Proteus: 57600
// ======================================================================

#if defined(CERRADURA_EN_ALTO) && defined(CERRADURA_EN_BAJO)
#error "Cerradura (D13): hay dos logicas activas, comenta una de las dos lineas"
#elif !defined(CERRADURA_EN_ALTO) && !defined(CERRADURA_EN_BAJO)
#error "Cerradura (D13): falta elegir la logica, descomenta una de las dos lineas"
#endif
#if defined(AUDIO_EN_ALTO) && defined(AUDIO_EN_BAJO)
#error "Audio GND (A0): hay dos logicas activas, comenta una de las dos lineas"
#elif !defined(AUDIO_EN_ALTO) && !defined(AUDIO_EN_BAJO)
#error "Audio GND (A0): falta elegir la logica, descomenta una de las dos lineas"
#endif
#ifdef CERRADURA_EN_ALTO
const bool RELE_ACTIVO_ALTO = true;
#else
const bool RELE_ACTIVO_ALTO = false;
#endif
#ifdef AUDIO_EN_ALTO
const bool AUDIO_ACTIVO_ALTO = true;
#else
const bool AUDIO_ACTIVO_ALTO = false;
#endif

static_assert(NUM_DEPTOS >= 1 && NUM_DEPTOS <= sizeof(PIN_PULSADORES),
              "NUM_DEPTOS debe estar entre 1 y la cantidad de pines de PIN_PULSADORES");

// Mapa de registros de cada esclavo (tiene que coincidir con Portero_Esclavo.ino)
const uint8_t REG_ESTADO = 0x01;   // lectura : bit0 = llave cerrada, bit1 = pulsador de abrir apretado
const uint8_t REG_TIMBRE = 0x02;   // escritura: 1 = sonar ding dong
const uint8_t REG_LINEA  = 0x03;   // escritura: 1 = linea permitida (se renueva), 0 = soltar
const uint8_t BIT_LLAVE = 0x01, BIT_ABRIR = 0x02;

const unsigned long PERIODO_REPOSO_MS = 30;     // en reposo: cada cuanto se lee la llave de UN depto (se van rotando)
const unsigned long PERIODO_LLAMADA_MS = 80;    // llamando: cada cuanto se lee la llave del depto llamado
const unsigned long PERIODO_COM_MS = 10;        // comunicando: cada cuanto se lee llave+pulsador (rapidez de la cerradura)
const unsigned long PERIODO_REFRESCO_MS = 200;  // cada cuanto se renueva el permiso de linea
const uint8_t MAX_FALLOS = 5;                   // lecturas/escrituras seguidas que fallan => se corta

const uint8_t DESCONOCIDA = 0xFF;               // estado de llave aun no leido

BusUnHiloMaster bus;

enum Estado { REPOSO, LLAMANDO, COMUNICANDO };
Estado estado = REPOSO;
uint8_t depto = 0;                              // depto en llamada / comunicacion (1..NUM_DEPTOS)
unsigned long tLlamada = 0, tPoll = 0, tRefresco = 0;
bool cerraduraAbierta = false;
uint8_t fallos = 0;
uint8_t proximo = 0;                            // depto que se lee a continuacion en reposo (0..NUM-1)
uint8_t llavePrev[sizeof(PIN_PULSADORES)];      // ultima llave leida: 0 abierta, 1 cerrada, DESCONOCIDA

bool btnApretado[sizeof(PIN_PULSADORES)];
unsigned long btnT[sizeof(PIN_PULSADORES)];

void setRele(uint8_t pin, bool encendido) 
    {
      digitalWrite(pin, (encendido == RELE_ACTIVO_ALTO) ? HIGH : LOW); 
    }

void setAudio(bool habilitado) 
    {
      digitalWrite(PIN_AUDIO_GND, (habilitado == AUDIO_ACTIVO_ALTO) ? HIGH : LOW);  
    }

// la cerradura solo cambia por aca; se corta SIEMPRE antes de cualquier otra cosa
void setCerradura(bool abierta) 
   {
    if (abierta == cerraduraAbierta) return;
    cerraduraAbierta = abierta;
    setRele(PIN_RELAY_CERRADURA, abierta);
    Serial.println(abierta ? F("Cerradura abierta") : F("Cerradura cerrada"));
   }

// despues de una comunicacion no se sabe quien cerro/abrio su llave: se vuelve a aprender sin disparar nada
void olvidarLlaves() 
   {
    for (uint8_t i = 0; i < NUM_DEPTOS; i++) llavePrev[i] = DESCONOCIDA;
   }

// true UNA vez por cada pulsacion (con antirrebote)
bool flancoBoton(uint8_t i) 
   {
    bool p = (digitalRead(PIN_PULSADORES[i]) == LOW);
    if (p != btnApretado[i] && millis() - btnT[i] > 30) 
      {
       btnT[i] = millis();
       btnApretado[i] = p;
       return p;
      }
    return false;
   }

void terminarComunicacion(const __FlashStringHelper *motivo) 
   {
    setCerradura(false);                          // primero lo critico, antes de tocar el bus
    setAudio(false);
    Serial.print(F("Fin de comunicacion: "));
    Serial.println(motivo);
    bus.write(0, REG_LINEA, 0);                   // difusion: todos sueltan la linea
    estado = REPOSO;
    depto = 0;
    olvidarLlaves();
   }

void contarFallo() 
   {
    setCerradura(false);                          // sin informacion del depto, la cerradura no se mantiene
    if (++fallos >= MAX_FALLOS) terminarComunicacion(F("el departamento no responde"));
   }

void iniciarComunicacion(uint8_t d) 
   {
     depto = d;
     estado = COMUNICANDO;
     fallos = 0;
     tRefresco = 0;                                // fuerza renovar el permiso ya mismo
     tPoll = 0;
     setAudio(true);
     Serial.print(F("Depto "));
     Serial.print(d);
     Serial.println(F(" atendio: linea de audio concedida"));
   }

void iniciarLlamada(uint8_t d) 
   {
     if (!bus.write(d, REG_TIMBRE, 1)) 
      {
       Serial.print(F("Depto "));
       Serial.print(d);
       Serial.print(F(" no responde: "));
       Serial.println(BusUnHiloMaster::errorText(bus.lastError()));
       return;
      }
     uint8_t e;                                    // estado inicial de la llave: hay que ver abrir->cerrar
     llavePrev[d - 1] = bus.read(d, REG_ESTADO, e) ? ((e & BIT_LLAVE) ? 1 : 0) : 0;
     depto = d;
     estado = LLAMANDO;
     tLlamada = tPoll = millis();
     Serial.print(F("Llamando al depto "));
     Serial.println(d);
   }

// REPOSO: se lee la llave de un depto por vez; si pasa de abierta a cerrada, atiende (sin llamada)
void atenderReposo() 
   {
    if (millis() - tPoll < PERIODO_REPOSO_MS) return;
    tPoll = millis();
    uint8_t i = proximo;
    proximo = (proximo + 1) % NUM_DEPTOS;
    uint8_t e;
    if (!bus.read(i + 1, REG_ESTADO, e)) { llavePrev[i] = DESCONOCIDA; return; }
    uint8_t llave = (e & BIT_LLAVE) ? 1 : 0;
    bool atiende = (llave == 1 && llavePrev[i] == 0);
    llavePrev[i] = llave;
    if (atiende) iniciarComunicacion(i + 1);
   }

void atenderLlamando() 
   {
    if (millis() - tLlamada > T_VENTANA_MS) 
      {
       Serial.println(F("Llamada sin respuesta"));
       estado = REPOSO;
       depto = 0;
       olvidarLlaves();
       return;
      }
    if (millis() - tPoll < PERIODO_LLAMADA_MS) return;
    tPoll = millis();
    uint8_t e;
    if (!bus.read(depto, REG_ESTADO, e)) return;   // se vuelve a intentar en el proximo ciclo
    uint8_t llave = (e & BIT_LLAVE) ? 1 : 0;
    bool atiende = (llave == 1 && llavePrev[depto - 1] == 0);
    llavePrev[depto - 1] = llave;
    if (atiende) iniciarComunicacion(depto);
   }

void atenderComunicacion() 
   {
     unsigned long ahora = millis();
     if (ahora - tRefresco >= PERIODO_REFRESCO_MS) 
       {
         tRefresco = ahora;
         if (!bus.write(depto, REG_LINEA, 1)) { contarFallo(); if (estado != COMUNICANDO) return; }
       }
     if (ahora - tPoll >= PERIODO_COM_MS) 
       {
        tPoll = ahora;
        uint8_t e;
        if (!bus.read(depto, REG_ESTADO, e)) { contarFallo(); return; }
        fallos = 0;
        if (!(e & BIT_LLAVE)) { terminarComunicacion(F("llave abierta")); return; }
        // llave cerrada (validado): la cerradura sigue al pulsador, sin memoria ni temporizador
        setCerradura((e & BIT_ABRIR) != 0);
       }
   }
////////////////////////////////////////////////////////
void setup() 
   {
     Serial.begin(BAUDIOS);
     setRele(PIN_RELAY_CERRADURA, false);          // apagado ANTES de ser salida
     pinMode(PIN_RELAY_CERRADURA, OUTPUT);
     setAudio(false);
     pinMode(PIN_AUDIO_GND, OUTPUT);
     for (uint8_t i = 0; i < NUM_DEPTOS; i++) 
       {
        pinMode(PIN_PULSADORES[i], INPUT_PULLUP);
        btnApretado[i] = false;
        btnT[i] = 0;
       }
     olvidarLlaves();
     bus.begin(PIN_BUS);                           // pull-up EXTERNA de 4,7k
     bus.write(0, REG_LINEA, 0);                   // por las dudas, todos sueltan la linea
     Serial.print(F("Portero listo. Departamentos: "));
     Serial.println(NUM_DEPTOS);
   }
////////////////////////////////////////////////////////
void loop() 
   {
    for (uint8_t i = 0; i < NUM_DEPTOS; i++)    // pulsadores de llamada (no se aceptan llamadas mientras hay una comunicacion)
      {
       bool p = flancoBoton(i);
       if (p && estado != COMUNICANDO) iniciarLlamada(i + 1);
      }
    if (estado == REPOSO) atenderReposo();
    else if (estado == LLAMANDO) atenderLlamando();
    else atenderComunicacion();
   }
////////////////////////////////////////////////////////


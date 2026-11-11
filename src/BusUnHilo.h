/*
  BusUnHilo - Bus casero de un solo hilo, bidireccional, de 8 bits.
  Autor: torres.electronico (Forosdeelectronica)  -  Licencia MIT

  Idea: un unico cable de datos "drenador abierto" (wired-AND) con una
  resistencia pull-up. Nadie maneja la linea en alto: o se tira a GND o se
  suelta (alta impedancia). Un MAESTRO temporiza todo; los ESCLAVOS responden.

  Trama (todo LSB primero, CRC8 Dallas/Maxim):
    Maestro -> [RESET + presencia] [DIR] [CMD] [REG] [N si bloque] [DATOS si WRITE] [CRC8]
    Esclavo -> ACK (1 bit: bajo = ACK, alto/silencio = NACK)
    Si CMD=READ:  Esclavo -> [DATOS] [CRC8(DATOS)]

  Modelo de registros: cada esclavo expone hasta 256 registros de 8 bits.
  El registro 0xFF es siempre la identificacion (tipo de dispositivo).
  Bloques: hasta 16 bytes por trama (para displays, buffers de muestras).
*/
#ifndef BUSUNHILO_H
#define BUSUNHILO_H

#include <Arduino.h>

#define BUSUNHILO_VERSION "0.1.0"

/* ================================================================== */
/*  CONFIGURACION POR PLACA  (editar ESTE archivo; el sketch no sirve) */
/*                                                                    */
/*  Placa                      Camino que usa la libreria             */
/*  -------------------------  -------------------------------------- */
/*  Uno, Nano, Pro Mini, Mega  AVR: registros directos (rapido)       */
/*  ATtiny85 (8 MHz o mas)     AVR: registros directos (rapido)       */
/*  ESP8266 / ESP32            generico (pinMode/digitalRead) o, si    */
/*                             activas BUH_ESP_REGISTROS, registros    */
/*  Otras (RP2040, STM32...)   generico (mas lento): usar MODO LENTO   */
/*                                                                    */
/*  REGLA: todas las placas del mismo bus con la MISMA configuracion   */
/*  de tiempos (todas normal, o todas en MODO LENTO).                  */
/*                                                                    */
/*  Opciones (quitar las // para activar):                             */
/*    //#define BUH_MODO_LENTO      tiempos ~3,5x mas largos (abajo)    */
/*    //#define BUH_ESP_REGISTROS   EXPERIMENTAL: ESP8266 (GPIO 0..15)  */
/*                                  o ESP32 (GPIO 0..31): acceso       */
/*                                  directo a registros GPIO           */
/* ================================================================== */
//#define BUH_ESP_REGISTROS

#if (defined(ESP8266) || defined(ESP32)) && defined(BUH_ESP_REGISTROS)
  #define BUH_USA_REG_ESP
  #if defined(ESP32)
    #include "soc/soc.h"
    #include "soc/gpio_reg.h"
  #endif
#endif

#if defined(__AVR__) && (F_CPU < 8000000UL) && !defined(BUH_MODO_LENTO) && !defined(BUH_SIM)
  #warning "BusUnHilo: con reloj menor a 8 MHz (p.ej. ATtiny85 a 1 MHz) los tiempos normales no alcanzan. Usar 8 MHz o mas."
#endif

/* ------------------------------------------------------------------ */
/*  MODO LENTO (~3,5x mas lento, mas tolerante). Util para Proteus u    */
/*  otros simuladores, cables largos o ambientes ruidosos.              */
/*  PARA ACTIVARLO: quitar las // de la linea siguiente y GUARDAR.      */
/*  Debe estar igual en el maestro y en TODOS los esclavos.             */
/*  (Un #define en el sketch NO sirve: la libreria se compila aparte.)  */
/* ------------------------------------------------------------------ */
//#define BUH_MODO_LENTO

#ifdef BUH_MODO_LENTO
  #define BUH_RESET_LOW_US          1000
  #define BUH_PRESENCE_SAMPLE_US    200
  #define BUH_RESET_REST_US         800
  #define BUH_W1_LOW_US             20
  #define BUH_W1_REST_US            230
  #define BUH_W0_LOW_US             200
  #define BUH_W0_REST_US            50
  #define BUH_R_LOW_US              10
  #define BUH_R_SAMPLE_US           30
  #define BUH_R_REST_US             210
  #define BUH_SLV_SAMPLE_US         80
  #define BUH_SLV_HOLD_US           100
  #define BUH_SLV_PRESENCE_DELAY_US 50
  #define BUH_SLV_PRESENCE_US       400
  #define BUH_SLV_MIN_RESET_US      500
  #define BUH_SLV_MAX_RESET_US      1500
  #define BUH_SLV_FIRST_TIMEOUT_US  4000
  #define BUH_SLV_BIT_TIMEOUT_US    1500
  #define BUH_SLV_BIT_END_US        500
  #define BUH_TURNAROUND_US         150
#endif

/* ------------------------------------------------------------------ */
/*  Tiempos en microsegundos (valores normales). Para cambiarlos, edita */
/*  los numeros de abajo EN ESTE ARCHIVO (no desde el sketch).          */
/* ------------------------------------------------------------------ */
#ifndef BUH_RESET_LOW_US
#define BUH_RESET_LOW_US        480   // maestro: pulso de reset
#endif
#ifndef BUH_PRESENCE_SAMPLE_US
#define BUH_PRESENCE_SAMPLE_US  70    // maestro: muestreo de presencia tras soltar
#endif
#ifndef BUH_RESET_REST_US
#define BUH_RESET_REST_US       410   // maestro: resto del slot de reset
#endif
#ifndef BUH_W1_LOW_US
#define BUH_W1_LOW_US           6     // escribir 1: bajo corto
#endif
#ifndef BUH_W1_REST_US
#define BUH_W1_REST_US          69
#endif
#ifndef BUH_W0_LOW_US
#define BUH_W0_LOW_US           60    // escribir 0: bajo largo
#endif
#ifndef BUH_W0_REST_US
#define BUH_W0_REST_US          15
#endif
#ifndef BUH_R_LOW_US
#define BUH_R_LOW_US            3     // leer: pulso corto del maestro
#endif
#ifndef BUH_R_SAMPLE_US
#define BUH_R_SAMPLE_US         9     // leer: espera antes de muestrear
#endif
#ifndef BUH_R_REST_US
#define BUH_R_REST_US           55
#endif
#ifndef BUH_SLV_SAMPLE_US
#define BUH_SLV_SAMPLE_US       25    // esclavo: muestreo de bit tras el flanco
#endif
#ifndef BUH_SLV_HOLD_US
#define BUH_SLV_HOLD_US         30    // esclavo: mantiene bajo para responder 0
#endif
#ifndef BUH_SLV_PRESENCE_DELAY_US
#define BUH_SLV_PRESENCE_DELAY_US 20
#endif
#ifndef BUH_SLV_PRESENCE_US
#define BUH_SLV_PRESENCE_US     120
#endif
#ifndef BUH_SLV_MIN_RESET_US
#define BUH_SLV_MIN_RESET_US    150   // bajo minimo (medido) para aceptar reset
#endif
#ifndef BUH_SLV_MAX_RESET_US
#define BUH_SLV_MAX_RESET_US    700
#endif
#ifndef BUH_SLV_FIRST_TIMEOUT_US
#define BUH_SLV_FIRST_TIMEOUT_US 1200 // espera del primer bit tras presencia
#endif
#ifndef BUH_SLV_BIT_TIMEOUT_US
#define BUH_SLV_BIT_TIMEOUT_US  400   // espera del flanco de cada bit
#endif
#ifndef BUH_SLV_BIT_END_US
#define BUH_SLV_BIT_END_US      150   // espera de fin de slot
#endif
#ifndef BUH_TURNAROUND_US
#define BUH_TURNAROUND_US       40    // maestro: pausa tras el CRC antes de pedir el ACK
#endif                                // (tiempo que tiene el esclavo para validar y llamar onRead)


/* ---------------- Codigos de error ---------------- */
const uint8_t BUH_OK              = 0;
const uint8_t BUH_ERR_NO_PRESENCE = 1;  // ningun esclavo respondio al reset
const uint8_t BUH_ERR_NO_ACK      = 2;  // el esclavo/direccion no confirmo
const uint8_t BUH_ERR_CRC         = 3;  // CRC invalido en la lectura
const uint8_t BUH_ERR_BUS_STUCK   = 4;  // la linea esta atascada en bajo
const uint8_t BUH_ERR_ADDR        = 5;  // direccion invalida

/* ---------------- Comandos ---------------- */
const uint8_t BUH_CMD_WRITE       = 0x01;  // escribe 1 registro
const uint8_t BUH_CMD_READ        = 0x02;  // lee 1 registro
const uint8_t BUH_CMD_PING        = 0x03;  // solo confirma presencia
const uint8_t BUH_CMD_WRITE_BLOCK = 0x04;  // escribe N bytes desde un registro
const uint8_t BUH_CMD_READ_BLOCK  = 0x05;  // lee N bytes desde un registro

const uint8_t BUH_REG_ID      = 0xFF;      // registro de identificacion
const uint8_t BUH_MAX_BLOCK   = 16;        // bytes maximos por bloque

// Tipos de dispositivo sugeridos para el registro de identificacion
const uint8_t BUH_DEV_GENERIC = 0x00;
const uint8_t BUH_DEV_SENSOR  = 0x10;
const uint8_t BUH_DEV_DISPLAY = 0x20;
const uint8_t BUH_DEV_OUTPUT  = 0x30;

#if defined(__AVR__) && !defined(BUH_SIM)
  #define BUH_SAVE_IRQ()    uint8_t _buh_sreg = SREG; noInterrupts()
  #define BUH_RESTORE_IRQ() SREG = _buh_sreg
#else
  #define BUH_SAVE_IRQ()    noInterrupts()
  #define BUH_RESTORE_IRQ() interrupts()
#endif

#if defined(BUH_SIM)
extern "C" void buh_sim_low();
extern "C" void buh_sim_release();
extern "C" bool buh_sim_read();
#endif

/* ================================================================== */
class BusUnHiloBase {
 public:
  static uint8_t crc8Update(uint8_t crc, uint8_t b);
  static uint8_t crc8(const uint8_t *data, uint8_t len, uint8_t crc = 0);

 protected:
  void pinBegin(uint8_t pin, bool pullup);
  bool waitLevel(bool level, uint16_t us);   // espera nivel, max "us" aprox.

#if defined(BUH_SIM)
  inline void busLow()     { buh_sim_low(); }
  inline void busRelease() { buh_sim_release(); }
  inline bool busRead()    { return buh_sim_read(); }
#elif defined(__AVR__)
  // PORT del pin queda en 0: DDR=1 -> baja a GND, DDR=0 -> alta impedancia.
  inline __attribute__((always_inline)) void busLow() {
    *_rPORT &= ~_mask;
    *_rDDR  |= _mask;
  }
  inline __attribute__((always_inline)) void busRelease() {
    *_rDDR &= ~_mask;
    if (_pullup) *_rPORT |= _mask;   // pull-up interna solo si se pidio
  }
  inline __attribute__((always_inline)) bool busRead() {
    return (*_rPIN & _mask) != 0;
  }
  volatile uint8_t *_rDDR, *_rPORT, *_rPIN;
  uint8_t _mask;
#elif defined(BUH_USA_REG_ESP) && defined(ESP8266)
  // ESP8266: GPIO 0..15 (no el 16). Los macros GPOC/GPES/GPEC/GPI los da el core.
  inline void busLow()     { GPOC = _bit; GPES = _bit; }   // latch en 0 + salida habilitada
  inline void busRelease() { GPEC = _bit; }                // salida deshabilitada = alta impedancia
  inline bool busRead()    { return (GPI & _bit) != 0; }
  uint32_t _bit;
#elif defined(BUH_USA_REG_ESP) && defined(ESP32)
  // ESP32: GPIO 0..31.
  inline void busLow()     { REG_WRITE(GPIO_OUT_W1TC_REG, _bit); REG_WRITE(GPIO_ENABLE_W1TS_REG, _bit); }
  inline void busRelease() { REG_WRITE(GPIO_ENABLE_W1TC_REG, _bit); }
  inline bool busRead()    { return (REG_READ(GPIO_IN_REG) & _bit) != 0; }
  uint32_t _bit;
#else
  inline void busLow()     { digitalWrite(_num, LOW); pinMode(_num, OUTPUT); }
  inline void busRelease() { pinMode(_num, _pullup ? INPUT_PULLUP : INPUT); }
  inline bool busRead()    { return digitalRead(_num) != 0; }
  uint8_t _num;
#endif
  bool _pullup;
};

/* ================================================================== */
class BusUnHiloMaster : public BusUnHiloBase {
 public:
  void begin(uint8_t pin, bool internalPullup = false);
  void setRetries(uint8_t n) { _retries = n; }

  // ---- alto nivel (direccion 1..254; 0 = difusion solo para write) ----
  bool ping(uint8_t addr);
  bool write(uint8_t addr, uint8_t reg, uint8_t data);
  bool read(uint8_t addr, uint8_t reg, uint8_t &data);
  bool writeBlock(uint8_t addr, uint8_t reg, const uint8_t *buf, uint8_t n);
  bool readBlock(uint8_t addr, uint8_t reg, uint8_t *buf, uint8_t n);
  bool identify(uint8_t addr, uint8_t &deviceType);   // lee BUH_REG_ID
  uint8_t scan(uint8_t *found, uint8_t maxFound, uint8_t first = 1, uint8_t last = 254);
  uint8_t lastError() const { return _err; }
  static const __FlashStringHelper *errorText(uint8_t code);

  // ---- bajo nivel (por si quieres armar tu propio protocolo) ----
  bool    reset();                 // true si hay presencia
  void    writeBit(bool b);
  bool    readBit();
  void    writeByte(uint8_t v);
  uint8_t readByte();

 private:
  bool transact(uint8_t addr, uint8_t cmd, uint8_t reg,
                uint8_t *buf, uint8_t n);
  uint8_t _retries;
  uint8_t _err;
};

/* ================================================================== */
class BusUnHiloSlave : public BusUnHiloBase {
 public:
  void begin(uint8_t pin, uint8_t address, bool internalPullup = false);
  void setDeviceType(uint8_t t) { _devType = t; }

  // Callbacks: el usuario implementa la logica del dispositivo.
  //  onWrite(reg, valor)  -> el maestro escribio un registro
  //  onRead(reg)          -> devuelve el valor del registro pedido
  void onWrite(void (*cb)(uint8_t reg, uint8_t val)) { _onWrite = cb; }
  void onRead(uint8_t (*cb)(uint8_t reg))            { _onRead  = cb; }

  // Llamar MUY seguido desde loop() (al menos cada ~300 us).
  // Devuelve true si atendio una trama valida.
  bool poll();

  uint8_t address() const { return _addr; }

 private:
  bool    handleFrame();
  int8_t  rxBit(uint16_t timeoutUs);
  int16_t rxByte(bool first);
  bool    txBit(bool v);
  bool    txByte(uint8_t v);

  uint8_t _addr;
  uint8_t _devType;
  uint8_t _wbuf[BUH_MAX_BLOCK];   // datos recibidos, entregados a onWrite fuera de la seccion critica
  uint8_t _wreg, _wn;
  void (*_onWrite)(uint8_t, uint8_t);
  uint8_t (*_onRead)(uint8_t);
};

#endif

# Libreria-BusUnHilo-Arduino
Librería que nos permite implementar un puerto en modo bidireccional, e incluye un bus de comunicación que no permite comunicarnos con hasta 254 dispositivos.

---

# BusUnHilo

Bus **casero de un solo hilo**, bidireccional, de 8 bits, para Arduino (AVR) y otras placas.
Un maestro y hasta 254 esclavos comparten **un cable de datos** (drenador abierto + resistencia pull-up).
Nació de los ejemplos de *https://forosdeelectronica.com/threads/arduino-cambio-de-direcci%C3%B3n-output-input-sin-romper-nada.193872/* de Forosdeelectronica (torres.electronico).

- Direcciones de esclavo (1..254) y **difusión** (dirección 0, solo escritura).
- **Registros** de 8 bits (256 por esclavo) y **bloques** de hasta 16 bytes (displays, buffers).
- **CRC8** (Dallas/Maxim), **ACK** y **reintentos** automáticos.
- **Identificación** (registro `0xFF`) y **escáner** de bus.
- En AVR usa registros directos (`DDR/PORT/PIN`) igual que el sketch original; en otras placas usa `pinMode/digitalRead`.

---

## Cableado

```
 +5V ──[4,7 kΩ]──┬────────── cable de datos ──┬──────────┬──>>>
                 │                            │          │ 
                 ┴                            ┴          ┴
          |Maestro (pin 8)|              |Esclavo 1|  |Esclavo 2|    
 GND ────────────┴─────────── GND común ──────┴──────────┴──>>>
```
GND común obligatorio. Pull-up externa de 4,7 kΩ solo en Arduino MASTER (tambien con `begin(pin, true)` se usa la interna, más débil: solo para cables cortos de prueba).
Todos los dispositivos deben trabajar al mismo nivel de tensión.

---

## Uso rápido

**Maestro**
```cpp
#include <BusUnHilo.h>
BusUnHiloMaster bus;
void setup(){ bus.begin(8); }
void loop(){
  bus.write(1, 0x01, 1);                 // dir 1, registro 0x01 = 1
  uint8_t v; if (bus.read(1, 0x02, v)) { /* usar v */ }
  delay(500);
}
```

**Esclavo**
```cpp
#include <BusUnHilo.h>
BusUnHiloSlave bus;
void alEscribir(uint8_t reg, uint8_t val){ /* ... */ }
uint8_t alLeer(uint8_t reg){ return 0; }
void setup(){ bus.begin(8, 1); bus.onWrite(alEscribir); bus.onRead(alLeer); }
void loop(){ bus.poll(); }               // llamar MUY seguido
```
Ejemplos completos en `examples/`: `Maestro_Basico`, `Esclavo_Basico`, `Maestro_Escaner`, `REDsensorMasterSlave`

---

## API

Maestro (`BusUnHiloMaster`)

| Función | Qué hace |
|---|---|
| `begin(pin, pullupInterna=false)` | Inicia el bus en ese pin |
| `ping(dir)` | ¿Está el esclavo? |
| `write(dir, reg, dato)` / `read(dir, reg, dato&)` | Un registro |
| `writeBlock(dir, reg, buf, n)` / `readBlock(dir, reg, buf, n)` | Hasta 16 bytes consecutivos |
| `identify(dir, tipo&)` | Lee el tipo de dispositivo (`BUH_DEV_SENSOR`, `_DISPLAY`, `_OUTPUT`, `_GENERIC`) |
| `scan(vector, max, desde=1, hasta=254)` | Devuelve cuántos esclavos hay y sus direcciones |
| `setRetries(n)` | Reintentos automáticos (por defecto 3) |
| `lastError()` / `errorText(cod)` | Último error: `NO_PRESENCE`, `NO_ACK`, `CRC`, `BUS_STUCK`, parámetro inválido |
| `reset() writeBit() readBit() writeByte() readByte()` | Nivel bajo, para armar tu propio protocolo |

Esclavo (`BusUnHiloSlave`): `begin(pin, dir, pullup)`, `setDeviceType(t)`, `onWrite(cb)`, `onRead(cb)`, `poll()`.

---

## Protocolo (resumen)

Todo LSB primero. Cada trama empieza con un **reset** del maestro (bajo 480 µs) y la **presencia** de los esclavos.

```
Maestro → [DIR][CMD][REG]([N] si bloque)([DATOS] si escritura)[CRC8]
Esclavo → ACK (1 bit en bajo)
Si es lectura: Esclavo → [DATOS][CRC8 de los datos]
```
Bit: el maestro baja la línea; 1 = pulso corto (6 µs), 0 = pulso largo (60 µs); en lectura el maestro hace un pulso de 3 µs y el esclavo mantiene bajo para responder 0.
Los tiempos se pueden ajustar con `#define BUH_...` (ver `src/BusUnHilo.h`).

---

## Rendimiento y límites (leer antes de usar)

- Un registro tarda **~4-5 ms** por trama (≈ 200 tramas/s); un bloque de 16 bytes ≈ 12 ms. Sirve para sensores, comandos y texto; **no** para video ni audio.
- **Un solo maestro.** No hay arbitraje.
- El esclavo debe llamar a `poll()` al menos cada ~300 µs: nada de `delay()` largos ni bloqueos.
- `onRead()` debe devolver al instante (usar valores ya medidos). `onWrite()` se llama fuera de la sección crítica.
- Durante una trama el esclavo tiene las interrupciones **deshabilitadas** unos ms: `millis()` puede atrasar un poco y un `Serial` rápido (115200) puede perder bytes en ese lapso.
- Cable corto (decenas de cm a pocos metros) y GND común. En ambientes ruidosos, bajar la velocidad (alargar tiempos) y confiar en CRC + reintentos.

---

## Estado de las pruebas

- Probado en un **simulador determinista** (`extras/simulador`): 1 maestro + 2 esclavos, 600/600 tramas sin reintentos, bloques de 16 bytes, difusión, `identify` y `scan`; CRC contra el vector conocido de 1-Wire.
- Verificado que **compila** (sintaxis) en las ramas AVR y genérica.
- **Aún no probado en hardware real.** Se agradecen reportes (placa, frecuencia de reloj, largo de cable, errores).

---

## Ideas

Red de sensores, expansor de E/S remoto, display esclavo (texto/valores), nodos de actuadores, vigías con latido (`ping`).
Próximos pasos posibles: modo con interrupción para el esclavo, mayor velocidad, ejemplo de display.

---

## Desarrollado por:

 - Autor: ETI PATAGONIA - prof.martintorres@educ.ar
 - Web: https://eti-patagonia-ar.github.io/ETI-Patagonia-ARG/index.html
 - Repositorios: https://github.com/ETI-PATAGONIA-AR

---


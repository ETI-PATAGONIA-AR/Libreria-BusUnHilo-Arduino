# Libreria-BusUnHilo-Arduino
Librería que nos permite implementar un puerto en modo bidireccional, e incluye un bus de comunicación que no permite comunicarnos con hasta 254 dispositivos.

*Índice (Language index)*
- 🇦🇷 Español
- 🇬🇧 English
- 🇮🇹 Italiano
- 🇫🇷 Français
- 🇷🇺 Русский

---
[![Demo1 en YouTube](https://img.shields.io/badge/▶_Ver_demo-YouTube-red)](https://www.youtube.com/watch?v=IaVxYmmLfTs)

# BusUnHilo (🇦🇷 Español)

Bus **casero de un solo hilo**, bidireccional, de 8 bits, para Arduino (AVR) y otras placas.
Un maestro y hasta 254 esclavos comparten **un cable de datos** (drenador abierto + resistencia pull-up).
Nació de los ejemplos de *https://forosdeelectronica.com/threads/arduino-cambio-de-direcci%C3%B3n-output-input-sin-romper-nada.193872/* de Forosdeelectronica (torres.electronico).

- Direcciones de esclavo (1..254) y **difusión** (dirección 0, solo escritura).
- **Registros** de 8 bits (256 por esclavo) y **bloques** de hasta 16 bytes (displays, buffers).
- **CRC8** (Dallas/Maxim), **ACK** y reintentos automáticos.
- **Identificación** (registro `0xFF`) y **escáner** de bus.
- En AVR usa registros directos (`DDR/PORT/PIN`) igual que el sketch original; en otras placas usa `pinMode/digitalRead`.

## Cableado

```text
 +5V ──[4,7 kΩ]──┬────────── cable de datos ──┬──────────┬──>>>
                  │                            │          │ 
                  ┴                            ┴          ┴
           |Maestro (pin 8)|              |Esclavo 1|  |Esclavo 2|    
 GND ────────────┴─────────── GND común ──────┴──────────┴──>>>
```
GND común obligatorio. Pull-up externa de 4,7 kΩ solo en Arduino MASTER (también con `begin(pin, true)` se usa la interna, más débil: solo para cables cortos de prueba).
Todos los dispositivos deben trabajar al mismo nivel de tensión.

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
Ejemplos completos en `examples/`: `Maestro_Basico`, `Esclavo_Basico`, `Maestro_Escaner`, `REDsensorMasterSlave`, `PorteroELECTRICO`.

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
| `reset()` `writeBit()` `readBit()` `writeByte()` `readByte()` | Nivel bajo, para armar tu propio protocolo |

Esclavo (`BusUnHiloSlave`): `begin(pin, dir, pullup)`, `setDeviceType(t)`, `onWrite(cb)`, `onRead(cb)`, `poll()`.

## Protocolo (resumen)

Todo LSB primero. Cada trama empieza con un **reset** del maestro (bajo 480 µs) y la **presencia** de los esclavos.

```text
Maestro → [DIR][CMD][REG]([N] si bloque)([DATOS] si escritura)[CRC8]
Esclavo → ACK (1 bit en bajo)
Si es lectura: Esclavo → [DATOS][CRC8 de los datos]
```
Bit: el maestro baja la línea; 1 = pulso corto (6 µs), 0 = pulso largo (60 µs); en lectura el maestro hace un pulso de 3 µs y el esclavo mantiene bajo para responder 0.
Los tiempos se pueden ajustar con `#define BUH_...` (ver `src/BusUnHilo.h`).

## Rendimiento y límites (leer antes de usar)

- Un registro tarda **~4-5 ms** por trama (≈ 200 tramas/s); un bloque de 16 bytes ≈ 12 ms. Sirve para sensores, comandos y texto; **no** para video ni audio.
- **Un solo maestro.** No hay arbitraje.
- El esclavo debe llamar a `poll()` al menos cada ~300 µs: nada de `delay()` largos ni bloqueos.
- `onRead()` debe devolver al instante (usar valores ya medidos). `onWrite()` se llama fuera de la sección crítica.
- Durante una trama el esclavo tiene las interrupciones **deshabilitadas** unos ms: `millis()` puede atrasar un poco y un `Serial` rápido (115200) puede perder bytes en ese lapso.
- Cable corto (decenas de cm a pocos metros) y GND común. En ambientes ruidosos, bajar la velocidad (alargar tiempos) y confiar en CRC + reintentos.

## Estado de las pruebas

- Probado en un **simulador determinista** (`extras/simulador`): 1 maestro + 2 esclavos, 600/600 tramas sin reintentos, bloques de 16 bytes, difusión, `identify` y `scan`; CRC contra el vector conocido.
- Verificado que **compila** (sintaxis) en las ramas AVR y genérica.
- **Aún no probado en hardware real.** Se agradecen reportes (placa, frecuencia de reloj, largo de cable, errores).

## Ideas

Red de sensores, expansor de E/S remoto, display esclavo (texto/valores), nodos de actuadores, vigías con latido (`ping`).
Próximos pasos posibles: modo con interrupción para el esclavo, mayor velocidad, ejemplo de display.

## Desarrollado por:

- Autor: ETI PATAGONIA - prof.martintorres@educ.ar
- Web: https://eti-patagonia-ar.github.io/ETI-Patagonia-ARG/index.html
- Repositorios: https://github.com/ETI-PATAGONIA-AR

---

# BusUnHilo (🇬🇧 English)

A homemade, bidirectional, 8-bit, single-wire bus for Arduino (AVR) and other boards. One master and up to 254 slaves share one data wire (open-drain line + pull-up resistor).
This project originated from examples shared on Forosdeelectronica (torres.electronico).

## Features

Slave addresses (1..254) and broadcast (address 0, write-only).
8-bit registers (256 per slave) and blocks of up to 16 bytes (displays, buffers).
CRC8 (Dallas/Maxim), ACK, and automatic retries.
Device identification (register `0xFF`) and bus scanner.
On AVR, it uses direct registers (`DDR/PORT/PIN`), as in the original sketch; on other boards, it uses `pinMode/digitalRead`.

## Wiring

```text
 +5V ──[4.7 kΩ]──┬────────── data wire ───────┬──────────┬──>>>
                  │                            │          │
                  ┴                            ┴          ┴
            |Master (pin 8)|              |Slave 1|   |Slave 2|
 GND ─────────────┴────────── common GND ─────┴──────────┴──>>>
```
A common GND connection is mandatory. Use an external 4.7 kΩ pull-up only at the Arduino MASTER (with `begin(pin, true)`, the weaker internal pull-up is also enabled; use it only for short-wire testing). All devices must operate at the same voltage level.

## Quick start

Master
```cpp
#include <BusUnHilo.h>
BusUnHiloMaster bus;
void setup(){ bus.begin(8); }
void loop(){
  bus.write(1, 0x01, 1);                 // address 1, register 0x01 = 1
  uint8_t v; if (bus.read(1, 0x02, v)) { /* use v */ }
  delay(500);
}
```
Slave
```cpp
#include <BusUnHilo.h>
BusUnHiloSlave bus;
void alEscribir(uint8_t reg, uint8_t val){ /* ... */ }
uint8_t alLeer(uint8_t reg){ return 0; }
void setup(){ bus.begin(8, 1); bus.onWrite(alEscribir); bus.onRead(alLeer); }
void loop(){ bus.poll(); }               // call VERY frequently
```
Complete examples are available in `examples/`: `Maestro_Basico`, `Esclavo_Basico`, `Maestro_Escaner`, `REDsensorMasterSlave`.

## API

| Function | Description |
|---|---|
| `begin(pin, pullupInterna=false)` | Starts the bus on the selected pin |
| `ping(dir)` | Checks whether the slave is present |
| `write(dir, reg, dato)` / `read(dir, reg, dato&)` | Accesses one register |
| `writeBlock(dir, reg, buf, n)` / `readBlock(dir, reg, buf, n)` | Transfers up to 16 consecutive bytes |
| `identify(dir, tipo&)` | Reads the device type (`BUH_DEV_SENSOR`, `_DISPLAY`, `_OUTPUT`, `_GENERIC`) |
| `scan(vector, max, desde=1, hasta=254)` | Returns the number of slaves found and their addresses |
| `setRetries(n)` | Sets automatic retries (default: 3) |
| `lastError()` / `errorText(cod)` | Last error: `NO_PRESENCE`, `NO_ACK`, `CRC`, `BUS_STUCK`, invalid parameter |
| `reset()` `writeBit()` `readBit()` `writeByte()` `readByte()` | Low-level access for implementing your own protocol |

Slave (`BusUnHiloSlave`): `begin(pin, dir, pullup)`, `setDeviceType(t)`, `onWrite(cb)`, `onRead(cb)`, `poll()`.

## Protocol (summary)

All data is transmitted least significant bit first (LSB first). Each frame begins with a master reset (line held low for 480 µs), followed by the slaves' presence pulse.
```text
Master → [DIR][CMD][REG]([N] for a block)([DATA] for a write)[CRC8]
Slave  → ACK (1 bit held low)
For reads: Slave → [DATA][data CRC8]
```
For each bit, the master pulls the line low: 1 = short pulse (6 µs), 0 = long pulse (60 µs). During a read, the master generates a 3 µs pulse and the slave holds the line low to send a 0.
Timing values can be adjusted with `#define BUH_...` (see `src/BusUnHilo.h`).

## Performance and limitations (read before use)

One register takes about 4–5 ms per frame (≈ 200 frames/s); a 16-byte block takes about 12 ms. Suitable for sensors, commands, and text; not for video or audio.
One master only. There is no bus arbitration.
The slave must call `poll()` at least every ~300 µs. Avoid long `delay()` calls and blocking operations.
`onRead()` must return immediately (use values that have already been measured). `onWrite()` is called outside the critical section.
During a frame, interrupts on the slave are disabled for a few milliseconds: `millis()` may be delayed slightly, and fast `Serial` communication (115200) may lose bytes during that interval.
Use a short cable (tens of centimetres to a few metres) and common GND. In electrically noisy environments, reduce the speed (increase timing intervals) and rely on CRC + retries.

## Test status

Tested with a deterministic simulator (`extras/simulador`): 1 master + 2 slaves, 600/600 frames without retries, 16-byte blocks, broadcast, `identify`, and `scan`; CRC checked against the known 1-Wire test vector.
Compilation (syntax) verified for both AVR and generic code paths.
Not yet tested on real hardware. Reports are welcome (board, clock frequency, cable length, errors).

## Ideas

Sensor networks, remote I/O expanders, slave displays (text/values), actuator nodes, and heartbeat monitors (`ping`).
Possible next steps: interrupt-driven slave mode, higher speed, and a display example.

## Developed by

Author: ETI PATAGONIA - prof.martintorres@educ.ar
Website: https://eti-patagonia-ar.github.io/ETI-Patagonia-ARG/index.html
Repositories: https://github.com/ETI-PATAGONIA-AR

---

# BusUnHilo (🇮🇹 Italiano)

Bus artigianale a filo singolo, bidirezionale e a 8 bit, per Arduino (AVR) e altre schede. Un master e fino a 254 slave condividono un unico filo dati (linea open-drain + resistenza di pull-up).
Il progetto nasce dagli esempi pubblicati su Forosdeelectronica (torres.electronico).

## Caratteristiche

Indirizzi degli slave (1..254) e broadcast (indirizzo 0, solo scrittura).
Registri a 8 bit (256 per slave) e blocchi fino a 16 byte (display, buffer).
CRC8 (Dallas/Maxim), ACK e nuovi tentativi automatici.
Identificazione del dispositivo (registro `0xFF`) e scanner del bus.
Su AVR utilizza i registri diretti (`DDR/PORT/PIN`), come nello sketch originale; sulle altre schede utilizza `pinMode/digitalRead`.

## Cablaggio

```text
 +5V ──[4,7 kΩ]──┬────────── filo dati ───────┬──────────┬──>>>
                  │                            │          │
                  ┴                            ┴          ┴
             |Master (pin 8)|              |Slave 1|   |Slave 2|
 GND ─────────────┴────────── GND comune ──────┴──────────┴──>>>
```
Il collegamento GND comune è obbligatorio. La resistenza pull-up esterna da 4,7 kΩ va collegata solo all'Arduino MASTER (con `begin(pin, true)` viene attivata anche la pull-up interna, più debole: usala solo per prove con cavi corti). Tutti i dispositivi devono funzionare allo stesso livello di tensione.

## Avvio rapido

Master
```cpp
#include <BusUnHilo.h>
BusUnHiloMaster bus;
void setup(){ bus.begin(8); }
void loop(){
  bus.write(1, 0x01, 1);                 // indirizzo 1, registro 0x01 = 1
  uint8_t v; if (bus.read(1, 0x02, v)) { /* usare v */ }
  delay(500);
}
```
Slave
```cpp
#include <BusUnHilo.h>
BusUnHiloSlave bus;
void alEscrivere(uint8_t reg, uint8_t val){ /* ... */ }
uint8_t alLeggere(uint8_t reg){ return 0; }
void setup(){ bus.begin(8, 1); bus.onWrite(alEscrivere); bus.onRead(alLeggere); }
void loop(){ bus.poll(); }               // chiamare MOLTO frequentemente
```
Gli esempi completi si trovano in `examples/`: `Maestro_Basico`, `Esclavo_Basico`, `Maestro_Escaner`, `REDsensorMasterSlave`.

## API

| Funzione | Descrizione |
|---|---|
| `begin(pin, pullupInterna=false)` | Avvia il bus sul pin selezionato |
| `ping(dir)` | Verifica se lo slave è presente |
| `write(dir, reg, dato)` / `read(dir, reg, dato&)` | Accesso a un registro |
| `writeBlock(dir, reg, buf, n)` / `readBlock(dir, reg, buf, n)` | Trasferisce fino a 16 byte consecutivi |
| `identify(dir, tipo&)` | Legge il tipo di dispositivo (`BUH_DEV_SENSOR`, `_DISPLAY`, `_OUTPUT`, `_GENERIC`) |
| `scan(vector, max, desde=1, hasta=254)` | Restituisce il numero di slave trovati e i relativi indirizzi |
| `setRetries(n)` | Imposta i tentativi automatici (predefinito: 3) |
| `lastError()` / `errorText(cod)` | Ultimo errore: `NO_PRESENCE`, `NO_ACK`, `CRC`, `BUS_STUCK`, parametro non valido |
| `reset()` `writeBit()` `readBit()` `writeByte()` `readByte()` | Accesso a basso livello per creare un protocollo personalizzato |

Slave (`BusUnHiloSlave`): `begin(pin, dir, pullup)`, `setDeviceType(t)`, `onWrite(cb)`, `onRead(cb)`, `poll()`.

## Protocollo (riepilogo)

Tutti i dati vengono trasmessi prima il bit meno significativo (LSB first). Ogni trama inizia con un reset del master (linea mantenuta bassa per 480 µs), seguito dall'impulso di presenza degli slave.
```text
Master → [DIR][CMD][REG]([N] per un blocco)([DATI] per una scrittura)[CRC8]
Slave  → ACK (1 bit mantenuto basso)
In lettura: Slave → [DATI][CRC8 dei dati]
```
Per ogni bit, il master porta la linea a livello basso: 1 = impulso breve (6 µs), 0 = impulso lungo (60 µs). Durante una lettura, il master genera un impulso di 3 µs e lo slave mantiene la linea bassa per trasmettere 0.
I tempi possono essere modificati con `#define BUH_...` (vedere `src/BusUnHilo.h`).

## Prestazioni e limiti (leggere prima dell'uso)

Un registro richiede circa 4–5 ms per trama (≈ 200 frame/s); un blocco da 16 byte richiede circa 12 ms. Adatto a sensori, comandi e testo; non a video o audio.
Un solo master. Non è previsto alcun arbitraggio del bus.
Lo slave deve chiamare `poll()` almeno ogni ~300 µs: evitare `delay()` lunghi e operazioni bloccanti.
`onRead()` deve restituire immediatamente (utilizzare valori già acquisiti). `onWrite()` viene chiamata al di fuori della sezione critica.
Durante una trama, gli interrupt dello slave sono disabilitati per alcuni millisecondi: `millis()` può subire un lieve ritardo e una comunicazione `Serial` veloce (115200) può perdere byte in quell'intervallo.
Utilizzare un cavo corto (da alcune decine di centimetri a pochi metri) e GND comune. In ambienti rumorosi, ridurre la velocità (allungare i tempi) e affidarsi a CRC + tentativi automatici.

## Stato dei test

Testato con un simulatore deterministico (`extras/simulador`): 1 master + 2 slave, 600/600 frame senza nuovi tentativi, blocchi da 16 byte, broadcast, `identify` e `scan`; CRC confrontato con il vettore di test noto.
Compilazione (sintassi) verificata per i percorsi AVR e generico.
Non ancora testato su hardware reale. Sono gradite segnalazioni (scheda, frequenza di clock, lunghezza del cavo, errori).

## Idee

Reti di sensori, espansori I/O remoti, display slave (testo/valori), nodi per attuatori e dispositivi di monitoraggio con heartbeat (`ping`).
Possibili sviluppi: modalità slave con interrupt, maggiore velocità ed esempio per display.

## Sviluppato da

Autore: ETI PATAGONIA - prof.martintorres@educ.ar
Sito web: https://eti-patagonia-ar.github.io/ETI-Patagonia-ARG/index.html
Repository: https://github.com/ETI-PATAGONIA-AR

---

# BusUnHilo (🇫🇷 Français)

Bus artisanal à un seul fil, bidirectionnel et de 8 bits, pour Arduino (AVR) et d'autres cartes. Un maître et jusqu'à 254 esclaves partagent un seul fil de données (ligne à drain ouvert + résistance de pull-up).
Ce projet est issu d'exemples publiés sur Forosdeelectronica (torres.electronico).

## Caractéristiques

Adresses des esclaves (1..254) et diffusion (adresse 0, écriture uniquement).
Registres de 8 bits (256 par esclave) et blocs jusqu'à 16 octets (afficheurs, tampons).
CRC8 (Dallas/Maxim), ACK et nouvelles tentatives automatiques.
Identification du périphérique (registre `0xFF`) et scanner du bus.
Sur AVR, utilise les registres directs (`DDR/PORT/PIN`), comme le sketch d'origine ; sur les autres cartes, utilise `pinMode/digitalRead`.

## Câblage

```text
 +5V ──[4,7 kΩ]──┬────────── fil de données ──┬──────────┬──>>>
                  │                            │          │
                  ┴                            ┴          ┴
            |Maître (broche 8)|            |Esclave 1| |Esclave 2|
 GND ─────────────┴────────── GND commun ─────┴──────────┴──>>>
```
La masse (GND) commune est obligatoire. La résistance pull-up externe de 4,7 kΩ doit être installée uniquement sur l'Arduino MASTER (avec `begin(pin, true)`, la résistance pull-up interne, plus faible, est également activée ; utilisez-la uniquement pour des tests sur câbles courts). Tous les appareils doivent fonctionner au même niveau de tension.

## Démarrage rapide

Maître
```cpp
#include <BusUnHilo.h>
BusUnHiloMaster bus;
void setup(){ bus.begin(8); }
void loop(){
  bus.write(1, 0x01, 1);                 // adresse 1, registre 0x01 = 1
  uint8_t v; if (bus.read(1, 0x02, v)) { /* utiliser v */ }
  delay(500);
}
```
Esclave
```cpp
#include <BusUnHilo.h>
BusUnHiloSlave bus;
void alEcrire(uint8_t reg, uint8_t val){ /* ... */ }
uint8_t alLire(uint8_t reg){ return 0; }
void setup(){ bus.begin(8, 1); bus.onWrite(alEcrire); bus.onRead(alLire); }
void loop(){ bus.poll(); }               // appeler TRÈS fréquemment
```
Les exemples complets se trouvent dans `examples/`: `Maestro_Basico`, `Esclavo_Basico`, `Maestro_Escaner`, `REDsensorMasterSlave`.

## API

| Fonction | Description |
|---|---|
| `begin(pin, pullupInterna=false)` | Démarre le bus sur la broche choisie |
| `ping(dir)` | Vérifie la présence de l'esclave |
| `write(dir, reg, dato)` / `read(dir, reg, dato&)` | Accède à un registre |
| `writeBlock(dir, reg, buf, n)` / `readBlock(dir, reg, buf, n)` | Transfère jusqu'à 16 octets consécutifs |
| `identify(dir, tipo&)` | Lit le type de périphérique (`BUH_DEV_SENSOR`, `_DISPLAY`, `_OUTPUT`, `_GENERIC`) |
| `scan(vector, max, desde=1, hasta=254)` | Renvoie le nombre d'esclaves détectés et leurs adresses |
| `setRetries(n)` | Définit le nombre de tentatives automatiques (3 par défaut) |
| `lastError()` / `errorText(cod)` | Dernière erreur : `NO_PRESENCE`, `NO_ACK`, `CRC`, `BUS_STUCK`, paramètre invalide |
| `reset()` `writeBit()` `readBit()` `writeByte()` `readByte()` | Accès bas niveau pour créer son propre protocole |

Esclave (`BusUnHiloSlave`) : `begin(pin, dir, pullup)`, `setDeviceType(t)`, `onWrite(cb)`, `onRead(cb)`, `poll()`.

## Protocole (résumé)

Toutes les données sont transmises du bit de poids faible au bit de poids fort (LSB first). Chaque trame commence par un reset du maître (ligne maintenue à l'état bas pendant 480 µs), suivi de l'impulsion de présence des esclaves.

```text
Maître → [DIR][CMD][REG]([N] pour un bloc)([DONNÉES] pour une écriture)[CRC8]
Esclave → ACK (1 bit maintenu à l'état bas)
En lecture : Esclave → [DONNÉES][CRC8 des données]
```
Pour chaque bit, le maître met la ligne à l'état bas : 1 = impulsion courte (6 µs), 0 = impulsion longue (60 µs). En lecture, le maître génère une impulsion de 3 µs et l'esclave maintient la ligne basse pour transmettre un 0.
Les temporisations peuvent être ajustées avec `#define BUH_...` (voir `src/BusUnHilo.h`).

## Performances et limites (à lire avant utilisation)

Un registre nécessite environ 4 à 5 ms par trame (≈ 200 trames/s) ; un bloc de 16 octets nécessite environ 12 ms. Convient aux capteurs, commandes et textes ; pas à la vidéo ni à l'audio.
Un seul maître. Il n'y a pas d'arbitrage du bus.
L'esclave doit appeler `poll()` au moins toutes les ~300 µs : éviter les longs `delay()` et les opérations bloquantes.
`onRead()` doit répondre immédiatement (utiliser des valeurs déjà mesurées). `onWrite()` est appelée en dehors de la section critique.
Pendant une trame, les interruptions de l'esclave sont désactivées pendant quelques millisecondes : `millis()` peut prendre un léger retard et une communication `Serial` rapide (115200) peut perdre des octets pendant cet intervalle.
Utiliser un câble court (de quelques dizaines de centimètres à quelques mètres) et une masse commune. Dans les environnements bruyants, réduire la vitesse (allonger les temporisations) et s'appuyer sur le CRC + nouvelles tentatives.

## État des tests

Testé avec un simulateur déterministe (`extras/simulador`) : 1 maître + 2 esclaves, 600/600 trames sans nouvelle tentative, blocs de 16 octets, diffusion, `identify` et `scan` ; CRC comparé au vecteur de test connu.
Compilation (syntaxe) vérifiée pour les branches AVR et générique.
Pas encore testé sur du matériel réel. Les retours sont les bienvenus (carte, fréquence d'horloge, longueur du câble, erreurs).

## Idées

Réseaux de capteurs, extension d'E/S déportée, afficheur esclave (textes/valeurs), nœuds d'actionneurs et dispositifs de surveillance avec signal de vie (`ping`).
Évolutions possibles : mode esclave avec interruptions, vitesse supérieure et exemple d'afficheur.

## Développé par

Auteur : ETI PATAGONIA - prof.martintorres@educ.ar
Site web : https://eti-patagonia-ar.github.io/ETI-Patagonia-ARG/index.html
Dépôts : https://github.com/ETI-PATAGONIA-AR

---

# BusUnHilo (🇷🇺 Русский)

Самодельная двунаправленная 8-битная однопроводная шина для Arduino (AVR) и других плат. Один ведущий контроллер и до 254 ведомых устройств разделяют один провод данных (линия с открытым стоком + подтягивающий резистор).
Проект создан на основе примеров, опубликованных на Forosdeelectronica (torres.electronico).

## Возможности

Адреса ведомых устройств (1..254) и широковещательная передача (адрес 0, только запись).
8-битные регистры (256 на каждое ведомое устройство) и блоки до 16 байт (дисплеи, буферы).
CRC8 (Dallas/Maxim), ACK и автоматические повторные попытки.
Идентификация устройства (регистр `0xFF`) и сканирование шины.
На AVR используются прямые регистры (`DDR/PORT/PIN`), как и в исходном скетче; на других платах используются `pinMode/digitalRead`.

## Подключение

```text
 +5V ──[4,7 кΩ]──┬────────── провод данных ──┬──────────┬──>>>
                  │                            │          │
                  ┴                            ┴          ┴
           |Master (pin 8)|              |Slave 1|   |Slave 2|
 GND ─────────────┴────────── общий GND ──────┴──────────┴──>>>
```
Общий GND обязателен. Внешний подтягивающий резистор 4,7 кОм устанавливается только на Arduino MASTER (при вызове `begin(pin, true)` также используется внутренний подтягивающий резистор, он слабее и подходит только для коротких тестовых кабелей). Все устройства должны работать на одном уровне напряжения.

## Быстрый запуск

Ведущее устройство (Master)
```cpp
#include <BusUnHilo.h>
BusUnHiloMaster bus;
void setup(){ bus.begin(8); }
void loop(){
  bus.write(1, 0x01, 1);                 // адрес 1, регистр 0x01 = 1
  uint8_t v; if (bus.read(1, 0x02, v)) { /* использовать v */ }
  delay(500);
}
```
Ведомое устройство (Slave)
```cpp
#include <BusUnHilo.h>
BusUnHiloSlave bus;
void alEscribir(uint8_t reg, uint8_t val){ /* ... */ }
uint8_t alLeer(uint8_t reg){ return 0; }
void setup(){ bus.begin(8, 1); bus.onWrite(alEscribir); bus.onRead(alLeer); }
void loop(){ bus.poll(); }               // вызывать ОЧЕНЬ часто
```
Полные примеры находятся в `examples/`: `Maestro_Basico`, `Esclavo_Basico`, `Maestro_Escaner`, `REDsensorMasterSlave`.

## API

| Функция | Описание |
|---|---|
| `begin(pin, pullupInterna=false)` | Запускает шину на указанном выводе |
| `ping(dir)` | Проверяет наличие ведомого устройства |
| `write(dir, reg, dato)` / `read(dir, reg, dato&)` | Доступ к одному регистру |
| `writeBlock(dir, reg, buf, n)` / `readBlock(dir, reg, buf, n)` | Передача до 16 последовательных байт |
| `identify(dir, tipo&)` | Читает тип устройства (`BUH_DEV_SENSOR`, `_DISPLAY`, `_OUTPUT`, `_GENERIC`) |
| `scan(vector, max, desde=1, hasta=254)` | Возвращает количество найденных ведомых устройств и их адреса |
| `setRetries(n)` | Устанавливает число автоматических повторов (по умолчанию: 3) |
| `lastError()` / `errorText(cod)` | Последняя ошибка: `NO_PRESENCE`, `NO_ACK`, `CRC`, `BUS_STUCK`, неверный параметр |
| `reset()` `writeBit()` `readBit()` `writeByte()` `readByte()` | Низкоуровневый доступ для создания собственного протокола |

Ведомое устройство (`BusUnHiloSlave`): `begin(pin, dir, pullup)`, `setDeviceType(t)`, `onWrite(cb)`, `onRead(cb)`, `poll()`.

## Протокол (краткое описание)

Все данные передаются начиная с младшего значащего бита (LSB first). Каждый кадр начинается с сброса со стороны мастера (линия удерживается в низком состоянии 480 µs), после чего следует импульс присутствия ведомых устройств.

```text
Master → [DIR][CMD][REG]([N] для блока)([DATA] при записи)[CRC8]
Slave  → ACK (1 бит — низкий уровень)
При чтении: Slave → [DATA][CRC8 данных]
```
Для передачи каждого бита мастер переводит линию в низкий уровень: 1 = короткий импульс (6 µs), 0 = длинный импульс (60 µs). Во время чтения мастер генерирует импульс 3 µs, а ведомое устройство удерживает линию низкой, чтобы передать 0.
Временные параметры можно изменять с помощью `#define BUH_...` (см. `src/BusUnHilo.h`).

## Производительность и ограничения (прочитайте перед использованием)

Передача одного регистра занимает примерно 4–5 мс на кадр (≈ 200 кадров/с); блок из 16 байт — около 12 мс. Подходит для датчиков, команд и текста; не подходит для видео или аудио.
Только одно ведущее устройство. Арбитраж шины не предусмотрен.
Ведомое устройство должно вызывать `poll()` не реже чем примерно каждые 300 µs. Не используйте длительные `delay()` и блокирующие операции.
`onRead()` должна возвращать данные немедленно (используйте уже измеренные значения). `onWrite()` вызывается вне критической секции.
Во время передачи кадра прерывания на ведомом устройстве отключаются на несколько миллисекунд: `millis()` может слегка задержаться, а быстрая `Serial` (115200) может терять байты в этот промежуток.
Используйте короткий кабель (от нескольких десятков сантиметров до нескольких метров) и общий GND. В шумных условиях уменьшайте скорость (увеличивайте временные интервалы) и полагайтесь на CRC + повторные попытки.

## Результаты тестирования

Проверено на детерминированном симуляторе (`extras/simulador`): 1 master + 2 slave, 600/600 кадров без повторных попыток, блоки 16 байт, broadcast, `identify` и `scan`; CRC сверяется с известным тестовым набором.
Проверена компиляция (синтаксис) для AVR и универсальной ветви кода.
На реальном оборудовании пока не тестировалось. Приветствуются отчёты (плата, тактовая частота, длина кабеля, ошибки).

## Идеи

Сеть датчиков, удалённый модуль расширения ввода-вывода, ведомый дисплей (текст/значения), узлы исполнителей и устройства мониторинга с heartbeat (`ping`).
Возможные дальнейшие шаги: режим ведомого устройства с использованием прерываний, увеличение скорости и пример дисплея.

## Разработчик

Автор: ETI PATAGONIA - prof.martintorres@educ.ar
Сайт: https://eti-patagonia-ar.github.io/ETI-Patagonia-ARG/index.html
Репозитории: https://github.com/ETI-PATAGONIA-AR

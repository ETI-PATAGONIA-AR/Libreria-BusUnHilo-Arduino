En esta sección les iré comentando las ideas que estoy trabajando para futuras versiones...

# Sketch HERRAMIENTAS / TOOLS

La primer herramienta que sumamos dentro de la carpeta de ejemplos en las primeras versiones de la libreria es el scanner del BUS, que tiene como funcion poder ver que dispositivos hay conectados...
Ahora, dos situaciones reales que pueden pasar:
- Que pasaria si el conductor que estamos empleando para nuestro BUS es de mala calidad, no es el adecuado o es muy extenso? Bien, de ahi nace la idea de hacer otro sketch Tools para poder diagnosticar nuestro conductor del BUS, al cual lo bautice como "BusMETER".
- Que pasaria si en un proceso el MASTER se cae y los esclavos quedan a la espera de instrucciones? Bien, en una red mesh la comunicacion ante la caida de un nodo, busca cerrar el circuito del flujo de comunicacion mediante otro nodo... En este caso en particular, si bien no es lo mismo, el concepto basico de intentar seguir manteniendo en funcionamiento el proceso mediante el empleo de otro dispositivo, es la idea principal...

Hagan de cuenta que estoy pensando en voz alta y voy comentando lo que estoy trabajando. Si quieren aportar ideas, corregir algo que ven mal, pueden hacerlo via correo electronico (prof.martintorres@educ.ar), o bien, son mas que bienvenidos para sumarse como colaboradores/desarrolladores de la libreria.

## BusMETER — diseño

### 1.1 Objetivo y alcance

- Determinar si el esclavo responde **y** en qué condiciones eléctricas trabaja el bus.
- Medir margen antes de perder confiabilidad, por perfil de tiempos (normal/lento) y por longitud/configuración de cable.
- Que sea reproducible: semilla, patrón, configuración y resultados.
- No prometer “impedancia” ni “calidad de cable” absoluta; entregar métricas y un veredicto conservador.

### 1.2 Nivel 1 — PING/PONG y estadísticas

**Estado actual:** `BUH_CMD_PING` solo confirma presencia; no devuelve payload. Para un PING con nonce en una sola trama hace falta un comando nuevo. Para v1 se puede hacer **loopback por registros**:

1. `writeBlock(dir, banco, tx, n)`.
2. `readBlock(dir, banco, rx, n)`.
3. Comparar `rx` con `tx`.

Es una prueba de dos transacciones, no un eco de una sola trama. Sirve para medir comunicación y integridad, pero el RTT incluye dos resets.

**Propuesta para v2 (comando nuevo, no en v1):**

```text
PING_NONCE:
  Maestro → [DIR][CMD][N][NONCE(4)][CRC8]
  Esclavo → [NONCE invertido(4)][CRC8]
```

- El nonce demuestra que el esclavo procesó el paquete actual, no una respuesta cacheada.
- La secuencia detecta respuestas viejas; el ID/tipo de dispositivo confirma identidad.
- `CMD` nuevo, por ejemplo `0x06`; los esclavos viejos lo rechazan por rango y no responden (el maestro lo detecta como `NO_ACK`).
- Hay que definir el largo del payload y el CRC; no reutilizar `n` de bloque sin documentarlo.

**Métricas a registrar:**

- Intentos, éxitos, `NO_PRESENCE`, `NO_ACK`, `CRC`, `BUS_STUCK`, respuestas incorrectas (`nonce`, secuencia, ID), reintentos.
- RTT: tiempo total de la transacción, medido por el maestro.
- Jitter: min/mediana/max/desvío del RTT.
- **Turnaround**: tiempo desde el último bit del request hasta el primer flanco de respuesta (ACK). Es medible con código de bajo nivel; no confundir con “tiempo de procesamiento del esclavo”.
- **Tiempo de procesamiento del esclavo**: no se puede separar del turnaround sin que el esclavo incluya su propio timestamp. No afirmarlo si no se mide.
- Tasa de tramas fallidas (FER), no “BER” de bytes, salvo que se haga un test por byte.

**Diseño experimental:**

- `setRetries(0)` para no ocultar fallos.
- Patrones fijos `0x00`, `0xFF`, `0x55`, `0xAA`, walking-1, más PRNG con semilla imprimible.
- Gap configurable entre escritura y lectura (0..500 µs) para medir el mínimo confiable.
- Semilla y configuración impresas al inicio de cada corrida.

### 1.3 Nivel 2 — Diagnóstico eléctrico

#### A. Tiempo de subida y capacitancia equivalente

La línea parte de ~0 V y carga a VCC por la pull-up:

```text
V(t) = VCC · (1 - e^(-t/(R·C)))

C = -t / (R · ln(1 - V(t)/VCC))
```

Si se mide el tiempo hasta el **63,2 % de VCC**:

```text
ln(1 - 0,632) = -1  →  C = t / R
```

Ejemplo: `t = 10 µs`, `R = 4700 Ω` → `C = 10e-6 / 4700 = 2,13 nF`.

**Corrección importante:** una entrada digital **no** mide el 63,2 % de VCC. Mide el cruce de su umbral de entrada, que no está especificado con precisión (en AVR, `VIH` mínimo ≈ 0,6 VCC, pero el punto real varía). Si se usa entrada digital, el resultado es “tiempo hasta el umbral digital”, **no** capacitancia exacta. Para usar `C = t/R` hace falta un umbral conocido: comparador con referencia, o una calibración contra osciloscopio.

#### B. Tensión LOW y corriente

Cuando un dispositivo hunde la línea:

```text
I_LOW = (VCC - V_LOW) / R_pullup
```

Ejemplo: `VCC = 5 V`, `R = 4700 Ω`, `V_LOW = 0,25 V`:

```text
I_LOW = (5 - 0,25) / 4700 = 1,01 mA
```

La fórmula del borrador `ILOW = VCC - ILOW/Rpullup` es dimensionalmente incorrecta y se autocontiene; la válida es la de arriba.

- `I_LOW` es la corriente por la pull-up, no necesariamente toda la corriente de hundimiento del esclavo (puede haber fugas).
- Una `V_LOW` alta puede indicar driver débil, GND deficiente, exceso de corriente o línea mal referenciada.
- Medir `V_LOW` requiere ADC en el pin del bus (o un divisor). El ADC es lento para el flanco, pero adecuado para continua.
- En AVR, el pin del bus debe ser analógico (A0..A7) o estar conectado a uno; en ESP32, verificar que el GPIO elegido sea ADC y su atenuación/calibración.
- Para medir el LOW de **un esclavo** (no el del maestro), el esclavo debe sostener la línea baja durante una ventana de diagnóstico; eso requiere un comando o registro de diagnóstico. No confundir con medir el LOW del propio maestro.

#### C. Perfiles eléctricos

Valores de ejemplo, **configurables**, no universales:

| Perfil | VCC | Pull-up | I pull-up (LOW ideal) |
|---|---:|---:|---:|
| ESP 3,3 V | 3,3 V | 2,2 kΩ | 1,50 mA |
| AVR 5 V | 5,0 V | 4,7 kΩ | 1,06 mA |

- El perfil debe describir la **tensión real del bus**, no solo la del microcontrolador maestro.
- No asumir 3,5 V como ideal universal. El autor observó ~3,2 V en pruebas; con conductor largo puede caer. Usar VCC medido/configurado.
- Un ESP32/ESP8266 con pull-up a 5 V puede dañar el GPIO. Mantener el bus en 3,3 V o usar adaptación de nivel.
- Esclavos de 5 V con bus de 3,3 V: `VIH` del AVR es ~3,0 V; 3,3 V es marginal. Para pruebas de cable, no mezclar niveles: es un factor de confusión.
- La herramienta debe permitir `VCC` y `R_pullup` manuales, además de los dos perfiles predeterminados.

### 1.4 Nivel 3 — Impedancia, reactancia, inductancia

| Parámetro | ¿Se puede estimar? | Método / límite |
|---|---|---|
| Resistencia de pull-up | Sí | Valor conocido o medición |
| Capacitancia equivalente | Sí, aproximada | Tiempo de subida RC con umbral conocido |
| Resistencia de salida en LOW | Aproximada | `V_LOW` y corriente |
| Impedancia dinámica | Parcial | Respuesta ante condiciones, no valor absoluto |
| Reactancia capacitiva | Derivada | `Xc = 1/(2πfC)`, a una frecuencia elegida |
| Inductancia del cable | No | Requiere instrumentación adicional |
| Impedancia característica | No | Requiere caracterización de línea |

- `Xc` es una cuenta derivada; no es un diagnóstico de salud del bus por sí misma.
- La estimación de C es de **capacitancia equivalente agregada** (cable + pines + probe + nodos), no la capacitancia física exacta del cable.
- El modelo RC simple no captura línea distribuida, reflexiones ni inductancia. No venderlo como caracterización de línea.

### 1.5 API propuesta (separar núcleo de herramienta)

**Núcleo actual (no renombrar):** `ping(addr)`, `write`, `read`, `writeBlock`, `readBlock`, `identify`, `scan`, `setRetries`, `lastError`, `errorText`, `reset`, `writeBit`, `readBit`, `writeByte`, `readByte`.

**Diagnóstico (propuesta, inicialmente en el ejemplo):**

```cpp
class BusUnHiloDiagnostics : public BusUnHiloMaster {
 public:
  // comunicación
  bool pingNonce(uint8_t addr, uint32_t nonce, uint32_t &nonceInv, uint32_t &rttUs);
  // eléctrico
  uint16_t measureRiseTime();          // cuentas o µs según plataforma
  uint16_t measureLowMillivolts();     // ADC, solo si el pin lo permite
  float estimateCapacitance(float rPullup, float vcc, float thresholdRatio);
  // informe
  void resetStats();
  void printStats(Stream &out);
};
```

- No todas las funciones estarán disponibles en todas las placas; deben devolver un estado “no soportado” explícito, no un valor inventado.
- `measureRiseTime()` necesita acceso a `busLow/busRelease/busRead` (protected) y, en AVR, un pin con comparador si se quiere precisión.
- `measureLowMillivolts()` necesita ADC en el pin del bus; el pin debe elegirse en el ejemplo.
- `pingNonce()` requiere el comando nuevo en la librería; en v1 usar loopback por registros.

**Funciones del borrador:** `getCommunicationStats`, `estimateBusCapacitance`, `testBusSpeed`, `getBusHealth`, `runBusDiagnostics`. Se pueden implementar como métodos de la herramienta, no como API del núcleo en 0.1.4. `testBusSpeed(maxSpeed)` implica cambiar tiempos; no entra en 0.1.4.

### 1.6 Salida

- Serie/CSV primero. LCD opcional y separado (dependencia extra, más código).
- Un renglón por corrida: fecha, placa maestra/esclava, VCC, R pull-up, largo/tipo de cable, perfil, patrón, N, semilla, ok, no_ack, crc, no_presence, bus_stuck, mismatch, RTT min/med/max, rise counts/µs, C estimada, veredicto.
- Veredicto conservador: “sin errores en N tramas”, “errores esporádicos”, “subida lenta”, “LOW fuera de rango”, “no concluyente”.

### 1.7 Criterios de aceptación de BusMETER v1

- Compila en AVR y ESP (al menos compilación; idealmente prueba en UNO/NANO y ESP8266).
- No modifica `src/` de la librería.
- Con un esclavo eco conocido, N=1000 tramas sin errores en cable corto y perfil normal.
- Detecta y cuenta correctamente: esclavo desconectado (`NO_PRESENCE`), dirección inexistente (`NO_ACK`), CRC forzado (si se puede inyectar), línea en bajo (`BUS_STUCK`).
- Gap escritura→lectura configurable y documentado.
- Semilla reproducible.
- Resultados en CSV parseables.

### 1.8 Riesgos

- El propio instrumento agrega capacidad al bus (pin, cables, probe).
- El pin elegido para el bus puede no ser analógico ni tener comparador.
- El ADC y el comparador varían por plataforma.
- La media de subida puede ser tan rápida que el método digital no la resuelva; hay que reportarlo, no inventar un número.
- Un “BUS_STUCK” puede ser subida lenta, no corto; el diagnóstico debe separarlo.

---

## 2. Medición de rise time con resolución (conceptual, sin código todavía)

### 2.1 Por qué el ADC no sirve para el flanco

- AVR: `analogRead()` tarda ~100 µs (prescaler 128, 13 ciclos ADC). El flanco es de µs o menos. Inviable.
- ESP32: ADC SAR, muestreo en µs; tampoco sirve para el flanco.
- El ADC sí sirve para `V_LOW` en continua (con cuidado de referencia, atenuación y calibración).

### 2.2 ATmega328P — comparador analógico + Timer1 Input Capture

- El ATmega328P tiene comparador analógico y Timer1 con input capture.
- `ACIC` (ACSR) conecta la salida del comparador al input capture de Timer1.
- `ICIE1` (TIMSK1) habilita la interrupción de captura.
- Timer1 sin prescaler a 16 MHz: 62,5 ns por tick; 16 bits = 4,096 ms de rango. Un flanco de µs entra sobrado.
- Referencia del comparador:
  - `AIN0`/`AIN1` son PD6/PD7. El bus debe estar en uno de esos pines, o usar un comparador externo.
  - Referencia interna de 1,1 V (bandgap) o divisor externo a 0,632·VCC.
  - Con bandgap de 1,1 V, usar la fórmula general con `V(t)=1,1 V`; no es el 63,2 %, pero es un punto conocido y repetible.
  - Con divisor a 0,632·VCC, se puede usar `C=t/R` directamente.
- Secuencia: bajar la línea, liberar, poner Timer1 en 0, esperar la captura del comparador, leer `ICR1`.
- Límites: retardo de propagación del comparador, histéresis, ruido, capacidad del pin/probe. Calibrar contra osciloscopio.
- Alternativa gruesa: Timer1 libre + interrupción de pin change; resolución limitada por latencia de ISR (~2-4 µs). Sirve para flancos lentos, no para rápidos.

### 2.3 ESP32 — RMT / esp_timer / comparador externo

- RMT: periférico de captura de flancos, reloj APB 80 MHz → 12,5 ns por tick, contador de 15 bits. Excelente para tiempos de flanco **digital**.
- `esp_timer`: resolución de 1 µs; demasiado gruesa para flancos sub-µs.
- El RMT captura el cruce del umbral digital del pin; no permite elegir un umbral analógico como el comparador AVR.
- En el ESP32 clásico no hay un comparador analógico de propósito general equivalente al AVR. Verificar la variante concreta antes de afirmarlo o implementarlo. Si se necesita umbral preciso: comparador externo + RMT/GPIO.
- Para medir desde la liberación: hace falta un timestamp de referencia. El RMT puede capturar el flanco de subida, pero el instante de “release” es una acción de software; hay latencia. Opciones: usar RMT TX para generar el pulso bajo y RMT RX para capturar la subida, o un GPIO de referencia, o calibrar. No prometer precisión absoluta sin validar con osciloscopio.
- El ADC del ESP32 no sirve para el flanco.

### 2.4 ESP8266

- No tiene RMT. `micros()` y interrupciones de GPIO dan resolución limitada por latencia de interrupción/software.
- Para precisión: comparador externo + timer/GPIO o osciloscopio.
- No presentar una medición de subida del ESP8266 como equivalente a la del AVR.

### 2.5 Umbral, calibración y validez

- La fórmula `C=t/R` solo vale si `t` es el tiempo hasta `0,632·VCC`.
- Si se mide con entrada digital, reportar “tiempo hasta umbral digital”, no capacitancia.
- La capacidad del probe y del pin de medición se suma al bus; el resultado es una **capacitancia equivalente**.
- La dispersión de R (tolerancia), VCC y umbral afecta la estimación. Repetir y reportar min/mediana/max.
- Validar contra osciloscopio en al menos una configuración antes de confiar en los números.

### 2.6 Qué se puede y qué no

- **Sí:** estimar C equivalente, comparar configuraciones, detectar flancos lentos, diagnosticar `BUS_STUCK` por subida lenta, medir `V_LOW` con ADC.
- **No:** impedancia característica, inductancia, integridad de señal completa, BER real por byte sin un test dedicado.
- **Derivado:** `Xc = 1/(2πfC)` a una frecuencia elegida; no es un diagnóstico por sí mismo.

---

## 3. Perfil de tiempos runtime (B)

### 3.1 API propuesta

```cpp
enum {
  BUH_PERFIL_NORMAL = 0,
  BUH_PERFIL_LENTO  = 1
};

// maestro
void begin(uint8_t pin, bool internalPullup = false,
           uint8_t perfil = BUH_PERFIL_NORMAL);

// esclavo
void begin(uint8_t pin, uint8_t address, bool internalPullup = false,
           uint8_t perfil = BUH_PERFIL_NORMAL);
```

- Los parámetros nuevos van con default: los sketches actuales siguen compilando.
- `BUH_MODO_LENTO` compile-time puede quedar como **perfil por defecto**; el runtime lo puede sobrescribir.
- Exponer `uint8_t perfil() const` para diagnóstico.
- El sketch ya puede **leer** `BUH_MODO_LENTO` con `#ifdef` después de `#include <BusUnHilo.h>` para imprimir el perfil compilado; no puede cambiarlo.

### 3.2 Implementaciones y costo

1. **Variables miembro** con los tiempos cargados en `begin()`: simple, ~30-40 B de RAM, `delayMicroseconds()` con valor variable.
2. **Tabla en PROGMEM + puntero**: poca RAM, lectura de flash de pocos ciclos; para delays de µs es despreciable.
3. **`switch (_perfil)` con delays constantes**: sin RAM extra, más flash. Puede ser la mejor en ATtiny85 (8 KB flash, 512 B RAM).

Medir en ATtiny85 y UNO: tamaño de flash, RAM estática, y funcionamiento real. No elegir por intuición.

### 3.3 Header-only

- Permite que un `#define` antes del `#include` afecte la implementación.
- Cambia el modelo de compilación; puede generar ODR si hay varios `.cpp`; el `.cpp` actual habría que vaciarlo/guardarlo.
- No es el camino principal. El runtime da la misma comodidad sin cambiar la estructura.

### 3.4 Documentación obligatoria

- El perfil debe ser **el mismo en todos los nodos**. Si no, cada placa usa tiempos distintos y el bus se rompe.
- Un registro/comando de “perfil activo” sirve para diagnosticar una vez que hay comunicación; no rescata un bus desincronizado.
- Al cambiar de perfil, recompilar/actualizar **todas** las placas.

### 3.5 Cambio de perfil por difusión (futuro)

- La difusión no tiene ACK; un nodo que se pierde la orden queda con otro perfil y parte el bus.
- Si se implementa: **prepare → commit → ventana de prueba → rollback automático**.
- Cada nodo guarda el perfil anterior y vuelve a él si no recibe tramas válidas en T.
- Requiere perfiles runtime y estado en cada esclavo.
- **No en 0.1.4.** Candidato a 0.1.5/0.2.0.

---

## 4. Autonomía (C) - Diseño

### 4.1 Nivel 1 — failsafe local

- Cada esclavo, si no recibe tramas durante un tiempo, ejecuta una regla local con sus propias entradas.
- No necesita datos de otros nodos.
- API propuesta: `uint32_t lastFrameMs()` y `bool linkAlive(uint16_t timeoutMs)`.
- Actualizar el timestamp con **cualquier trama válida**, aunque no sea para ese esclavo. El esclavo ya lee y valida el CRC antes de filtrar por dirección; solo falta marcar el evento.
- Sin callback automático: el usuario lo consulta en su `loop()`.
- Ejemplo: relé que sigue un pulsador local si no hay maestro; al volver, retoma el control remoto.

### 4.2 Nivel 2 — caché por difusión

- El maestro difunde valores no críticos que ya conoce a un banco reservado (por ejemplo `0xE0..0xEF`).
- Todos los esclavos los reciben en `onWrite()` porque la difusión de escritura sí se entrega.
- No hace falta sniffing de respuestas ajenas; eso obligaría a leer datos/CRC de tramas no dirigidas y aumentaría la sección crítica.
- La difusión no tiene ACK: repetir periódicamente; el esclavo marca antigüedad y lo usa solo como dato no crítico.
- Patrón de ejemplo, no función del núcleo en 0.1.4.

### 4.3 Nivel 3 — maestro suplente (0.2.0)

- El problema no es eléctrico: open-drain tolera que dos tiren bajo. El problema es que el protocolo no tiene arbitraje.
- `transact()` no hace carrier sense; `reset()` solo verifica 5 µs en alto.
- Dos maestros se pisan las tramas; los esclavos ven basura.
- Hacen falta: elección de suplente, detección de silencio, backoff, recuperación de colisión, handshake de retorno del maestro original.
- Además, el suplente necesita el mapa de registros y la lógica de la aplicación.
- **No es 0.1.x.** 0.2.0 como mínimo.

### 4.4 Seguridad

- Nada de seguridad de vida (humo, fuego, pánico, accesos críticos) puede depender de este bus ni de estos failsafes. Esas señales necesitan su propio camino independiente.

---

## 5. Plan de fases y checklist

### Fase 0 — Baseline (hecho)

- [x] `git init`, baseline 0.1.3, tag `v0.1.3-oficial`.
- [x] Copia `BusUnHilo-0.1.4`, rama `0.1.4`, 37 archivos idénticos.
- [x] `memoria.md`.

### Fase 1 — BusMETER v1 (sin tocar `src/`)

- [ ] `examples/BusMETER/Maestro_BusMETER` (o nombre a definir).
- [ ] `examples/BusMETER/Esclavo_Eco` para NANO/UNO.
- [ ] Eco por registros con `writeBlock/readBlock`, `setRetries(0)`.
- [ ] Patrones fijos + PRNG con semilla.
- [ ] Conteo de errores y CSV.
- [ ] Gap escritura→lectura configurable.
- [ ] Perfil compilado normal/lento: dos builds documentados.
- [ ] Prueba en cable corto, N=1000, sin errores.
- [ ] Prueba de esclavo desconectado, dirección inexistente y línea en bajo.
- [ ] Commit separado.

### Fase 2 — Perfil runtime

- [ ] API `begin(..., perfil)` con defaults.
- [ ] Implementación elegida tras medir RAM/flash en ATtiny85.
- [ ] `BUH_MODO_LENTO` como default compatible.
- [ ] Pruebas normal/lento con BusMETER.
- [ ] Documentación: mismo perfil en todos los nodos.
- [ ] Commit separado.

### Fase 3 — BusMETER v2

- [ ] Normal y lento en una corrida (runtime).
- [ ] `measureRiseTime()` con método por plataforma.
- [ ] `measureLowMillivolts()` con ADC.
- [ ] `estimateBusCapacitance()` con umbral declarado.
- [ ] `pingNonce` (comando nuevo) o loopback documentado.
- [ ] Informe/veredicto conservador.
- [ ] LCD opcional, Separado.
- [ ] Commit separado.

### Fase 4 — Nivel 1

- [ ] `lastFrameMs()`/`linkAlive()`.
- [ ] Timestamp con tramas válidas ajenas.
- [ ] Ejemplo de failsafe local.
- [ ] Commit separado.

### Fase 5 — Nivel 2

- [ ] Banco de caché reservado.
- [ ] Ejemplo maestro publica / esclavo usa.
- [ ] Antigüedad y dato no crítico.
- [ ] Commit separado.

### Fase 6 — Nivel 3 (0.2.0)

- [ ] Documento de diseño de arbitraje/elección.
- [ ] Prototipo aislado, no en 0.1.x.
- [ ] Pruebas de colisión y retorno.

---

## 5. Matriz de pruebas (plantilla)

| Fecha | Maestro | Esclavo | VCC | Pull-up | Cable (m/tipo) | Perfil | Patrón | N | OK | NO_ACK | CRC | NO_PRESENCE | BUS_STUCK | Mismatch | RTT med | Rise | C est. | Semilla | Notas |
|---|---|---|---|---|---:|---|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|---|
| | | | | | | | | | | | | | | | | | | |

Reglas:

- Un cambio por corrida (cable, pull-up, perfil). No mezclar.
- Registrar la semilla.
- Repetir al menos 3 veces la misma configuración.
- No comparar 3,3 V con 5 V como si fuera solo “cable”.
- Anotar si se usó pull-up interna o externa.
- Anotar el pin del bus y si es analógico/comparador.

---

## 6. Decisiones pendientes

1. ¿BusMETER v1 usa solo loopback por registros o agrega ya `CMD_PING_NONCE`?
2. ¿Qué pin usa BusMETER en UNO/NANO para tener ADC y, si se quiere comparador, AIN0/AIN1 (PD6/PD7)?
3. ¿Se acepta un comparador externo en ESP32/ESP8266 para medición precisa, o se declara “solo digital” en esas plataformas?
4. ¿El perfil runtime se implementa con variables, PROGMEM o `switch`? Medir en ATtiny85.
5. ¿Se ofrece header-only como opción avanzada o se descarta?
6. ¿Qué banco de registros se reserva para caché (`0xE0..0xEF` u otro)?
7. ¿Se agrega un registro de perfil/versión de solo lectura? ¿Qué dirección?
8. ¿LCD en BusMETER v1 o solo serie/CSV?

---

### Referencias externas

- Repo: `https://github.com/ETI-PATAGONIA-AR/Libreria-BusUnHilo-Arduino`
- Tag 0.1.3: `https://github.com/ETI-PATAGONIA-AR/Libreria-BusUnHilo-Arduino/tree/0.1.3`
- Índice Library Manager: `https://downloads.arduino.cc/libraries/library_index.json`
- Foro: `https://forum.arduino.cc/t/aporte-libreria-busunhilo-bus-bidireccional-maestro-esclavo-por-1-cable/1467364`
- Datasheets a consultar antes de implementar: ATmega328P (analog comparator, Timer1 input capture), ESP32 (RMT, esp_timer), ESP8266 (timers/GPIO).

---

## 7. Frases que no se van a usar

- “Mide la impedancia del cable” → no con este hardware.
- “Mide el 63,2 % con una entrada digital” → no sin umbral conocido.
- “El esclavo devuelve el eco en la misma trama” → no con el protocolo actual; es loopback de dos transacciones.
- “El maestro suplente ya funciona” → no hasta 0.2.0 y con arbitraje.
- “Es un mesh” → no; es maestro-esclavo con failsafe/caché.
- “Está probado en hardware” → solo lo que el usuario confirme en el sketch y se registre en la matriz.


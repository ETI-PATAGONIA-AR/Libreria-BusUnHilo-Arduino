# REDsensorMasterSlave

Ejemplo de una red **BusUnHilo** con un maestro y dos esclavos. El maestro envía órdenes a los LED de los esclavos y, a la vez, lee sus entradas para controlar dos LED locales.

## Archivos

- `Maestro.ino`: lee dos pulsadores locales, controla los LED de ambos esclavos, consulta sus entradas y refleja sus estados en dos LED del maestro.
- `Esclavo1_Pulsador_LED.ino`: esclavo con dirección `1`; informa el estado de un pulsador y recibe órdenes para encender o apagar su LED.
- `Esclavo2_Pote_LED.ino`: esclavo con dirección `2`; informa la lectura de un potenciómetro y recibe órdenes para encender o apagar su LED.
- `Ejemplo_RedSENSORES.pdsprj`: proyecto de simulación para Proteus.

## Funcionamiento

El maestro realiza dos tareas:

1. Lee sus pulsadores en los pines `5` y `6`. Cada pulsador controla, respectivamente, el LED del Esclavo 1 y el LED del Esclavo 2. El envío se realiza cuando cambia el estado o cuando hay que reintentar un envío que falló.
2. Cada `40 ms`, consulta las entradas de ambos esclavos:
   - La lectura del pulsador del Esclavo 1 controla el LED 1 del maestro (pin `9`).
   - La lectura del potenciómetro del Esclavo 2 controla el LED 2 del maestro (pin `10`). El maestro usa umbrales de histéresis (`140` para encender y `115` para apagar) para evitar parpadeos cerca del punto de cambio.

El Esclavo 1 tiene la dirección `1` y el Esclavo 2 la dirección `2`. Ambos usan el registro `0x01` para recibir el estado de su LED y el registro `0x02` para entregar su entrada al maestro. El potenciómetro del Esclavo 2 se convierte al rango `0..255`.

## Cableado

El bus de datos es una única línea compartida por el pin `8` del maestro y de los dos esclavos. Usa una resistencia *pull-up* externa de `4,7 kΩ` entre el bus y `+5 V`, y conecta GND entre todas las placas.

### Maestro

- Pulsador A: pin `5` a GND; usa `INPUT_PULLUP`.
- Pulsador B: pin `6` a GND; usa `INPUT_PULLUP`.
- LED 1: pin `9` → resistencia de `330 Ω` → LED → GND.
- LED 2: pin `10` → resistencia de `330 Ω` → LED → GND.

### Esclavo 1 (dirección 1)

- Pulsador: pin `2` a GND; usa `INPUT_PULLUP`.
- LED: pin `4` → resistencia de `330 Ω` → LED → GND.

### Esclavo 2 (dirección 2)

- Potenciómetro: extremos a `+5 V` y GND; cursor a `A0`.
- LED: pin `4` → resistencia de `330 Ω` → LED → GND.

## Prueba

1. Carga cada archivo `.ino` en su placa correspondiente.
2. Conecta el bus, la resistencia *pull-up* y GND común.
3. Abre el monitor serie del maestro a **57600 baudios** para ver el estado y los errores de comunicación.
4. Prueba los pulsadores del maestro y de los esclavos, y gira el potenciómetro del Esclavo 2.

Se probó este montaje con hasta **34 metros de conductor** sin fallas. Es un resultado de esa prueba concreta, no una garantía de distancia para cualquier instalación: el comportamiento puede depender del cable, la topología, las conexiones, las condiciones eléctricas y la configuración del bus.

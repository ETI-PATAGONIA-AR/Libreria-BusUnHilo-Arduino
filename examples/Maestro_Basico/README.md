# Maestro_Basico

Ejemplo básico de un maestro que se comunica con un esclavo mediante la librería **BusUnHilo**. El maestro escribe en un registro para alternar el LED del esclavo y lee otro registro para recibir una medición analógica.

Este sketch está pensado para trabajar con [`Esclavo_Basico`](../Esclavo_Basico/), configurado con dirección `1`.

## Archivo

- `Maestro_Basico.ino`: escribe y lee los registros del esclavo, e informa los resultados o errores por el monitor serie.

## Secuencia de comunicación

Cada 500 ms, el maestro:

1. Invierte el estado del bit del LED y lo escribe en el registro `0x01` del esclavo con dirección `1`.
2. Lee el registro `0x02` del mismo esclavo.
3. Muestra en el monitor serie el valor recibido (`0..255`) o el mensaje de error del bus.

En el esclavo del ejemplo, `0x01` controla el LED del pin `13` y `0x02` entrega el valor analógico de `A0`, convertido al rango de 8 bits.

## Cableado

- Conecta el pin `8` del maestro y el pin `8` del esclavo al mismo cable de datos.
- Añade una resistencia *pull-up* externa de `4,7 kΩ` entre el bus y `+5 V`.
- Conecta las tierras (`GND`) de ambas placas.
- Conecta la señal analógica que quieras medir a `A0` del esclavo.

## Puesta en marcha

1. Carga `Maestro_Basico.ino` en una placa y `Esclavo_Basico.ino` en otra.
2. Conecta el bus compartido, la resistencia *pull-up* y GND común.
3. Abre el monitor serie del maestro a `115200` baudios.
4. Verifica que el LED del esclavo alterne y que aparezcan las lecturas del sensor.

El maestro usa `bus.begin(8)` y requiere la resistencia *pull-up* externa indicada. Si una operación falla, muestra el texto correspondiente a `bus.lastError()`.

# Esclavo_Basico

Ejemplo básico de un esclavo con la librería **BusUnHilo**. El dispositivo, con dirección `1`, ofrece al maestro una lectura analógica de `A0` y permite encender o apagar el LED integrado del pin `13`.

Este ejemplo se complementa con [`Maestro_Basico`](../Maestro_Basico/), que consulta el sensor y conmuta el LED del esclavo.

## Archivo

- `Esclavo_Basico.ino`: configura el bus, registra las funciones de lectura y escritura, y atiende las solicitudes del maestro.

## Registros del esclavo

| Registro | Operación | Función |
| --- | --- | --- |
| `0x01` | Escritura | Bit 0 controla el LED del pin `13` (`0` apagado, `1` encendido). |
| `0x02` | Lectura | Devuelve la lectura de `A0` escalada al rango `0..255`. |
| `0xFF` | Lectura | Consulta el tipo de dispositivo, gestionado automáticamente por la librería. |

El sketch actualiza la lectura analógica cada 200 ms. La función `onRead()` devuelve el valor previamente guardado, de modo que la conversión analógica no se realiza durante la respuesta del bus.

## Cableado

- Conecta el pin `8` del maestro y el pin `8` del esclavo al mismo cable de datos.
- Añade una resistencia *pull-up* externa de `4,7 kΩ` entre el bus y `+5 V`.
- Conecta las tierras (`GND`) de ambas placas.
- Conecta el sensor o señal analógica a `A0`.
- El LED integrado del pin `13` se controla desde el bus.

## Prueba con el maestro

1. Carga `Esclavo_Basico.ino` en una placa y `Maestro_Basico.ino` en otra.
2. Conecta el bus, la resistencia *pull-up* y GND común.
3. Abre el monitor serie del maestro a `115200` baudios.
4. El maestro alterna el LED del esclavo cada 500 ms y muestra el valor leído de `A0`.

El esclavo llama a `bus.poll()` continuamente para atender las tramas; evita introducir esperas largas en su `loop()`.

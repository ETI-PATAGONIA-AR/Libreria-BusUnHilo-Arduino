# Maestro_Escaner

Este ejemplo configura un maestro BusUnHilo que busca dispositivos conectados al bus e informa sus direcciones y tipos por el monitor serie. La carpeta incluye un proyecto de Proteus y los archivos `.hex` precompilados del maestro y los dos esclavos para probar la simulación sin compilar los sketches.

## Archivos

- `Maestro_Escaner.ino`: sketch fuente del maestro. Escanea las direcciones `1..254`, identifica los dispositivos encontrados e imprime los resultados.
- `Maestro_Escaner.ino.hex`: firmware compilado del maestro para cargarlo en la placa correspondiente del proyecto de Proteus.
- `Esclavo1.ino.hex` y `Esclavo2.ino.hex`: firmware compilado de los esclavos que participan en la simulación.
- `Maestro_Escaner.pdsprj`: proyecto de simulación para Proteus.

Los archivos `.hex` son firmware compilado, no sketches fuente editables. Si modificas un sketch, vuelve a compilarlo y reemplaza el `.hex` correspondiente en el proyecto de Proteus.

## Cómo funciona el sketch

Al iniciar, el maestro:

1. Abre el puerto serie a `57600` baudios.
2. Inicializa el bus en el pin `8`.
3. Busca esclavos en las direcciones `1..254`; el arreglo del ejemplo guarda hasta 16 dispositivos encontrados.
4. Consulta el tipo de cada dispositivo y muestra su dirección, el código de tipo en hexadecimal y una etiqueta (`sensor`, `display`, `salidas` o `generico`).

El escaneo se realiza una vez en `setup()`. Si no encuentra dispositivos, el maestro muestra el mensaje de error del bus. El `loop()` queda vacío.

## Probar en Proteus

1. Abre `Maestro_Escaner.pdsprj` con Proteus.
2. Verifica que el microcontrolador del maestro use `Maestro_Escaner.ino.hex` y que los microcontroladores de los esclavos usen `Esclavo1.ino.hex` y `Esclavo2.ino.hex`.
3. Ejecuta la simulación y abre la terminal virtual asociada al puerto serie del maestro, configurada a `57600` baudios.
4. Observa el resultado del escaneo al iniciar la simulación.

## Montaje físico

Para probarlo con placas reales, carga `Maestro_Escaner.ino` en el maestro y el firmware BusUnHilo correspondiente en cada esclavo. Conecta el pin `8` de las placas al mismo conductor de datos, utiliza la resistencia *pull-up* externa requerida por el bus (por ejemplo, `4,7 kΩ` hacia `+5 V`) y comparte GND entre las placas. Abre el monitor serie a `57600` baudios para ver los dispositivos detectados.

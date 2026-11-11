# Portero eléctrico con BusUnHilo

Este ejemplo implementa un portero eléctrico distribuido: un Arduino maestro en el panel de calle y un Arduino esclavo por departamento. El maestro gestiona las llamadas, concede el uso de la línea de audio y controla la cerradura; cada esclavo atiende los pulsadores y la llave de su departamento, reproduce el timbre y habilita el audio cuando recibe permiso.

## Contenido

- `Portero_Master.ino`: sketch del maestro. La cantidad de departamentos se configura con `NUM_DEPTOS`.
- `Portero_Esclavo1.ino` y `Portero_Esclavo2.ino`: ejemplos de esclavo para los departamentos 1 y 2. Se usa el mismo programa en cada Arduino esclavo; cambia `DIR_DEPTO` para asignar a cada placa su número, del 1 al 10.
- `DiagramCIRCUIT.png`: diagrama del circuito.
- `PCB_AUDIO.pdf`: documentación de la placa de audio.

## Cómo funciona

- En reposo, el maestro recorre los departamentos para leer el estado de cada llave.
- Al presionar el pulsador de llamada de un departamento, el maestro ordena a su esclavo reproducir dos veces el sonido “ding dong”. Hay una ventana de 30 segundos para atender.
- Para atender, se cierra la llave del departamento. El maestro concede la línea de audio a ese departamento y renueva el permiso periódicamente. Mientras la comunicación está activa, los demás departamentos no pueden usar la línea ni iniciar llamadas.
- La cerradura se activa solo mientras la llave del departamento está cerrada y se mantiene presionado el pulsador de apertura. Al soltarlo o abrir la llave, el maestro desactiva la salida.
- Si el maestro deja de renovar el permiso, el esclavo libera su salida de intercomunicador mediante un watchdog de un segundo. Si se abre la llave, la salida se libera inmediatamente.
- Si el bus falla repetidamente durante una comunicación, el maestro corta el audio y la cerradura y vuelve al estado de reposo.

La llamada también puede atenderse sin llamada previa si, estando el sistema en reposo, se cierra la llave del departamento. Una llave que ya estaba cerrada al encender o durante una comunicación no genera atención: hay que abrirla y volverla a cerrar.

## Configuración de departamentos

En `Portero_Master.ino`, ajusta `NUM_DEPTOS` al número de departamentos que vas a usar (de 1 a 10). Los pines de los pulsadores de llamada están definidos en `PIN_PULSADORES`.

En cada placa esclava, configura `DIR_DEPTO` con un valor distinto: departamento 1 usa dirección 1, departamento 2 usa dirección 2, y así sucesivamente. El resto del sketch es común a todos los departamentos.

## Modo lento: Proteus y tendidos largos

En `BusUnHilo.h` existe la opción `BUH_MODO_LENTO`. Al habilitarla, los tiempos del bus se alargan aproximadamente **3,5 veces**. Esto puede ayudar a que Proteus simule correctamente las comunicaciones y también puede resultar útil en tendidos largos.

Para probarlo en Proteus:

1. En `BusUnHilo.h`, descomenta la línea `#define BUH_MODO_LENTO` (o comenta la línea que la deshabilita, según cómo esté escrita en la versión de la librería).
2. Vuelve a compilar y cargar los sketches del maestro y de todos los esclavos con esa configuración.
3. Configura el monitor serie del maestro a **57600 baudios**.

En placas reales, comienza con el modo lento deshabilitado y prueba primero la velocidad normal. Actívalo solo si observas errores de comunicación o mensajes **“Sin ACK”**. Si cambias el modo, recompila los sketches que participan en el bus con la misma versión/configuración de la librería. El modo lento no garantiza por sí solo una distancia determinada: el resultado depende también del cableado y de las condiciones de la instalación.

## Lógica de las salidas: HIGH o LOW activo

Las salidas de cerradura, audio e intercomunicador admiten lógica activa en nivel alto o bajo. En cada pareja de opciones debe quedar **exactamente una macro sin comentar**. Si quedan las dos activas o las dos comentadas, el sketch genera un error de compilación con una indicación.

En `Portero_Master.ino`:

```cpp
// Cerradura en D13: deja activa solo una opción
#define CERRADURA_EN_ALTO
//#define CERRADURA_EN_BAJO

// Habilitación del GND de audio en A0: deja activa solo una opción
//#define AUDIO_EN_ALTO
#define AUDIO_EN_BAJO
```

En cada sketch de esclavo, para la salida D13:

```cpp
// Deja activa solo una opción
//#define RELE_EN_ALTO
#define RELE_EN_BAJO
```

El nombre `RELE_EN_ALTO/BAJO` se conserva por compatibilidad e historia del ejemplo: la salida puede manejar un relé, pero en el prototipo de los esclavos se usa un transistor NPN para la función de mute. En ese circuito, para que el amplificador pueda reproducir el audio cuando el departamento atiende, la configuración requerida es `RELE_EN_BAJO`.

La lógica se configura según el módulo o etapa de potencia que conectes: algunas placas de relé se activan con HIGH y otras con LOW. Revisa la polaridad de tu circuito antes de conectar la cerradura o el intercomunicador. No conectes una bobina de relé ni una carga de cerradura directamente a un pin del Arduino; utiliza un controlador adecuado y una fuente compatible.

El sketch coloca las salidas en su nivel inactivo **antes** de configurarlas como salidas para evitar un pulso al arrancar. Aun así, en el maestro D13 puede producir un breve clic por el parpadeo del bootloader; si eso no es aceptable, considera usar otro pin y adaptar el circuito/sketch.

## Lógica de las entradas y del bus

Los pulsadores y las llaves se conectan entre el pin y GND y se leen con `INPUT_PULLUP`: **LOW significa activo** (pulsador presionado o llave cerrada), y HIGH significa inactivo. El bus también interpreta el nivel bajo como el estado lógico activo durante la comunicación. No confundas el nivel eléctrico LOW con el valor lógico de las señales de aplicación: los sketches convierten el estado de cada entrada a los bits y registros que procesa el maestro.

## Parámetros serie

El maestro define la velocidad mediante `BAUDIOS`. En la configuración incluida aparece `57600`, indicada para Proteus; el comentario del sketch señala **115200 baudios para hardware real**. Asegúrate de que el monitor serie use la misma velocidad configurada en el sketch.

## Notas para Arduino Uno/Nano

En los esclavos, el tono del buzzer pasivo utiliza Timer1 en el pin D9. Por eso, mientras se usa esta función, no utilices `analogWrite()` en D9/D10 ni la biblioteca Servo, que también depende de Timer1 en estas placas.

## Cableado general

<img width="1497" height="2042" alt="DiagramCIRCUIT" src="https://github.com/user-attachments/assets/e0a00158-f75e-4964-b646-11a356668ae5" />

- Une el pin D8 del maestro y de todos los esclavos a un único conductor de datos.
- Coloca una resistencia *pull-up* externa de `4,7 kΩ` entre el bus y `+5 V`.
- Conecta GND común entre todas las placas.
- En el maestro, conecta los pulsadores de llamada a los pines definidos en `PIN_PULSADORES`, cada uno hacia GND; conecta la salida de cerradura a D13 y la habilitación de audio a A0.
- En cada esclavo, conecta el pulsador de apertura a D2 y la llave a D3, ambos hacia GND; conecta el buzzer pasivo a D9 y la etapa de mute/intercomunicador a D13.

Usa una etapa de manejo apropiada para relés, cerraduras y circuitos de audio, respetando las tensiones y corrientes de cada carga.

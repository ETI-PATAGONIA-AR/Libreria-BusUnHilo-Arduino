# ControlPWM

Este ejemplo muestra cómo usar un potenciómetro para controlar el brillo de LEDs PWM a través del bus de un hilo.

## Idea del ejemplo

La idea es que el valor leído del potenciómetro se use como referencia de brillo y se distribuya a varios dispositivos conectados al mismo bus:

- El maestro usa ese valor para controlar su propio LED PWM.
- El maestro también lo envía al Esclavo 1 para que este ajuste el brillo de su LED.
- El Esclavo 2 entrega el valor del potenciómetro al maestro mediante un registro de lectura.

## Archivos del ejemplo

- `Maestro_PotPWM.ino`: sketch del maestro.
- `Esclavo1_LED_PWM.ino`: esclavo 1, que recibe el valor de brillo y lo aplica a un LED con PWM.
- `Esclavo2_Pote_LED.ino`: esclavo 2, que mide un potenciómetro y expone su valor al bus.

## Flujo del dato

1. El Esclavo 2 mide el valor del potenciómetro conectado a `A0`.
2. El maestro lee ese valor desde el registro `0x02` del Esclavo 2.
3. El maestro aplica ese valor al LED PWM del maestro (pin 9) con `analogWrite()`.
4. El maestro reenvía el mismo valor al Esclavo 1, en el registro `0x03`.
5. El Esclavo 1 escribe ese valor en su LED PWM (pin 3), cambiando su brillo.

## Registros

### Esclavo 2 (`direccion 2`)
- `0x01` (escritura): activa/desactiva el LED local del esclavo.
- `0x02` (lectura): valor del potenciómetro, de `0` a `255`.

### Esclavo 1 (`direccion 1`)
- `0x03` (escritura): brillo del LED, de `0` a `255`.

## Cableado

### Bus
- Pin `8` de todas las placas al mismo cable de datos.
- Pull-up externo de `4,7 kΩ` hacia `+5V`.
- GND común entre maestro y esclavos.

### Maestro
- LED PWM en pin `9` con resistencia de `330 Ω` hacia GND.

### Esclavo 1
- LED PWM en pin `3` con resistencia de `330 Ω` hacia GND.

### Esclavo 2
- Potenciómetro: extremo a `+5V`, extremo a `GND`, cursor a `A0`.
- LED en pin `4` con resistencia de `330 Ω` hacia GND.

## Cómo funciona el programa

### `Esclavo2_Pote_LED.ino`
Este esclavo tiene dos tareas principales:

- leer el potenciómetro con `analogRead(A0)`
- convertir el valor con `>> 2` para trabajarlo en `0..255`
- responder al maestro cuando le pida el registro `0x02`
- aceptar un comando de escritura en `0x01` para encender o apagar su LED local

### `Esclavo1_LED_PWM.ino`
Este esclavo tiene un solo objetivo:

- escuchar el bus
- si recibe un dato en el registro `0x03`, ejecutar `analogWrite(PIN_LED_PWM, valor)`
- así el LED del esclavo cambia de brillo según el valor recibido

### `Maestro_PotPWM.ino`
Este sketch hace la coordinación:

- cada cierto tiempo lee el valor del potenciómetro del Esclavo 2
- controla el LED del maestro con ese mismo valor
- compara el valor con el último enviado y, si cambió lo suficiente, lo escribe al Esclavo 1

Esto permite que ambos LEDs se comporten de forma sincronizada con el movimiento del potenciómetro.

## Ejemplo de uso

1. Carga `Esclavo2_Pote_LED.ino` en la placa del esclavo 2.
2. Carga `Esclavo1_LED_PWM.ino` en la placa del esclavo 1.
3. Carga `Maestro_PotPWM.ino` en la placa del maestro.
4. Gira el potenciómetro del esclavo 2.
5. El LED del maestro y el LED del esclavo 1 deben variar su brillo de manera proporcional.

## Observación

El maestro informa por monitor serie si falla una lectura o escritura del bus. La velocidad de comunicación puede configurarse con `Serial.begin(115200)`.

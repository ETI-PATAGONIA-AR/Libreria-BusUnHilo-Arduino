EJEMPLO DOMOTICA con BusUnHilo  (ESP32 o ESP8266 maestro + ATtiny85 x2 + Arduino UNO)

PLACAS Y DIRECCIONES
  Master_ESP32      ESP32 WROOM o ESP8266 (NodeMCU / Wemos D1 mini)  - el mismo sketch para las dos
                    bus: GPIO23 (ESP32)  /  GPIO4 = D2 (ESP8266).  Sin pulsadores fisicos.
  SL1_Smag_A85      ATtiny85      0x01  bus PB0, sensor magnetico PB3
  SL2_Spir_A85      ATtiny85      0x02  bus PB0, PIR PB4
  SL3_StempRelay_A1 Arduino UNO   0x03  bus D2, DHT11 D4, reles D5..D8, pulsadores D9..D12

ANTES DE COMPILAR
  1) Master_ESP32.ino: completar ssid, password y otaPassword (y la IP si hace falta).
     Placa en el IDE: ESP32 Dev Module  o  NodeMCU 1.0 / LOLIN(WEMOS) D1 mini.
  2) BusUnHilo.h: activar  #define BUH_ESP_REGISTROS  (quitar las //). Vale para ESP32 y ESP8266.
     Si hay errores de comunicacion, probar en su lugar  #define BUH_MODO_LENTO.
     Es un solo archivo de libreria: despues recompilar TODAS las placas.
  3) Libreria DHT sensor library (Adafruit) para el UNO.

ESP8266: PINES
  Usar GPIO4 (D2) o GPIO5 (D1) o GPIO14 (D5), GPIO12 (D6), GPIO13 (D7).
  NO usar GPIO0, GPIO2, GPIO15 (definen el arranque) ni GPIO16 (no sirve para el bus).

BUS (importante)
  - UNA sola resistencia pull-up de 4k7, a 3,3 V. Los esclavos NO activan su pull-up interna.
  - Antes de conectar el ESP, medir el bus con tester: tiene que dar ~3,3 V y nunca mas de 3,6 V.
  - Opcional: 220 ohm en serie entre el bus y el pin del ESP, como proteccion.
  - GND comun entre todas las placas.
  - Los esclavos de 5 V ven el nivel alto del bus en 3,3 V: el minimo que aceptan es 3,0 V. Funciona
    en distancias cortas; para mas margen usar un convertidor de nivel bidireccional (tipo BSS138).
  - ESP8266 tiene un solo nucleo y el WiFi interrumpe mas el codigo: si ves errores de comunicacion
    pasar a BUH_MODO_LENTO (el CRC y los reintentos cubren fallos esporadicos).

PRUEBA EN ORDEN
  1) Solo el ESP: el dashboard tiene que mostrar "Sin conexion" en todo (no 0 C ni "Cerrada").
  2) Agregar un esclavo por vez. En el monitor serie aparece "[BUS] ... CONECTADO".
  3) Esporadicamente puede verse "UNO: Sin ACK": es la lectura del DHT11 (~25 ms sin atender el bus).
     El maestro reintenta; solo es problema si el UNO se declara "SIN CONEXION".
  4) Historial de temperatura: una muestra cada 10 min. Para probar rapido, MUESTRA_CADA_MS = 10000.
     Se guarda en RAM (se pierde al reiniciar). La hora la pone el navegador.

REGISTROS DEL UNO (0x03)
  escritura 0x00: mascara de reles (bit0..bit3)
  lectura   0x00 temperatura (0xFF = sin lectura) | 0x01 humedad | 0x02 estado real de reles | 0x03 pulsadores

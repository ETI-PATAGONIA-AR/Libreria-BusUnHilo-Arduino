/*
  ESP32 o ESP8266 - Maestro BusUnHilo + Dashboard Web + IP fija + OTA   (el mismo sketch sirve para las dos)

  - WiFi con IP estatica y OTA (si no hay WiFi, el bus y los pulsadores siguen funcionando)
  - BusUnHilo en GPIO23 (ESP32) o GPIO4 = D2 (ESP8266), con pull-up externa UNICA de 4k7 a 3,3 V
  - Esclavos: 0x01 puerta (MAG), 0x02 movimiento (PIR), 0x03 UNO (DHT11 + 4 reles + 4 pulsadores)
  - Dashboard: sensores con estado de conexion, humedad (indicador circular),
    temperatura de las ultimas 24 h (grafica de lineas) y reles
  - El historial se guarda en la RAM del ESP32 (una muestra cada 10 min = 24 h) y se pierde al reiniciar.
    La hora la pone el navegador: el ESP32 no necesita internet ni NTP.

  Libreria: en BusUnHilo.h conviene activar BUH_ESP_REGISTROS (acceso directo a GPIO; ESP8266: GPIO 0..15).
  Si da errores de comunicacion, probar en cambio BUH_MODO_LENTO. Es un solo archivo para todas las placas.
  ESP8266: no usar GPIO0, GPIO2, GPIO15 (arranque) ni GPIO16 (no sirve para el bus).
*/

#include <Arduino.h>
#include <BusUnHilo.h>
#if defined(ESP8266)
  #include <ESP8266WiFi.h>
  #include <ESP8266WebServer.h>
  ESP8266WebServer server(80);
#else
  #include <WiFi.h>
  #include <WebServer.h>
  WebServer server(80);
#endif
#include <ArduinoOTA.h>

// ============================ CONFIGURACION ============================
const char* ssid = "------------";     // <<< nombre de tu red
const char* password = "----------";  // <<< clave de tu red
const char* otaPassword = "CAMBIAME";          // <<< clave para actualizar por OTA

IPAddress local_IP(192, 168, 1, 200);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(8, 8, 8, 8);

#if defined(ESP8266)
const uint8_t PIN_BUS_MASTER = 4;     // GPIO4 = D2 (NodeMCU / Wemos D1 mini): bus con pull-up externa 4k7 a 3,3 V
#else
const uint8_t PIN_BUS_MASTER = 23;    // GPIO23: bus con pull-up externa 4k7 a 3,3 V
#endif

const uint8_t DIR_MAG = 0x01, DIR_PIR = 0x02, DIR_UNO = 0x03;     // direcciones de los esclavos
const uint8_t REG_SENSOR = 0x00;                // MAG y PIR: 1 = activo
const uint8_t REG_UNO_RELES_ORDEN = 0x00;       // UNO escritura: mascara de reles
const uint8_t REG_UNO_RELES_ESTADO = 0x02;      // UNO lectura: estado real de los reles

const unsigned long INTERVALO_SONDEO_MS = 500;  // cada cuanto se consultan los esclavos
const unsigned long OFFLINE_MS = 3000;          // sin respuesta por este tiempo = "sin conexion"
const unsigned long MUESTRA_CADA_MS = 600000UL; // una muestra cada 10 min (144 muestras = 24 h). Para probar: 10000
const uint16_t MAX_MUESTRAS = 144;
const bool DEBUG_SERIE = true;
// ======================================================================

const uint8_t DATO_INVALIDO = 0xFF;             // el UNO manda 0xFF si el DHT11 todavia no leyo

BusUnHiloMaster busMaster;
struct Esclavo {
  const char* nombre;
  unsigned long tOk;   // ultima respuesta correcta (0 = nunca)
  bool online;
};
Esclavo eMag = {"MAG 0x01", 0, false};
Esclavo ePir = {"PIR 0x02", 0, false};
Esclavo eUno = {"UNO 0x03", 0, false};

struct {
  uint8_t puertaAbierta = 0;
  uint8_t movimiento = 0;
  uint8_t temp = DATO_INVALIDO;
  uint8_t hum = DATO_INVALIDO;
  uint8_t reles = 0;
} sistema;

struct Muestra { uint32_t ms; uint8_t temp; uint8_t hum; };
Muestra muestras[MAX_MUESTRAS];
uint16_t nMuestras = 0, cabeza = 0;
unsigned long tUltimaMuestra = 0;

unsigned long tSondeo = 0, tError = 0, tWifi = 0;

// ---------------------------------------------------------------- utilidades
bool estaOnline(const Esclavo &e) { return e.tOk != 0 && (millis() - e.tOk) < OFFLINE_MS; }

void avisarError(const char* quien) {
  if (!DEBUG_SERIE || millis() - tError < 2000) return;   // no saturar el monitor serie
  tError = millis();
  Serial.print(F("[BUS] "));
  Serial.print(quien);
  Serial.print(F(": "));
  Serial.println(BusUnHiloMaster::errorText(busMaster.lastError()));
}

void revisarConexion(Esclavo &e) {
  bool on = estaOnline(e);
  if (on != e.online) {
    e.online = on;
    if (DEBUG_SERIE) Serial.printf("[BUS] %s: %s\n", e.nombre, on ? "CONECTADO" : "SIN CONEXION");
  }
}

// ---------------------------------------------------------------- reles
// Lee el estado REAL del UNO, cambia un bit y lo escribe: asi no pisa lo que se haya tocado en su tablero
bool conmutarRele(uint8_t i) {
  uint8_t actual;
  if (!busMaster.read(DIR_UNO, REG_UNO_RELES_ESTADO, actual)) { avisarError("UNO leyendo reles"); return false; }
  uint8_t nueva = (actual ^ (1 << i)) & 0x0F;
  if (!busMaster.write(DIR_UNO, REG_UNO_RELES_ORDEN, nueva)) { avisarError("UNO escribiendo reles"); return false; }
  sistema.reles = nueva;
  eUno.tOk = millis();
  if (DEBUG_SERIE) Serial.printf("[RELES] mascara = 0x%02X\n", nueva);
  return true;
}

// ---------------------------------------------------------------- historial
void guardarMuestra() {
  if (!estaOnline(eUno) || sistema.temp == DATO_INVALIDO || sistema.hum == DATO_INVALIDO) return;
  unsigned long ahora = millis();
  if (nMuestras > 0 && ahora - tUltimaMuestra < MUESTRA_CADA_MS) return;
  tUltimaMuestra = ahora;
  muestras[cabeza] = {(uint32_t)ahora, sistema.temp, sistema.hum};
  cabeza = (cabeza + 1) % MAX_MUESTRAS;
  if (nMuestras < MAX_MUESTRAS) nMuestras++;
}

// ---------------------------------------------------------------- sondeo del bus
void sondear() {
  uint8_t v;
  if (busMaster.read(DIR_MAG, REG_SENSOR, v)) { sistema.puertaAbierta = v ? 1 : 0; eMag.tOk = millis(); }
  else avisarError("MAG");

  if (busMaster.read(DIR_PIR, REG_SENSOR, v)) { sistema.movimiento = v ? 1 : 0; ePir.tOk = millis(); }
  else avisarError("PIR");

  uint8_t d[4];                                   // temp, hum, estado de reles, pulsadores
  if (busMaster.readBlock(DIR_UNO, 0x00, d, 4)) {
    sistema.temp = d[0];
    sistema.hum = d[1];
    sistema.reles = d[2] & 0x0F;                  // el estado real manda (incluye los pulsadores del UNO)
    eUno.tOk = millis();
  } else avisarError("UNO");

  revisarConexion(eMag);
  revisarConexion(ePir);
  revisarConexion(eUno);
  guardarMuestra();
}

// ---------------------------------------------------------------- pagina web
const char index_html[] PROGMEM = R"raw(
<!DOCTYPE html>
<html lang="es">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Panel Domotico BusUnHilo</title>
<style>
:root{
  --bg-color:#f1f5f9; --card-bg:#ffffff; --text-primary:#0f172a; --text-secondary:#64748b;
  --border-color:#e2e8f0; --accent:#2563eb; --danger:#dc2626; --danger-bg:#fef2f2;
  --success:#16a34a; --success-bg:#f0fdf4; --warn:#d97706; --warn-bg:#fffbeb;
  --shadow:0 4px 6px -1px rgba(0,0,0,.05),0 2px 4px -1px rgba(0,0,0,.03);
}
*{box-sizing:border-box}
body{font-family:'Segoe UI',system-ui,-apple-system,sans-serif;background:var(--bg-color);color:var(--text-primary);
  margin:0;padding:24px 16px;display:flex;flex-direction:column;align-items:center}
header{text-align:center;margin-bottom:20px}
h1{margin:0 0 6px 0;font-size:1.75rem;font-weight:700}
.subtitle{color:var(--text-secondary);margin:0;font-size:.95rem}
.grid-container{display:grid;grid-template-columns:repeat(auto-fit,minmax(300px,1fr));gap:20px;width:100%;max-width:900px}
.card{background:var(--card-bg);border:1px solid var(--border-color);border-radius:16px;padding:20px;
  box-shadow:var(--shadow);display:flex;flex-direction:column;gap:16px}
.card.wide{grid-column:1/-1}
.card h2{margin:0;font-size:1.1rem;font-weight:600;border-bottom:1px solid var(--border-color);padding-bottom:12px;
  color:var(--accent);display:flex;justify-content:space-between;align-items:baseline;gap:8px;flex-wrap:wrap}
.card h2 small{font-weight:500;color:var(--text-secondary);font-size:.8rem}
.metric-row{display:flex;justify-content:space-between;align-items:center;font-size:1rem}
.badge{padding:4px 12px;border-radius:20px;font-weight:600;font-size:.8rem;text-transform:uppercase;letter-spacing:.5px}
.badge.ok{background:var(--success-bg);color:var(--success);border:1px solid #bbf7d0}
.badge.alert{background:var(--danger-bg);color:var(--danger);border:1px solid #fecaca}
.badge.off{background:#f8fafc;color:var(--text-secondary);border:1px solid var(--border-color)}
.badge.warn{background:var(--warn-bg);color:var(--warn);border:1px solid #fde68a}
.value-display{font-weight:700;font-size:1.1rem}
.gauge-box{display:flex;flex-direction:column;align-items:center;gap:6px}
.gauge{width:170px;height:170px}
.g-bg{fill:none;stroke:#e2e8f0;stroke-width:12}
.g-arc{fill:none;stroke:var(--accent);stroke-width:12;stroke-linecap:round;stroke-dasharray:314.16;
  stroke-dashoffset:314.16;transition:stroke-dashoffset .6s ease,stroke .3s}
.g-val{font-size:26px;font-weight:700;fill:var(--text-primary)}
.g-lbl{font-size:9px;fill:var(--text-secondary)}
.g-text{color:var(--text-secondary);font-size:.9rem;min-height:1.2em}
#cvTemp{width:100%;height:250px;display:block}
.relay-grid{display:grid;grid-template-columns:repeat(2,1fr);gap:12px}
.btn-relay{background:#f8fafc;color:var(--text-primary);border:1px solid var(--border-color);padding:14px;
  border-radius:10px;cursor:pointer;font-weight:600;font-size:.95rem;transition:all .2s ease;box-shadow:0 1px 2px rgba(0,0,0,.05)}
.btn-relay.active{background:var(--success-bg);color:var(--success);border-color:#86efac}
.btn-relay:hover{border-color:var(--accent);transform:translateY(-1px)}
.btn-relay:disabled{opacity:.5;cursor:not-allowed;transform:none}
.alarm-banner{grid-column:1/-1;background:var(--danger-bg);border:2px solid var(--danger);color:var(--danger);
  padding:16px;border-radius:12px;text-align:center;font-weight:700;font-size:1.1rem;display:none;box-shadow:var(--shadow)}
.aviso{display:none;margin-top:4px;color:var(--danger);font-size:.9rem;font-weight:600}
.pie{margin-top:18px;color:var(--text-secondary);font-size:.85rem}
</style>
</head>
<body>
<header>
  <h1>Panel Domotico Central</h1>
  <p class="subtitle">Arquitectura BusUnHilo &bull; ESP32 Master</p>
</header>
<div id="alarmBanner" class="alarm-banner">ALERTA: ALARMA DISPARADA! INTRUSION DETECTADA</div>
<div class="grid-container">

  <div class="card">
    <h2>Sensores Perif&eacute;ricos</h2>
    <div class="metric-row"><span>Puerta / Ventana (MAG):</span><span id="stMag" class="badge off">Cargando...</span></div>
    <div class="metric-row"><span>Movimiento (PIR):</span><span id="stPir" class="badge off">Cargando...</span></div>
    <div class="metric-row"><span>Temperatura (DHT):</span><span id="valTemp" class="value-display">-- &deg;C</span></div>
    <div class="metric-row"><span>Humedad (DHT):</span><span id="valHum" class="value-display">-- %</span></div>
  </div>

  <div class="card">
    <h2>Humedad</h2>
    <div class="gauge-box">
      <svg class="gauge" viewBox="0 0 120 120" role="img" aria-label="Humedad relativa">
        <circle class="g-bg" cx="60" cy="60" r="50"/>
        <circle id="gArc" class="g-arc" cx="60" cy="60" r="50" transform="rotate(-90 60 60)"/>
        <text id="gVal" class="g-val" x="60" y="62" text-anchor="middle">--</text>
        <text class="g-lbl" x="60" y="78" text-anchor="middle">% humedad</text>
      </svg>
      <div id="gTxt" class="g-text">Sin datos</div>
    </div>
  </div>

  <div class="card wide">
    <h2>Temperatura &uacute;ltimas 24 h <small id="minmax"></small></h2>
    <canvas id="cvTemp"></canvas>
  </div>

  <div class="card wide">
    <h2>Control de Rel&eacute;s / Luces</h2>
    <div class="relay-grid">
      <button id="r0" class="btn-relay" onclick="toggleRelay(0)">Rel&eacute; 1 [OFF]</button>
      <button id="r1" class="btn-relay" onclick="toggleRelay(1)">Rel&eacute; 2 [OFF]</button>
      <button id="r2" class="btn-relay" onclick="toggleRelay(2)">Rel&eacute; 3 [OFF]</button>
      <button id="r3" class="btn-relay" onclick="toggleRelay(3)">Rel&eacute; 4 [OFF]</button>
    </div>
    <div id="aviso" class="aviso"></div>
  </div>
</div>
<div class="pie" id="pie">Conectando...</div>

<script>
let hist = null;
const pad = n => String(n).padStart(2, '0');
const css = v => getComputedStyle(document.documentElement).getPropertyValue(v).trim();

function badge(id, online, txt, cls) {
  const e = document.getElementById(id);
  if (!online) { e.textContent = 'Sin conexión'; e.className = 'badge warn'; }
  else { e.textContent = txt; e.className = 'badge ' + cls; }
}

function gauge(h, ok) {
  const arc = document.getElementById('gArc'), val = document.getElementById('gVal'), txt = document.getElementById('gTxt');
  if (!ok) { arc.style.strokeDashoffset = 314.16; val.textContent = '--'; txt.textContent = 'Sin datos'; return; }
  arc.style.strokeDashoffset = 314.16 * (1 - Math.max(0, Math.min(100, h)) / 100);
  val.textContent = h;
  let color = css('--accent'), t = 'Confortable';
  if (h < 30) { color = css('--warn'); t = 'Ambiente seco'; }
  else if (h > 60) { color = css('--accent'); t = 'Ambiente húmedo'; }
  else { color = css('--success'); }
  arc.style.stroke = color;
  txt.textContent = t;
}

async function actualizarEstado() {
  try {
    const r = await fetch('/estado');
    const d = await r.json();
    badge('stMag', d.mag_on, d.puerta ? 'Abierta' : 'Cerrada', d.puerta ? 'alert' : 'ok');
    badge('stPir', d.pir_on, d.pir ? 'Movimiento' : 'Reposo', d.pir ? 'alert' : 'off');
    document.getElementById('valTemp').textContent = (d.uno_on && d.dht) ? d.temp + ' °C' : '-- °C';
    document.getElementById('valHum').textContent = (d.uno_on && d.dht) ? d.hum + ' %' : '-- %';
    gauge(d.hum, d.uno_on && d.dht);
    for (let i = 0; i < 4; i++) {
      const b = document.getElementById('r' + i), on = (d.reles & (1 << i)) !== 0;
      b.textContent = 'Relé ' + (i + 1) + ' [' + (d.uno_on ? (on ? 'ON' : 'OFF') : 'sin conexión') + ']';
      b.disabled = !d.uno_on;
      b.classList.toggle('active', d.uno_on && on);
    }
    document.getElementById('pie').textContent = 'Actualizado ' + new Date().toLocaleTimeString('es-AR', {hour12: false});
  } catch (e) {
    document.getElementById('pie').textContent = 'Sin conexión con el ESP32...';
  }
}

async function actualizarHistorial() {
  try { hist = await (await fetch('/historial')).json(); dibujarTemp(); } catch (e) {}
}

function toggleRelay(i) {
  fetch('/api/relay?idx=' + i).then(r => {
    const av = document.getElementById('aviso');
    if (!r.ok) { av.textContent = 'El esclavo 0x03 no respondió. Revisá el cableado del bus.'; av.style.display = 'block'; }
    else av.style.display = 'none';
    actualizarEstado();
  }).catch(() => {});
}

function dibujarTemp() {
  const cv = document.getElementById('cvTemp');
  const dpr = window.devicePixelRatio || 1, W = cv.clientWidth, H = cv.clientHeight;
  cv.width = W * dpr; cv.height = H * dpr;
  const c = cv.getContext('2d'); c.setTransform(dpr, 0, 0, dpr, 0, 0); c.clearRect(0, 0, W, H);
  const L = 42, R = 14, T = 14, B = 30, w = W - L - R, h = H - T - B;
  const now = Date.now(), t0 = now - 24 * 3600e3;
  const pts = (hist ? hist.m : []).map(m => ({ x: now - ((hist.ahora - m[0]) >>> 0), y: m[1] })).filter(p => p.x >= t0);
  const txt = css('--text-secondary'), grid = css('--border-color'), acc = css('--accent');
  c.font = '12px "Segoe UI", system-ui, sans-serif';

  let lo = 15, hi = 35;
  if (pts.length) {
    const ys = pts.map(p => p.y);
    lo = Math.floor((Math.min(...ys) - 2) / 2) * 2; hi = Math.ceil((Math.max(...ys) + 2) / 2) * 2;
    if (hi - lo < 8) { const m = (hi + lo) / 2; lo = Math.floor(m - 4); hi = lo + 8; }
    hi = lo + Math.ceil((hi - lo) / 4) * 4;          // 4 divisiones de valor entero
    document.getElementById('minmax').textContent = 'Mín ' + Math.min(...ys) + ' °C · Máx ' + Math.max(...ys) + ' °C';
  } else document.getElementById('minmax').textContent = '';
  const X = t => L + (t - t0) / (now - t0) * w, Y = v => T + (1 - (v - lo) / (hi - lo)) * h;

  c.strokeStyle = grid; c.fillStyle = txt; c.lineWidth = 1; c.textAlign = 'right'; c.textBaseline = 'middle';
  const paso = (hi - lo) / 4;
  for (let k = 0; k <= 4; k++) {
    const v = lo + paso * k, y = Y(v);
    c.beginPath(); c.moveTo(L, y); c.lineTo(L + w, y); c.stroke();
    c.fillText(Math.round(v * 10) / 10 + '°', L - 6, y);
  }
  c.textAlign = 'center'; c.textBaseline = 'top';
  let d0 = new Date(t0); d0.setMinutes(0, 0, 0); d0.setHours(d0.getHours() + 1);
  while (d0.getHours() % 4 !== 0) d0.setHours(d0.getHours() + 1);
  for (let t = d0.getTime(); t <= now; t += 4 * 3600e3) {
    const x = X(t);
    c.beginPath(); c.moveTo(x, T); c.lineTo(x, T + h); c.stroke();
    c.fillText(pad(new Date(t).getHours()) + ':00', x, T + h + 8);
  }
  if (!pts.length) {
    c.fillStyle = txt; c.textBaseline = 'middle';
    c.fillText('Recopilando datos... (una muestra cada ' + (hist ? Math.round(hist.cada / 60000) : 10) + ' min)', L + w / 2, T + h / 2);
    return;
  }
  const g = c.createLinearGradient(0, T, 0, T + h); g.addColorStop(0, 'rgba(37,99,235,.18)'); g.addColorStop(1, 'rgba(37,99,235,0)');
  c.beginPath(); c.moveTo(X(pts[0].x), T + h);
  pts.forEach(p => c.lineTo(X(p.x), Y(p.y)));
  c.lineTo(X(pts[pts.length - 1].x), T + h); c.closePath(); c.fillStyle = g; c.fill();
  c.beginPath(); pts.forEach((p, i) => i ? c.lineTo(X(p.x), Y(p.y)) : c.moveTo(X(p.x), Y(p.y)));
  c.strokeStyle = acc; c.lineWidth = 2.5; c.lineJoin = 'round'; c.stroke();
  const u = pts[pts.length - 1];
  c.fillStyle = acc; c.beginPath(); c.arc(X(u.x), Y(u.y), 4.5, 0, 7); c.fill();
  c.textAlign = 'right'; c.textBaseline = 'bottom'; c.font = 'bold 13px "Segoe UI", system-ui, sans-serif';
  c.fillText(u.y + ' °C', X(u.x) - 8, Y(u.y) - 6);
}

window.addEventListener('resize', dibujarTemp);
actualizarEstado(); actualizarHistorial();
setInterval(actualizarEstado, 1000);
setInterval(actualizarHistorial, 30000);
</script>
</body>
</html>
)raw";

void handleRoot() {
#if defined(ESP8266)
  server.send_P(200, "text/html", index_html);   // en el ESP8266 la pagina esta en flash
#else
  server.send(200, "text/html", index_html);
#endif
}

void handleEstado() {
  char buf[200];
  snprintf(buf, sizeof(buf),
    "{\"puerta\":%u,\"pir\":%u,\"temp\":%u,\"hum\":%u,\"dht\":%s,\"reles\":%u,"
    "\"mag_on\":%s,\"pir_on\":%s,\"uno_on\":%s}",
    sistema.puertaAbierta, sistema.movimiento, sistema.temp, sistema.hum,
    (sistema.temp != DATO_INVALIDO && sistema.hum != DATO_INVALIDO) ? "true" : "false", sistema.reles,
    estaOnline(eMag) ? "true" : "false", estaOnline(ePir) ? "true" : "false", estaOnline(eUno) ? "true" : "false");
  server.send(200, "application/json", buf);
}

void handleHistorial() {
  String json;
  json.reserve(60 + nMuestras * 22);
  char tmp[40];
  snprintf(tmp, sizeof(tmp), "{\"ahora\":%lu,\"cada\":%lu,\"m\":[", (unsigned long)millis(), MUESTRA_CADA_MS);
  json += tmp;
  uint16_t inicio = (nMuestras < MAX_MUESTRAS) ? 0 : cabeza;       // la mas vieja primero
  for (uint16_t k = 0; k < nMuestras; k++) {
    const Muestra &m = muestras[(inicio + k) % MAX_MUESTRAS];
    snprintf(tmp, sizeof(tmp), "%s[%lu,%u,%u]", k ? "," : "", (unsigned long)m.ms, m.temp, m.hum);
    json += tmp;
  }
  json += "]}";
  server.send(200, "application/json", json);
}

void handleRelay() {
  if (!server.hasArg("idx")) { server.send(400, "text/plain", "falta idx"); return; }
  int idx = server.arg("idx").toInt();
  if (idx < 0 || idx > 3) { server.send(400, "text/plain", "idx invalido"); return; }
  if (conmutarRele(idx)) server.send(200, "text/plain", "OK");
  else server.send(503, "text/plain", "el esclavo 0x03 no responde");
}

// ---------------------------------------------------------------- setup / loop
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println(F("\n[MAESTRO] Iniciando..."));

  busMaster.begin(PIN_BUS_MASTER, false);        // pull-up EXTERNA
  busMaster.setRetries(3);

  WiFi.mode(WIFI_STA);
  WiFi.config(local_IP, gateway, subnet, primaryDNS);
  WiFi.begin(ssid, password);
  Serial.print(F("[WIFI] Conectando"));
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) { delay(250); Serial.print("."); }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print(F("\n[WIFI] Conectado. IP: "));
    Serial.println(WiFi.localIP());
  } else {
    Serial.println(F("\n[WIFI] Sin WiFi por ahora: el bus y los pulsadores funcionan igual; se reintenta cada 10 s"));
  }

  ArduinoOTA.setHostname("esp32-domotica");
  ArduinoOTA.setPassword(otaPassword);
  ArduinoOTA.begin();

  server.on("/", handleRoot);
  server.on("/estado", handleEstado);
  server.on("/historial", handleHistorial);
  server.on("/api/relay", handleRelay);
  server.begin();
  Serial.println(F("[WEB] Servidor HTTP listo"));
}

void loop() {
  ArduinoOTA.handle();
  server.handleClient();

  if (millis() - tSondeo >= INTERVALO_SONDEO_MS) {
    tSondeo = millis();
    sondear();
  }

  if (WiFi.status() != WL_CONNECTED && millis() - tWifi > 10000) {
    tWifi = millis();
    WiFi.reconnect();
  }
}

#include <WiFi.h>
#include <ArduinoOTA.h>
#include <WebServer.h>
#include <Preferences.h>

#define pinPressostato 18
#define pinBomba 35

const char *ssid = "linksysExterno";
const char *password = "f1a2c3a415";

unsigned long tempoMinimoLow    = 2000;
unsigned long tempoMinimoHigh   = 2000;
unsigned long tempoMaximoLigado = 120000;
unsigned long cooldown          = 2000;

bool bombaLigada    = false;
bool falhaSeguranca = false;

unsigned long tempoEstadoPressostato  = 0;
unsigned long tempoBombaLigada        = 0;
unsigned long tempoUltimoDesligamento = 0;

Preferences prefs;
WebServer server(80);

// ─── HTML page ────────────────────────────────────────────────────────────────
const char PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Pump Controller</title>
<style>
  *{box-sizing:border-box;margin:0;padding:0}
  body{font-family:system-ui,sans-serif;background:#0f172a;color:#e2e8f0;padding:20px}
  h1{font-size:1.25rem;font-weight:700;margin-bottom:20px;color:#f8fafc}
  .grid{display:grid;grid-template-columns:1fr 1fr;gap:12px;margin-bottom:20px}
  .card{background:#1e293b;border-radius:10px;padding:16px}
  .card h2{font-size:.7rem;text-transform:uppercase;letter-spacing:.08em;color:#94a3b8;margin-bottom:8px}
  .badge{display:inline-block;padding:4px 12px;border-radius:99px;font-weight:700;font-size:.95rem}
  .on  {background:#15803d;color:#dcfce7}
  .off {background:#334155;color:#94a3b8}
  .fault{background:#991b1b;color:#fee2e2}
  .ok  {background:#1e3a5f;color:#bae6fd}
  .subtext{font-size:.75rem;color:#64748b;margin-top:6px}
  .config{background:#1e293b;border-radius:10px;padding:16px;margin-bottom:12px}
  .config h2{font-size:.7rem;text-transform:uppercase;letter-spacing:.08em;color:#94a3b8;margin-bottom:14px}
  .row{display:flex;align-items:center;justify-content:space-between;margin-bottom:10px;gap:8px}
  .row label{font-size:.85rem;color:#cbd5e1;flex:1}
  .row input{width:90px;background:#0f172a;border:1px solid #334155;color:#f1f5f9;
             border-radius:6px;padding:5px 8px;font-size:.9rem;text-align:right}
  .row span{font-size:.75rem;color:#64748b;width:24px}
  .actions{display:flex;gap:10px}
  button{flex:1;padding:10px;border:none;border-radius:8px;font-weight:600;
         font-size:.9rem;cursor:pointer;transition:opacity .15s}
  button:active{opacity:.7}
  #btnSave  {background:#2563eb;color:#fff}
  #btnReset {background:#1e3a5f;color:#bae6fd}
  #btnLock  {background:#7c2d12;color:#fed7aa}
  #msg{margin-top:10px;font-size:.8rem;color:#4ade80;min-height:1.2em;text-align:center}
  .uptime{font-size:.75rem;color:#64748b;text-align:right;margin-top:14px}
  .failsafe-actions{display:flex;gap:10px;margin-top:12px}
</style>
</head>
<body>
<h1>⚙ Pump Controller</h1>

<div class="grid">
  <div class="card">
    <h2>Pump</h2>
    <span id="sBomba" class="badge off">—</span>
    <div class="subtext" id="sPumpOn"></div>
  </div>
  <div class="card">
    <h2>Pressostat</h2>
    <span id="sPress" class="badge off">—</span>
  </div>
  <div class="card" style="grid-column:1/-1">
    <h2>Safety Failsafe</h2>
    <span id="sFalha" class="badge ok">—</span>
    <div class="failsafe-actions">
      <button id="btnReset" onclick="resetFault()">Reset Failsafe</button>
      <button id="btnLock"  onclick="triggerFault()">Trigger Failsafe</button>
    </div>
  </div>
</div>

<div class="config">
  <h2>Timing (milliseconds)</h2>
  <div class="row">
    <label>Min LOW before start</label>
    <input id="cLow"      type="number" min="0">
    <span>ms</span>
  </div>
  <div class="row">
    <label>Min HIGH before stop</label>
    <input id="cHigh"     type="number" min="0">
    <span>ms</span>
  </div>
  <div class="row">
    <label>Max ON (safety timeout)</label>
    <input id="cMax"      type="number" min="0">
    <span>ms</span>
  </div>
  <div class="row">
    <label>Cooldown between cycles</label>
    <input id="cCooldown" type="number" min="0">
    <span>ms</span>
  </div>
  <div class="actions">
    <button id="btnSave" onclick="saveConfig()">Save Config</button>
  </div>
  <div id="msg"></div>
</div>

<div class="uptime">Uptime: <span id="uptime">—</span></div>

<script>
function fmt(ms){
  const s=Math.floor(ms/1000), m=Math.floor(s/60), h=Math.floor(m/60);
  return h?h+'h '+(m%60)+'m':m?m+'m '+(s%60)+'s':s+'s';
}

async function poll(){
  try{
    const r=await fetch('/status');
    const d=await r.json();

    const b=document.getElementById('sBomba');
    b.textContent=d.bomba?'ON':'OFF';
    b.className='badge '+(d.bomba?'on':'off');

    const po=document.getElementById('sPumpOn');
    po.textContent=d.bomba?'Running for '+fmt(d.pumpon):'';

    const p=document.getElementById('sPress');
    p.textContent=d.pressostato?'HIGH':'LOW';
    p.className='badge '+(d.pressostato?'on':'off');

    const f=document.getElementById('sFalha');
    f.textContent=d.falha?'FAULT — pump locked':'OK';
    f.className='badge '+(d.falha?'fault':'ok');

    document.getElementById('uptime').textContent=fmt(d.uptime);

    if(!document.getElementById('cLow').value){
      document.getElementById('cLow').value      = d.cfg.low;
      document.getElementById('cHigh').value     = d.cfg.high;
      document.getElementById('cMax').value      = d.cfg.max;
      document.getElementById('cCooldown').value = d.cfg.cooldown;
    }
  }catch(e){}
}

async function saveConfig(){
  const body=new URLSearchParams({
    low:      document.getElementById('cLow').value,
    high:     document.getElementById('cHigh').value,
    max:      document.getElementById('cMax').value,
    cooldown: document.getElementById('cCooldown').value
  });
  const r=await fetch('/config',{method:'POST',body});
  showMsg(r.ok?'✓ Saved':'✗ Error');
}

async function resetFault(){
  const r=await fetch('/reset',{method:'POST'});
  showMsg(r.ok?'✓ Failsafe cleared':'✗ Error');
}

async function triggerFault(){
  const r=await fetch('/lock',{method:'POST'});
  showMsg(r.ok?'Failsafe triggered':'✗ Error');
}

function showMsg(t){
  const el=document.getElementById('msg');
  el.textContent=t;
  setTimeout(()=>el.textContent='',3000);
}

poll();
setInterval(poll,2000);
</script>
</body>
</html>
)rawliteral";

// ─── Handlers ─────────────────────────────────────────────────────────────────
void handleRoot() {
  server.send_P(200, "text/html", PAGE);
}

void handleStatus() {
  int pressostato = digitalRead(pinPressostato);
  unsigned long pumpon = (bombaLigada && tempoBombaLigada > 0)
                         ? (millis() - tempoBombaLigada)
                         : 0;
  String json = "{";
  json += "\"bomba\":"       + String(bombaLigada    ? "true" : "false") + ",";
  json += "\"pumpon\":"      + String(pumpon)                            + ",";
  json += "\"pressostato\":" + String(pressostato == HIGH ? "true" : "false") + ",";
  json += "\"falha\":"       + String(falhaSeguranca ? "true" : "false") + ",";
  json += "\"uptime\":"      + String(millis())                          + ",";
  json += "\"cfg\":{";
  json += "\"low\":"      + String(tempoMinimoLow)    + ",";
  json += "\"high\":"     + String(tempoMinimoHigh)   + ",";
  json += "\"max\":"      + String(tempoMaximoLigado) + ",";
  json += "\"cooldown\":" + String(cooldown);
  json += "}}";
  server.send(200, "application/json", json);
}

void handleConfig() {
  if (server.hasArg("low"))      tempoMinimoLow    = server.arg("low").toInt();
  if (server.hasArg("high"))     tempoMinimoHigh   = server.arg("high").toInt();
  if (server.hasArg("max"))      tempoMaximoLigado = server.arg("max").toInt();
  if (server.hasArg("cooldown")) cooldown          = server.arg("cooldown").toInt();

  prefs.begin("pump", false);
  prefs.putULong("tLow",  tempoMinimoLow);
  prefs.putULong("tHigh", tempoMinimoHigh);
  prefs.putULong("tMax",  tempoMaximoLigado);
  prefs.putULong("tCool", cooldown);
  prefs.end();

  server.send(200, "text/plain", "ok");
}

// ─── Setup ────────────────────────────────────────────────────────────────────
void setup() {
  prefs.begin("pump", false);
  tempoMinimoLow    = prefs.getULong("tLow",  1000);
  tempoMinimoHigh   = prefs.getULong("tHigh", 1000);
  tempoMaximoLigado = prefs.getULong("tMax",  120000);
  cooldown          = prefs.getULong("tCool", 2000);
  prefs.end();

  WiFi.begin(ssid, password);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
    delay(100);
  }

  if (WiFi.status() == WL_CONNECTED) {
    ArduinoOTA.setPassword(nullptr);
    ArduinoOTA.begin();

    server.on("/",       HTTP_GET,  handleRoot);
    server.on("/status", HTTP_GET,  handleStatus);
    server.on("/config", HTTP_POST, handleConfig);
    server.on("/reset",  HTTP_POST, handleReset);
    server.on("/lock",   HTTP_POST, handleLock);
    server.begin();
  }

  pinMode(pinPressostato, INPUT_PULLUP);
  pinMode(pinBomba, OUTPUT);
  digitalWrite(pinBomba, LOW);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
}

// ─── Loop ─────────────────────────────────────────────────────────────────────
void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    ArduinoOTA.handle();
    server.handleClient();
  }

  if (falhaSeguranca) {
    desligarBomba();
    return;
  }

  static int estadoAnterior = digitalRead(pinPressostato);
  int estadoAtual = digitalRead(pinPressostato);
  unsigned long agora = millis();

  if (estadoAtual != estadoAnterior) {
    tempoEstadoPressostato = agora;
    estadoAnterior = estadoAtual;
  }

  if (bombaLigada && (agora - tempoBombaLigada >= tempoMaximoLigado)) {
    desligarBomba();
    falhaSeguranca = true;
    return;
  }

  if (bombaLigada) {
    if (estadoAtual == HIGH &&
        (agora - tempoEstadoPressostato >= tempoMinimoHigh)) {
      desligarBomba();
    }
  } else {
    if (estadoAtual == LOW &&
        (agora - tempoEstadoPressostato >= tempoMinimoLow) &&
        (agora - tempoUltimoDesligamento >= cooldown)) {
      ligarBomba();
    }
  }
}

// ─── Pump control ─────────────────────────────────────────────────────────────
void ligarBomba() {
  digitalWrite(pinBomba, HIGH);
  digitalWrite(LED_BUILTIN, HIGH);
  tempoBombaLigada = millis();
  bombaLigada = true;
}

void desligarBomba() {
  digitalWrite(pinBomba, LOW);
  digitalWrite(LED_BUILTIN, LOW);
  tempoUltimoDesligamento = millis();
  bombaLigada = false;
}

void handleReset() {
  falhaSeguranca = false;
  server.send(200, "text/plain", "ok");
}

void handleLock() {
  desligarBomba();
  falhaSeguranca = true;
  server.send(200, "text/plain", "ok");
}

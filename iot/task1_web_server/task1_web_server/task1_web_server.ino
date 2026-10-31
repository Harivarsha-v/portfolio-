/*
 * ======================================================================
 *  TASK 1 - ESP32 WEB SERVER: LED CONTROL AND BLINKING
 * ======================================================================
 *  The ESP32 joins the local WiFi network, starts an HTTP server on
 *  port 80 and serves a control page. The page's buttons call small
 *  HTTP routes that change the state of the on-board LED.
 *
 *  Routes
 *    /          control page (HTML)
 *    /on        LED solid ON
 *    /off       LED OFF
 *    /blink     LED blinks (non-blocking, so the server stays responsive)
 *    /toggle    flip between ON and OFF
 *    /status    JSON: LED state, uptime, WiFi signal, request count
 *
 *  Board    : ESP32 Dev Module   (on-board LED on GPIO2)
 *  Libraries: WiFi, WebServer, ESPmDNS   (all ship with the ESP32 core)
 * ======================================================================
 */

#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>

// ------------------------- user settings ---------------------------
const char* WIFI_SSID = "YOUR_WIFI_NAME";        // must be a 2.4 GHz network
const char* WIFI_PASS = "YOUR_WIFI_PASSWORD";
const char* HOSTNAME  = "esp32-led";             // reachable as http://esp32-led.local

const uint8_t       LED_PIN              = 2;    // on-board LED
const unsigned long BLINK_INTERVAL_MS    = 400;  // time between blink toggles
const unsigned long WIFI_TIMEOUT_MS      = 20000;
const unsigned long WIFI_RECHECK_MS      = 10000;

// ----------------------------- state -------------------------------
enum LedMode { MODE_OFF, MODE_ON, MODE_BLINK };

LedMode       ledMode       = MODE_OFF;
bool          ledLevel      = false;     // what the pin is currently driving
unsigned long lastBlinkAt   = 0;
unsigned long lastWifiCheck = 0;
unsigned long requestCount  = 0;

WebServer server(80);

// ------------------------- web page (HTML) -------------------------
const char PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Don't You Dare</title>
<style>
  * { box-sizing: border-box; }
  body { margin: 0; min-height: 100vh; color: #fff; font-family: Arial, Helvetica, sans-serif;
         background: radial-gradient(circle at center, #242424 0%, #111111 55%, #080808 100%);
         display: flex; flex-direction: column; align-items: center; justify-content: center; overflow: hidden; }
  .warning { position: absolute; top: 55px; width: 100%; text-align: center; text-transform: uppercase;
             font-family: Impact, Haettenschweiler, "Arial Narrow Bold", sans-serif;
             font-size: clamp(38px, 6vw, 78px); letter-spacing: 2px; text-shadow: 4px 4px 0 #000; }
  .subtitle { position: absolute; top: 150px; color: #888; font-size: 15px; letter-spacing: 3px; text-transform: uppercase; }
  .main-button { width: 220px; height: 220px; border-radius: 50%; border: 10px solid #ff3b3b; color: #fff; cursor: pointer;
                 background: radial-gradient(circle at 35% 30%, #ff5555, #d90000 55%, #8b0000 100%);
                 font-family: Impact, Haettenschweiler, sans-serif; font-size: 30px; letter-spacing: 2px;
                 box-shadow: 0 0 20px rgba(255,0,0,.9), 0 0 60px rgba(255,0,0,.45),
                             inset 0 10px 20px rgba(255,255,255,.15), inset 0 -15px 20px rgba(0,0,0,.35);
                 transition: transform .15s ease, box-shadow .15s ease; }
  .main-button:hover  { transform: scale(1.05); box-shadow: 0 0 30px rgba(255,0,0,1), 0 0 90px rgba(255,0,0,.6); }
  .main-button:active { transform: scale(.94); }
  .controls { display: flex; gap: 18px; margin-top: 45px; }
  .control { min-width: 170px; padding: 16px 28px; border-radius: 10px; font-size: 15px; font-weight: bold;
             letter-spacing: 1px; cursor: pointer; transition: transform .15s ease; }
  .control:hover { transform: translateY(-3px); }
  .on  { background: #1f9d55; border: 2px solid #39d77a; color: #fff; }
  .off { background: #252525; border: 2px solid #666;    color: #ddd; }
  .tag { position: absolute; bottom: 85px; color: #555; font-size: 11px; letter-spacing: 2px; text-transform: uppercase; }
  .status { position: absolute; bottom: 45px; display: flex; align-items: center; gap: 10px; color: #aaa; font-size: 14px; letter-spacing: 1px; }
  .light { width: 10px; height: 10px; border-radius: 50%; background: #39d77a; box-shadow: 0 0 10px #39d77a; }
  .light.blink { animation: pulse .8s infinite; }
  @keyframes pulse { 50% { opacity: .2; } }
</style>
</head>
<body>
  <div class="warning">DON'T YOU DARE PRESS THE BUTTON</div>
  <div class="subtitle" id="subtitle">You have been warned.</div>

  <button class="main-button" onclick="send('/blink', 'You pressed it. The LED is now blinking.')">PRESS ME</button>

  <div class="controls">
    <button class="control on"  onclick="send('/on',  'LED switched ON.')">TURN BULB ON</button>
    <button class="control off" onclick="send('/off', 'LED switched OFF.')">TURN BULB OFF</button>
  </div>

  <div class="tag">ESP32 web server - LED control</div>
  <div class="status"><div class="light" id="light"></div><span id="statusText">SYSTEM READY</span></div>

<script>
  const subtitle = document.getElementById('subtitle');
  const light    = document.getElementById('light');
  const label    = document.getElementById('statusText');

  function paint(s) {
    light.className   = 'light' + (s.mode === 'blink' ? ' blink' : '');
    light.style.background = s.led === 'on' ? '#39d77a' : '#555';
    label.textContent = 'LED ' + s.led.toUpperCase() + ' - ' + s.mode.toUpperCase() +
                        ' - WIFI ' + s.rssi + ' dBm';
  }
  async function refresh() {
    try { paint(await (await fetch('/status')).json()); }
    catch (e) { label.textContent = 'CONNECTION LOST'; }
  }
  async function send(path, message) {
    try { await fetch(path); subtitle.textContent = message; refresh(); }
    catch (e) { subtitle.textContent = 'Request failed - is the ESP32 still online?'; }
  }
  refresh();
  setInterval(refresh, 2000);
</script>
</body>
</html>
)rawliteral";

// ------------------------- LED helpers -----------------------------
const char* modeName(LedMode m) {
  switch (m) {
    case MODE_ON:    return "solid";
    case MODE_BLINK: return "blink";
    default:         return "off";
  }
}

void drive(bool level) {
  ledLevel = level;
  digitalWrite(LED_PIN, level ? HIGH : LOW);
}

void setMode(LedMode m) {
  ledMode = m;
  lastBlinkAt = millis();
  if (m == MODE_ON)  drive(true);
  if (m == MODE_OFF) drive(false);
  if (m == MODE_BLINK) drive(true);
  Serial.printf("[LED] mode -> %s\n", modeName(m));
}

// Called from loop(): blinks without ever calling delay(), so the web
// server keeps answering requests while the LED flashes.
void updateBlink() {
  if (ledMode != MODE_BLINK) return;
  if (millis() - lastBlinkAt >= BLINK_INTERVAL_MS) {
    lastBlinkAt = millis();
    drive(!ledLevel);
  }
}

// ------------------------- HTTP handlers ---------------------------
void sendState() {
  String json = "{\"led\":\"";
  json += ledLevel ? "on" : "off";
  json += "\",\"mode\":\"";
  json += modeName(ledMode);
  json += "\",\"uptime\":";
  json += millis() / 1000;
  json += ",\"rssi\":";
  json += WiFi.RSSI();
  json += ",\"requests\":";
  json += requestCount;
  json += "}";
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", json);
}

void handleRoot()   { requestCount++; server.send_P(200, "text/html", PAGE); }
void handleOn()     { requestCount++; setMode(MODE_ON);    sendState(); }
void handleOff()    { requestCount++; setMode(MODE_OFF);   sendState(); }
void handleBlink()  { requestCount++; setMode(MODE_BLINK); sendState(); }
void handleToggle() { requestCount++; setMode(ledLevel ? MODE_OFF : MODE_ON); sendState(); }
void handleStatus() { requestCount++; sendState(); }

void handleNotFound() {
  String msg = "404 - route not found: ";
  msg += server.uri();
  msg += "\n\nAvailable: /  /on  /off  /blink  /toggle  /status\n";
  server.send(404, "text/plain", msg);
}

// --------------------------- WiFi ----------------------------------
bool connectWiFi() {
  Serial.printf("[WiFi] connecting to \"%s\" ", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(HOSTNAME);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  unsigned long started = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (millis() - started > WIFI_TIMEOUT_MS) {
      Serial.println("\n[WiFi] timed out - check SSID/password and that the network is 2.4 GHz");
      return false;
    }
  }
  Serial.println();
  Serial.print("[WiFi] connected, IP address: ");
  Serial.println(WiFi.localIP());
  return true;
}

void checkWiFi() {
  if (millis() - lastWifiCheck < WIFI_RECHECK_MS) return;
  lastWifiCheck = millis();
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] connection lost - reconnecting");
    WiFi.disconnect();
    WiFi.reconnect();
  }
}

// ---------------------------- setup / loop -------------------------
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("=== Task 1 : ESP32 web server ===");

  pinMode(LED_PIN, OUTPUT);
  drive(false);

  connectWiFi();

  if (MDNS.begin(HOSTNAME)) {
    Serial.printf("[mDNS] http://%s.local\n", HOSTNAME);
  }

  server.on("/",        HTTP_GET, handleRoot);
  server.on("/on",      HTTP_GET, handleOn);
  server.on("/off",     HTTP_GET, handleOff);
  server.on("/blink",   HTTP_GET, handleBlink);
  server.on("/toggle",  HTTP_GET, handleToggle);
  server.on("/status",  HTTP_GET, handleStatus);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("[HTTP] server started on port 80");
}

void loop() {
  server.handleClient();   // answer any pending browser request
  updateBlink();           // advance the blink state machine
  checkWiFi();             // reconnect if the router dropped us
}

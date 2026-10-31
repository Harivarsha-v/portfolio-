/*
 * ======================================================================
 *  TASK 2 - ADAFRUIT IO DASHBOARD + MQTT RELAY CONTROL
 * ======================================================================
 *  The ESP32 is an MQTT *subscriber*. It stays connected to Adafruit IO's
 *  broker and listens to one feed. When the dashboard toggle publishes
 *  "ON" or "OFF" to that feed, the broker pushes the value to the ESP32
 *  and the callback switches the relay that drives the 230 V bulb.
 *
 *  Data flow
 *     Adafruit IO toggle --publish--> broker --push--> ESP32 --GPIO--> relay --> bulb
 *
 *  Wiring    : relay IN -> GPIO26, relay VCC -> 5V (VIN), relay GND -> GND
 *              bulb live wire through the relay COM and NO terminals
 *  Relay     : active-LOW module (energises when the input pin is LOW)
 *  Libraries : PubSubClient (Nick O'Leary)
 * ======================================================================
 */

#include <WiFi.h>
#include <PubSubClient.h>

// ------------------------- user settings ---------------------------
#define WIFI_SSID     "YOUR_WIFI_NAME"       // 2.4 GHz only
#define WIFI_PASS     "YOUR_WIFI_PASSWORD"

#define AIO_SERVER    "io.adafruit.com"
#define AIO_PORT      1883                   // 8883 = TLS (needs WiFiClientSecure)
#define AIO_USERNAME  "YOUR_AIO_USERNAME"
#define AIO_KEY       "YOUR_AIO_KEY"
#define FEED_KEY      "esp"                  // the feed the dashboard toggle writes to

const uint8_t RELAY_PIN = 26;
#define RELAY_ON  LOW                        // active-LOW relay module
#define RELAY_OFF HIGH

const unsigned long WIFI_TIMEOUT_MS   = 20000;
const unsigned long MQTT_RETRY_MS     = 5000;
const unsigned long HEARTBEAT_MS      = 30000;

// ------------------------------ MQTT -------------------------------
WiFiClient   wifiClient;
PubSubClient mqtt(wifiClient);

String topicFeed;       // <username>/feeds/<feed>
String topicGet;        // <username>/feeds/<feed>/get   (asks for the last value)
String clientId;

bool          relayOn         = false;
unsigned long lastMqttAttempt = 0;
unsigned long lastHeartbeat   = 0;

// Human readable meaning of PubSubClient's state codes. State -2 was the
// error that kept repeating in the serial monitor before the broker
// address was corrected.
const char* mqttStateText(int s) {
  switch (s) {
    case -4: return "connection timeout";
    case -3: return "connection lost";
    case -2: return "connect failed (cannot reach broker - check server and port)";
    case -1: return "disconnected";
    case  0: return "connected";
    case  1: return "bad protocol";
    case  2: return "bad client id";
    case  3: return "broker unavailable";
    case  4: return "bad username or key";
    case  5: return "not authorised";
    default: return "unknown";
  }
}

// ------------------------------ relay ------------------------------
void applyRelay(bool on) {
  relayOn = on;
  digitalWrite(RELAY_PIN, on ? RELAY_ON : RELAY_OFF);
  Serial.printf("[RELAY] %s\n", on ? "ON  (bulb lit)" : "OFF (bulb dark)");
}

// Accepts the values the Adafruit toggle sends ("ON"/"OFF") and a few
// common alternatives so the feed can also be driven by hand or by IFTTT.
bool parseCommand(String payload, bool &result) {
  payload.trim();
  payload.toUpperCase();
  if (payload == "ON"  || payload == "1" || payload == "TRUE")  { result = true;  return true; }
  if (payload == "OFF" || payload == "0" || payload == "FALSE") { result = false; return true; }
  return false;
}

// Runs every time the broker pushes a message on a subscribed topic.
void onMessage(char* topic, byte* payload, unsigned int length) {
  String text;
  text.reserve(length);
  for (unsigned int i = 0; i < length; i++) text += (char)payload[i];

  Serial.printf("[MQTT] %s -> \"%s\"\n", topic, text.c_str());

  bool wanted;
  if (parseCommand(text, wanted)) {
    applyRelay(wanted);
  } else {
    Serial.println("[MQTT] ignored: not ON/OFF");
  }
}

// ------------------------------ WiFi -------------------------------
bool connectWiFi() {
  Serial.printf("[WiFi] connecting to \"%s\" ", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  unsigned long started = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (millis() - started > WIFI_TIMEOUT_MS) {
      Serial.println("\n[WiFi] timed out");
      return false;
    }
  }
  Serial.print("\n[WiFi] connected, IP ");
  Serial.println(WiFi.localIP());
  return true;
}

// ----------------------------- MQTT --------------------------------
// Non-blocking reconnect: tries once every MQTT_RETRY_MS so loop() keeps
// running (and keeps the relay state) while the broker is unreachable.
void ensureMqtt() {
  if (mqtt.connected()) return;
  if (millis() - lastMqttAttempt < MQTT_RETRY_MS) return;
  lastMqttAttempt = millis();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] down - reconnecting");
    WiFi.reconnect();
    return;
  }

  Serial.print("[MQTT] connecting to broker... ");
  if (mqtt.connect(clientId.c_str(), AIO_USERNAME, AIO_KEY)) {
    Serial.println("connected");
    mqtt.subscribe(topicFeed.c_str());
    mqtt.subscribe(topicGet.c_str());
    // Publishing to <feed>/get makes Adafruit IO re-send the most recent
    // value, so the relay matches the dashboard after a reboot. Both topics
    // are subscribed so the reply is caught wherever the broker delivers it;
    // the empty message we publish is ignored by parseCommand().
    mqtt.publish(topicGet.c_str(), "");
  } else {
    int rc = mqtt.state();
    Serial.printf("failed, rc=%d (%s) - retrying in %lus\n", rc, mqttStateText(rc), MQTT_RETRY_MS / 1000);
  }
}

// -------------------------- setup / loop ---------------------------
void setup() {
  // Drive the relay to OFF *before* switching the pin to OUTPUT so the bulb
  // never flashes on during boot.
  digitalWrite(RELAY_PIN, RELAY_OFF);
  pinMode(RELAY_PIN, OUTPUT);

  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("=== Task 2 : Adafruit IO MQTT relay ===");

  topicFeed = String(AIO_USERNAME) + "/feeds/" + FEED_KEY;
  topicGet  = topicFeed + "/get";
  clientId  = "esp32-" + String((uint32_t)ESP.getEfuseMac(), HEX);

  connectWiFi();

  mqtt.setServer(AIO_SERVER, AIO_PORT);
  mqtt.setCallback(onMessage);
  mqtt.setKeepAlive(30);
  mqtt.setBufferSize(256);

  Serial.printf("[MQTT] broker %s:%d  feed %s\n", AIO_SERVER, AIO_PORT, topicFeed.c_str());
}

void loop() {
  ensureMqtt();
  mqtt.loop();             // keeps the connection alive and dispatches callbacks

  if (millis() - lastHeartbeat >= HEARTBEAT_MS) {
    lastHeartbeat = millis();
    Serial.printf("[STATUS] wifi=%s mqtt=%s relay=%s rssi=%d dBm\n",
                  WiFi.status() == WL_CONNECTED ? "up" : "down",
                  mqtt.connected() ? "up" : "down",
                  relayOn ? "ON" : "OFF",
                  WiFi.RSSI());
  }
}

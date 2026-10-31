#define ENABLE_USER_AUTH
#define ENABLE_DATABASE

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <FirebaseClient.h>
#include <DHT.h>
#include <time.h>

// ======================================================
// WIFI
// ======================================================

#define WIFI_SSID "YOUR_WIFI_NAME"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

// ======================================================
// FIREBASE
// ======================================================

#define API_KEY "YOUR_FIREBASE_WEB_API_KEY"

#define USER_EMAIL "YOUR_ESP32_DEVICE_EMAIL"
#define USER_PASSWORD "YOUR_ESP32_DEVICE_PASSWORD"

#define DATABASE_URL "https://YOUR-PROJECT-default-rtdb.REGION.firebasedatabase.app/"

// ======================================================
// PINS
// ======================================================

#define DHTPIN 4
#define DHTTYPE DHT11

#define LDR_PIN 34

#define RELAY_PIN 26

// ======================================================
// OBJECTS
// ======================================================

DHT dht(DHTPIN, DHTTYPE);

WiFiClientSecure ssl_client;

using AsyncClient = AsyncClientClass;
AsyncClient aClient(ssl_client);

FirebaseApp app;
UserAuth user_auth(API_KEY, USER_EMAIL, USER_PASSWORD);
RealtimeDatabase Database;

// ======================================================
// TIMERS
// ======================================================

unsigned long lastReading = 0;
unsigned long lastControlRead = 0;

const unsigned long readingInterval = 5000;
const unsigned long controlInterval = 2000;
const unsigned long historyInterval = 60000; // one /history record per minute
unsigned long lastHistoryLog = 0;

// ======================================================
// SYSTEM STATE
// ======================================================

bool bulbState = false;

String operatingMode = "MANUAL";

// Your LDR module is digital.
// Change this to 0 later if your module behaves opposite.
const int DARK_LDR_STATE = 1;

// ======================================================
// HISTORY LOGGING
// ======================================================

// Current Unix time in milliseconds as text, or "" until NTP has synced.
// A String is used because the value does not fit in a 32-bit integer.
String timestampMs() {
  time_t now = time(nullptr);
  if (now < 1700000000) {
    return "";
  }
  return String((unsigned long)now) + "000";
}

// Adds one record under /history with an auto-generated key.
void logHistory(float temperature, float humidity, int ldrState) {
  String ts = timestampMs();
  if (ts.length() == 0) {
    Serial.println("History skipped: time not synced yet");
    return;
  }

  String json = "{\"timestamp\":" + ts +
                ",\"temperature\":" + String(temperature, 1) +
                ",\"humidity\":" + String(humidity, 1) +
                ",\"ldr\":" + String(ldrState) +
                ",\"bulbState\":" + String(bulbState ? "true" : "false") +
                ",\"mode\":\"" + operatingMode + "\"}";

  String key = Database.push<object_t>(aClient, "/history", object_t(json));

  if (aClient.lastError().code() == 0) {
    Serial.print("History record added: ");
    Serial.println(key);
  } else {
    Serial.print("History write failed: ");
    Serial.println(aClient.lastError().message());
  }
}

// ======================================================
// SETUP
// ======================================================

void setup() {

  Serial.begin(115200);
  delay(1000);

  // Sensors
  dht.begin();

  pinMode(LDR_PIN, INPUT);

  // Relay
  pinMode(RELAY_PIN, OUTPUT);

  // Relay is ACTIVE-LOW
  // HIGH = OFF
  digitalWrite(RELAY_PIN, HIGH);

  Serial.println();
  Serial.println("================================");
  Serial.println("SMART ENVIRONMENT MONITOR");
  Serial.println("================================");

  // ----------------------------------------------------
  // Wi-Fi
  // ----------------------------------------------------

  Serial.println();
  Serial.println("Connecting to Wi-Fi...");

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {

    Serial.print(".");
    delay(500);
  }

  Serial.println();
  Serial.println("Wi-Fi connected!");

  Serial.print("ESP32 IP: ");
  Serial.println(WiFi.localIP());

  // ----------------------------------------------------
  // Firebase
  // ----------------------------------------------------

  // Real time for /history timestamps (stored as UTC)
  configTime(0, 0, "pool.ntp.org", "time.google.com");

  ssl_client.setInsecure();

  Serial.println();
  Serial.println("Initializing Firebase...");

  initializeApp(aClient, app, getAuth(user_auth));

  app.getApp<RealtimeDatabase>(Database);

  Database.url(DATABASE_URL);

  Serial.println("Firebase initialized!");

  Serial.println();
  Serial.println("System ready.");
}

// ======================================================
// LOOP
// ======================================================

void loop() {

  // Keep Firebase authentication alive
  app.loop();

  if (!app.ready()) {
    return;
  }

  // ====================================================
  // READ CONTROL FROM FIREBASE
  // ====================================================

  if (millis() - lastControlRead >= controlInterval) {

    lastControlRead = millis();

    // --------------------------------------------------
    // Read operating mode
    // --------------------------------------------------

    String newMode =
      Database.get<String>(aClient, "/control/mode");

    if (aClient.lastError().code() == 0) {

      if (newMode == "AUTO" || newMode == "MANUAL") {

        operatingMode = newMode;

      }

    } else {

      Serial.print("Mode read failed: ");
      Serial.println(aClient.lastError().message());
    }

    // --------------------------------------------------
    // Read manual bulb state
    // --------------------------------------------------

    bool manualBulbState =
      Database.get<bool>(aClient, "/control/bulbState");

    if (aClient.lastError().code() != 0) {

      Serial.print("Bulb control read failed: ");
      Serial.println(aClient.lastError().message());

    } else {

      // ================================================
      // MANUAL MODE
      // ================================================

      if (operatingMode == "MANUAL") {

        bulbState = manualBulbState;

      }

      // ================================================
      // AUTO MODE
      // ================================================

      else if (operatingMode == "AUTO") {

        int ldrState = digitalRead(LDR_PIN);

        // Dark → bulb ON
        // Bright → bulb OFF

        if (ldrState == DARK_LDR_STATE) {

          bulbState = true;

        } else {

          bulbState = false;
        }
      }

      // ------------------------------------------------
      // Actually control relay
      // ------------------------------------------------

      if (bulbState) {

        // Active LOW relay
        digitalWrite(RELAY_PIN, LOW);

      } else {

        digitalWrite(RELAY_PIN, HIGH);
      }

      // ------------------------------------------------
      // Print control information
      // ------------------------------------------------

      Serial.println();
      Serial.println("========== CONTROL ==========");

      Serial.print("Mode: ");
      Serial.println(operatingMode);

      Serial.print("Bulb: ");
      Serial.println(bulbState ? "ON" : "OFF");

      Serial.println("=============================");

      // ------------------------------------------------
      // Update Firebase current state
      // ------------------------------------------------

      if (Database.set<bool>(
            aClient,
            "/current/bulbState",
            bulbState)) {

        Serial.println("Bulb state → Firebase OK");

      } else {

        Serial.print("Bulb state → Firebase FAILED: ");
        Serial.println(aClient.lastError().message());
      }

      if (Database.set<String>(
            aClient,
            "/current/mode",
            operatingMode)) {

        Serial.println("Mode → Firebase OK");

      } else {

        Serial.print("Mode → Firebase FAILED: ");
        Serial.println(aClient.lastError().message());
      }
    }
  }

  // ====================================================
  // READ SENSORS
  // ====================================================

  if (millis() - lastReading >= readingInterval) {

    lastReading = millis();

    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();

    int ldrState = digitalRead(LDR_PIN);

    // --------------------------------------------------
    // Check DHT
    // --------------------------------------------------

    if (isnan(temperature) || isnan(humidity)) {

      Serial.println("ERROR: DHT11 reading failed!");

      return;
    }

    // --------------------------------------------------
    // Print readings
    // --------------------------------------------------

    Serial.println();
    Serial.println("--------------------------------");

    Serial.print("Temperature: ");
    Serial.print(temperature);
    Serial.println(" °C");

    Serial.print("Humidity: ");
    Serial.print(humidity);
    Serial.println(" %");

    Serial.print("LDR: ");
    Serial.println(ldrState);

    Serial.print("Bulb: ");
    Serial.println(bulbState ? "ON" : "OFF");

    Serial.print("Mode: ");
    Serial.println(operatingMode);

    Serial.println("--------------------------------");

    // --------------------------------------------------
    // Firebase temperature
    // --------------------------------------------------

    if (Database.set<float>(
          aClient,
          "/current/temperature",
          temperature)) {

      Serial.println("Temperature → Firebase OK");

    } else {

      Serial.print("Temperature → Firebase FAILED: ");
      Serial.println(aClient.lastError().message());
    }

    // --------------------------------------------------
    // Firebase humidity
    // --------------------------------------------------

    if (Database.set<float>(
          aClient,
          "/current/humidity",
          humidity)) {

      Serial.println("Humidity → Firebase OK");

    } else {

      Serial.print("Humidity → Firebase FAILED: ");
      Serial.println(aClient.lastError().message());
    }

    // --------------------------------------------------
    // Firebase LDR
    // --------------------------------------------------

    if (Database.set<int>(
          aClient,
          "/current/ldr",
          ldrState)) {

      Serial.println("LDR → Firebase OK");

    } else {

      Serial.print("LDR → Firebase FAILED: ");
      Serial.println(aClient.lastError().message());
    }

    // --------------------------------------------------
    // Firebase history log
    // --------------------------------------------------

    if (lastHistoryLog == 0 || millis() - lastHistoryLog >= historyInterval) {

      lastHistoryLog = millis();
      logHistory(temperature, humidity, ldrState);
    }
  }
}
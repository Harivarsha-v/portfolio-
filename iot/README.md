# IoT Assignment source code

Source for the five IoT tasks documented at `week-7.html`. Tasks 1 to 3 were group work; Tasks 4 and 5 are my individual work.

| Folder | Contents |
|---|---|
| `task1_web_server/` | GPIO warm-up sketch and the ESP32 web server sketch |
| `task2_adafruit_mqtt/` | ESP32 MQTT client for Adafruit IO |
| `task3_ifttt_voice/` | Script to test the feed. The firmware is the same as Task 2 |
| `task4_task5_firebase/firmware/esp32_env/` | Firmware used in the demo; writes `/current` and reads `/control` |
| `task4_task5_firebase/firmware/esp32_env_history_logging/` | Same firmware plus a timestamped `/history` record every minute. It compiles, but this update has not been flashed to the board yet |
| `task4_task5_firebase/dashboard/` | Complete dashboard page |
| `task4_task5_firebase/data/` | The imported test records and the CSV made from them |

Wi-Fi details, keys and passwords are placeholders. Fill them in before uploading a sketch.

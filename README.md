# VENTRA — Source Code

**Two-Loop Indoor Hazard Detection & Ventilation Demonstrator**

This submission contains the source code for the VENTRA system. The system uses two processors: an ESP32 for sensing and a Raspberry Pi (running Node-RED) for logic, display, ventilation, logging, outdoor air data, and AI explanations.

---

## Files

**1. `Ventra.ino`** — ESP32 firmware (Arduino)
Reads three sensors (MQ-2 gas, flame sensor, DHT11 temperature/humidity), runs a local safety reflex (LEDs, buzzer, relay), and publishes readings as JSON over MQTT to the topic `ventra/readings`. Subscribes to `ventra/command` to receive fan on/off commands.

**2. `flows.json`** — Node-RED flow (Raspberry Pi)
The control loop. Subscribes to `ventra/readings`, evaluates severity (NORMAL / GAS_WARN / GAS_HIGH / FIRE), decides ventilation, controls the fan via MQTT, drives a web dashboard (gauges, chart, status, AI text), fetches outdoor air quality from Open-Meteo every 5 minutes, generates AI explanations via the Gemini API, and logs every reading to a SQLite database.


---

## Design note: ventilation logic

The system deliberately keeps the ventilation fan **OFF** during a fire event. Forcing air onto a fire feeds it oxygen, so ventilation is suppressed on fire and enabled only for gas events. This is an intentional safety decision.

---

## Dependencies

**ESP32 (Arduino libraries):**
- WiFi
- PubSubClient (MQTT)
- DHT sensor library
- ArduinoJson

**Raspberry Pi:**
- Node-RED
- Mosquitto (MQTT broker)
- Node-RED palettes: `node-red-dashboard`, `node-red-node-sqlite`
- Python: `luma.oled` (for the OLED script)

---

## Setup notes

- The ESP32 firmware has WiFi credentials and the MQTT broker IP set at the top of the file — update these to match your network.
- Mosquitto must be configured to accept network connections (listener on `0.0.0.0:1883`) so the ESP32 can connect from another device.
- I2C must be enabled on the Raspberry Pi for the OLED.
- The Node-RED exec node calls `oled.py` and appends `msg.payload` as arguments; the payload is a `~`-separated string built from the current status and readings.
- The SQLite `readings` table is created by the "create table" inject node — **run this inject once after first import** before expecting readings to log.
- The SQLite database uses the relative path `ventra.db`, which Node-RED resolves inside its own working directory (`~/.node-red/`). No absolute path is hardcoded, so it works for any user on any machine.

---

## Note: 

**Testing without hardware:** After importing the flow, open the Node-RED editor and click the sample inject node (preloaded with a test sensor payload). The debug panel on the right will display the processed message — showing the severity evaluation, and the data flowing through to the dashboard and database. No ESP32 or Raspberry Pi sensors are required to demonstrate the logic.

The debug pannel `Debug 5` will throw an error this is because Gemini API key to run the AI explanation feature on the dashboard as well as on the debug pannel has not been inserted. Please generate a gemini api from [here](https://ai.google.dev/gemini-api/docs/api-key) please enter your generated key in the gemini node. Gemini node> URL section> Scroll towards extreme right> Enter API.

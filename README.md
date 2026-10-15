# Crawler

ESP-gesteuerter Crawler mit Joystick-Fernbedienung.

| Ordner | Inhalt | Ursprung |
|---|---|---|
| `crawler/` | Firmware des Crawlers (ESP32/ESP8266, PCA9685- oder GPIO-Motoren, optional FPV-Kamera) | `combi10` |
| `OSC_Joystick/` | Joystick-Fernbedienung (ESP32), sendet per WLAN/OSC oder ESP-NOW | `OSC_Joystick10` |

## Kommunikation

- **WLAN/OSC:** Joystick und Crawler verbinden sich mit dem WLAN, Steuerung per UDP/OSC.
- **ESP-NOW:** Fallback, wenn der Joystick kein WLAN hat; Pairing per Broadcast-Handshake auf Kanal 1–13.

## Voraussetzungen

Die WLAN-Zugangsdaten kommen aus der Bibliothek `Credentials.h` (nicht im Repository), die `ssid` und `password` definiert.

Benötigte Bibliotheken: `OSCMessage`, `Streaming`, `Adafruit_NeoPixel`, `Adafruit_PWMServoDriver`.

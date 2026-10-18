// WiFi roaming: several APs share the same SSID. While driving, the crawler
// watches its signal and switches to a clearly stronger AP when it gets weak.
// The scan runs in the background (async), so loop() keeps driving.

#define ROAM_CHECK_INTERVAL 2000     // ms between RSSI checks
#define ROAM_RSSI_THRESHOLD -70      // dBm: only look for another AP below this
#define ROAM_SCAN_INTERVAL 15000     // ms: at most one scan in this time
#define ROAM_MARGIN 8                // dB the new AP must be better than the current one
#define ROAM_SCAN_MS_PER_CHAN 120    // ESP32: short dwell time per channel (default 300)
#define ROAM_FALLBACK_TIMEOUT 10000  // ms without connection after a switch -> connect to any AP

// Index of the scan result with our SSID and the strongest signal, or -1
int strongestAp(int networks) {
  int strongest = -1;
  for (int i = 0; i < networks; i++) {
    if (WiFi.SSID(i) == ssid && (strongest < 0 || WiFi.RSSI(i) > WiFi.RSSI(strongest))) strongest = i;
  }
  return strongest;
}

void handleRoaming() {
  static unsigned long lastCheck = 0;
  static unsigned long lastScan = 0;
  static bool scanning = false;
  static bool wasConnected = true;
  static unsigned long lostSince = 0;
  static bool lockedToBssid = true;    // last begin() may be for one specific AP (ESP8266 setupWifi() too)

  if (!wifiRoaming) return;
  unsigned long now = millis();

  if (WiFi.status() != WL_CONNECTED) {
    if (wasConnected) {
      wasConnected = false;
      lostSince = now;
      Serial << "WiFi lost" << endl;
    }
    // A begin() with a BSSID only reconnects to that AP; if it is gone, accept any AP
    if (lockedToBssid && now - lostSince > ROAM_FALLBACK_TIMEOUT) {
      Serial << "Roaming: AP not reachable, connecting to any AP" << endl;
      lockedToBssid = false;
      WiFi.begin(ssid, password);
    }
    return;
  }
  if (!wasConnected) {
    wasConnected = true;
    homeChannel = WiFi.channel();   // reported to the joystick on the next ESP-NOW pairing
    Serial << "WiFi connected  Channel: " << homeChannel
           << "  AP: " << WiFi.BSSIDstr() << "  RSSI: " << WiFi.RSSI() << endl;
  }

  if (scanning) {
    int networks = WiFi.scanComplete();
    if (networks == WIFI_SCAN_RUNNING) return;
    scanning = false;
    if (networks < 0) return;        // scan failed, try again after ROAM_SCAN_INTERVAL
    int best = strongestAp(networks);
    int currentRssi = WiFi.RSSI();
    if (best >= 0 && memcmp(WiFi.BSSID(best), WiFi.BSSID(), 6) != 0 && WiFi.RSSI(best) >= currentRssi + ROAM_MARGIN) {
      Serial << "Roaming: " << WiFi.BSSIDstr() << " (" << currentRssi << " dBm) -> "
             << WiFi.BSSIDstr(best) << " (" << WiFi.RSSI(best) << " dBm)" << endl;
      uint8_t bssid[6];
      memcpy(bssid, WiFi.BSSID(best), 6);
      int32_t channel = WiFi.channel(best);
      WiFi.scanDelete();
      WiFi.config(staticIP, gateway, subnet);
      WiFi.begin(ssid, password, channel, bssid);   // disconnects from the current AP first
      lockedToBssid = true;
    } else {
      WiFi.scanDelete();
    }
    return;
  }

  if (now - lastCheck < ROAM_CHECK_INTERVAL) return;
  lastCheck = now;
  if (WiFi.RSSI() >= ROAM_RSSI_THRESHOLD || now - lastScan < ROAM_SCAN_INTERVAL) return;
  lastScan = now;
#if defined ESP8266
  scanning = WiFi.scanNetworks(true) == WIFI_SCAN_RUNNING;
#elif defined ESP32
  scanning = WiFi.scanNetworks(true, false, false, ROAM_SCAN_MS_PER_CHAN, 0, ssid) == WIFI_SCAN_RUNNING;
#endif
}

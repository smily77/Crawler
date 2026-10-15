// Funktion zum Scannen nach dem Controller auf allen Kanälen
void findController() {
  isPaired = false;
  
  for (int channel = 1; channel <= 13; channel++) {
    if (!isPaired) testChannel(channel);
    
  }  // for channel 1 to 13  
  if (!isPaired) { if (DEBUG)  Serial << "No success" << endl;
    delay(1000);
  }
}

void testChannel(int channel) {
      if (DEBUG) Serial << "Try Channel: " << channel << endl;
    esp_now_deinit();                                         // Bestehende ESP-NOW deinit um Kanal zu wechseln 
    WiFi.disconnect();                                        // WiFi trennen und Kanal wechseln
    delay(100);   
    homeChannel = channel;
    esp_wifi_set_channel(homeChannel, WIFI_SECOND_CHAN_NONE);
    if (esp_now_init() != ESP_OK) {                             // ESP-NOW neu initialisieren
      if (DEBUG) Serial << "Fehler bei der ESP-NOW Re-Initialisierung" << endl;
      return;
    }
    esp_now_register_recv_cb(OnDataRecv);                        // Callbacks neu registrieren
    esp_now_register_send_cb(OnDataSent);
    pairing_message message;                                    // Broadcast-Pairing-Nachricht erstellen
    WiFi.macAddress(message.mac_addr);
    message.channel = homeChannel;
    message.isPairing = true;
    uint8_t broadcastAddress[6];                                // Broadcast-Adresse erstellen (alle 1en)
    memset(broadcastAddress, 0xFF, sizeof(broadcastAddress));
    esp_now_peer_info_t peerInfo = {};                         // Peer für Broadcast hinzufügen // Initialisiert alle Werte auf 0 
    memcpy(peerInfo.peer_addr, broadcastAddress, 6);
    peerInfo.channel = 0;  // Automatischer Kanal
    peerInfo.encrypt = false;
    if (esp_now_is_peer_exist(broadcastAddress)) esp_now_del_peer(broadcastAddress);  // Prüfen, ob der Peer bereits existiert
    if (esp_now_add_peer(&peerInfo) == ESP_OK) {    
      esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &message, sizeof(message)); // Pairing-Nachricht senden
      if (result == ESP_OK) if (DEBUG) Serial << "Pairing-Anfrage gesendet" << endl;
        else if (DEBUG) Serial << "Fehler beim Senden der Pairing-Anfrage: " << result << endl;  
    } else if (DEBUG) Serial << "Fehler beim Hinzufügen des Broadcast-Peers"  << endl;    
    
    delay(1000);                                                               // Warten auf Antwort
     
    if (isPaired) {                                                            // Wenn Pairing erfolgreich, Schleife beenden
      Serial.println("Pairing erfolgreich!");
      return;
    }
    if (esp_now_is_peer_exist(broadcastAddress)) esp_now_del_peer(broadcastAddress);   // Broadcast-Peer entfernen vor dem nächsten Durchlauf    
}


// Callback-Funktion für gesendete Daten
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  if (status == ESP_NOW_SEND_SUCCESS) if (FIXESP) Serial << "OnDataSent - Send success full" << endl;
    else if (FIXESP) Serial << "OnDataSent - Send failed " << endl;
}

// Callback-Funktion für empfangene Daten
void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
  if (len == sizeof(pairing_message)) {
    pairing_message message;
    memcpy(&message, incomingData, sizeof(message));       
    if (!message.isPairing && !isPaired) {                                           // Wenn wir eine Pairing-Bestätigung erhalten haben
      if (DEBUG) Serial << "Pairing-Bestätigung erhalten!"  << endl;
      memcpy(controllerMacAddress, message.mac_addr, sizeof(controllerMacAddress));  // Controller-Daten speichern
      esp_now_deinit();                                                              // Alle bestehenden Peers entfernen und ESP-NOW neu starten
      homeChannel = message.channel;                                                 // Auf den Kanal des Controllers wechseln 
      esp_wifi_set_channel(homeChannel, WIFI_SECOND_CHAN_NONE);
      Serial.print("Wechsel zu Kanal: ");
      Serial.println(homeChannel);
      if (esp_now_init() != ESP_OK) {                                               // ESP-NOW neu initialisieren
        if (DEBUG) Serial << "Fehler bei der ESP-NOW Re-Initialisierung"  << endl;
        return;
      }
      esp_now_register_recv_cb(OnDataRecv);                                          // Callbacks neu registrieren
      esp_now_register_send_cb(OnDataSent);
      esp_now_peer_info_t peerInfo = {};  // Initialisiert alle Werte auf 0         // Peer-Verbindung herstellen
      memcpy(peerInfo.peer_addr, message.mac_addr, 6);
      peerInfo.channel = 0;              // Automatischer Kanal
      peerInfo.encrypt = false;     
      if (esp_now_add_peer(&peerInfo) == ESP_OK) {
        Serial.println("Controller in Peer-Liste hinzugefügt");
        isPaired = true;
      }
      else if (DEBUG) Serial << "Fehler beim Hinzufügen des Controllers als Peer"  << endl;
    }  // is paining msg
  }    // is size of pairing msg
  else if (isPaired) {
    // Normale Datenverarbeitung nach erfolgreichem Pairing
    Serial.print("Daten vom Controller empfangen: ");
    for (int i = 0; i < len; i++) {
      Serial.print(incomingData[i]);
      Serial.print(" ");
    }
    Serial.println();
  }
}

#include <FastLED.h>
#include <ETH.h>
#include <ArtnetWifi.h>
#include <ArduinoOTA.h>
#include <WebServer.h>
#include <Preferences.h>

// Anzahl der LEDs pro Board
#define NUM_LEDS_PER_BOARD 32 // 32 LEDs pro Board (16 gespiegelt + 16 normal)
#define LED_PIN 4 // Ein einziger Pin für alle LEDs
#define wiederholungen 5 // Anzahl der Wiederholungen für die ersten 5 Boards


// Gesamtanzahl der LEDs (alle Boards zusammen)
#define TOTAL_LEDS (NUM_LEDS_PER_BOARD * wiederholungen + 40) // 40 LEDs für das zusätzliche Board



// LED-Array für alle LEDs
CRGB leds[TOTAL_LEDS];

// Art-Net-Objekt
ArtnetWifi artnet;

Preferences preferences;

// Ethernet-Event-Handler
void WiFiEvent(WiFiEvent_t event) {
  switch (event) {
    case ARDUINO_EVENT_ETH_START:
      Serial.println("Ethernet gestartet");
      ETH.setHostname("WT32-ETH01");
      break;
    case ARDUINO_EVENT_ETH_CONNECTED:
      Serial.println("Ethernet verbunden");
      break;
    case ARDUINO_EVENT_ETH_GOT_IP:
      Serial.print("IP-Adresse: ");
      Serial.println(ETH.localIP());
      break;
    case ARDUINO_EVENT_ETH_DISCONNECTED:
      Serial.println("Ethernet getrennt");
      break;
    case ARDUINO_EVENT_ETH_STOP:
      Serial.println("Ethernet gestoppt");
      break;
    default:
      break;
  }
}

// Standardwerte für Timeout, Farbe und Helligkeit
unsigned long artnetTimeout = 2000; // 2 Sekunden
CRGB defaultColor = CRGB::Blue;
uint8_t brightness = 128; // 50% Helligkeit

unsigned long lastArtNetPacket = 0; // Zeitstempel des letzten Art-Net-Pakets

// Funktion, um LEDs auf die Standardfarbe zu setzen
void setAllToDefaultColor() {
    for (int i = 0; i < TOTAL_LEDS; i++) {
        leds[i] = defaultColor;
    }
    FastLED.setBrightness(brightness);
    FastLED.show();
}

// Webserver auf Port 80
WebServer server(80);

// Webinterface-Handler
void handleRoot() {
    String html = "<html><body>";
    html += "<h1>LED Controller</h1>";
    html += "<form action='/update' method='POST'>";
    html += "Timeout (ms): <input type='number' name='timeout' value='" + String(artnetTimeout) + "'><br>";
    html += "Farbe: <input type='color' name='color' value='#" + String(defaultColor.r, HEX) + String(defaultColor.g, HEX) + String(defaultColor.b, HEX) + "'><br>";
    html += "Helligkeit: <input type='number' name='brightness' value='" + String(brightness) + "'><br>";
    html += "<input type='submit' value='Update' onclick='window.location.reload()'>";
    html += "</form></body></html>";
    server.send(200, "text/html", html);
}

// Funktion, um Werte aus dem Speicher zu laden
void loadPreferences() {
    preferences.begin("led-config", true); // Nur-Lese-Modus
    artnetTimeout = preferences.getULong("timeout", 2000); // Standardwert 2000 ms
    uint32_t color = preferences.getUInt("color", 0x0000FF); // Standardwert Blau
    defaultColor = CRGB((color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF);
    brightness = preferences.getUChar("brightness", 128); // Standardwert 50%
    preferences.end();
}

// Funktion, um Werte im Speicher zu speichern
void savePreferences() {
    preferences.begin("led-config", false); // Schreib-Modus
    preferences.putULong("timeout", artnetTimeout);
    uint32_t color = (defaultColor.r << 16) | (defaultColor.g << 8) | defaultColor.b;
    preferences.putUInt("color", color);
    preferences.putUChar("brightness", brightness);
    preferences.end();
}

// Webinterface-Handler aktualisieren
void handleUpdate() {
    if (server.hasArg("timeout")) {
        artnetTimeout = server.arg("timeout").toInt();
    }
    if (server.hasArg("color")) {
        String color = server.arg("color");
        long hexColor = strtol(color.c_str() + 1, NULL, 16);
        defaultColor = CRGB((hexColor >> 16) & 0xFF, (hexColor >> 8) & 0xFF, hexColor & 0xFF);
    }
    if (server.hasArg("brightness")) {
        brightness = server.arg("brightness").toInt();
    }
    savePreferences(); // Änderungen speichern
    server.send(200, "text/plain", "Updated");
}



void setup() {
  // Serielle Kommunikation deaktivieren, um Pins 1 und 3 zu verwenden
  // Serielle Ausgabe für Debugging
  Serial.begin(115200);
  Serial.println("Starte WT32-ETH01 mit Ethernet...");

  // FastLED-Controller für alle LEDs initialisieren
  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, TOTAL_LEDS);

  // Ethernet initialisieren
  WiFi.onEvent(WiFiEvent);
  ETH.begin();  // Startet Ethernet

  // OTA initialisieren
  ArduinoOTA.setHostname("WT32-Faderboard");
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "Sketch";
    } else { // U_SPIFFS
      type = "Filesystem";
    }
    Serial.println("OTA Update Start: " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("OTA Update Ende");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("OTA Fortschritt: %u%%\n", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA Fehler [%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Fehler");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Fehler");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Verbindungsfehler");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Empfangsfehler");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Fehler");
    }
  });
  ArduinoOTA.begin();
  Serial.println("OTA bereit");

  // Art-Net initialisieren
  artnet.begin();
  Serial.println("Art-Net gestartet!");

  // Webserver konfigurieren
  server.on("/", handleRoot);
  server.on("/update", HTTP_POST, handleUpdate);
  server.begin();
  Serial.println("Webserver gestartet");
}

void loop() {
  // OTA-Handler
  ArduinoOTA.handle();

  // Art-Net Timeout prüfen
  if (millis() - lastArtNetPacket > artnetTimeout) {
    setAllToDefaultColor();
  }

  // Webserver-Handler
  server.handleClient();

  // Art-Net-Daten empfangen
  int packetSize = artnet.read();
  if (packetSize) {
    lastArtNetPacket = millis(); // Zeitstempel aktualisieren

    // DMX-Daten auslesen
    uint8_t* dmxData = artnet.getDmxFrame();

    // LEDs basierend auf den DMX-Daten steuern
    int offset = 0;
    // Muster 5-mal wiederholen
    for (int repeat = 0; repeat < wiederholungen; repeat++) {
        // LEDs 1-8 gespiegelt mit LEDs 9-16 (DMX 1-24 pro Wiederholung)
        for (int i = 0; i < 8; i++) {
            int channelIndex = (repeat * 72) + (i * 3); // 72 Kanäle pro Wiederholung, 3 Kanäle pro LED
            leds[offset + i] = CRGB(dmxData[channelIndex], dmxData[channelIndex + 1], dmxData[channelIndex + 2]);
            leds[offset + 15 - i] = leds[offset + i]; // Spiegelung
        }
        offset += 16; // Weiter zu den nächsten LEDs

        // LEDs 17-32 "normal" (DMX 25-72 pro Wiederholung)
        for (int i = 0; i < 16; i++) {
            int channelIndex = (repeat * 72) + 24 + (i * 3); // Start bei DMX 25 pro Wiederholung
            leds[offset + i] = CRGB(dmxData[channelIndex], dmxData[channelIndex + 1], dmxData[channelIndex + 2]);
        }
        offset += 16; // Weiter zur nächsten Wiederholung
    }

    // 40 einzelne LEDs "normal" steuern
    for (int i = 0; i < 40; i++) {
        int channelIndex = ((wiederholungen) * 72) + (i * 3); // Start bei DMX 361 nach den 5 Wiederholungen
        leds[offset + i] = CRGB(dmxData[channelIndex], dmxData[channelIndex + 1], dmxData[channelIndex + 2]);
    }

    FastLED.show();
  }
}
#include <FastLED.h>
#include <ETH.h>
#include <ArtnetWifi.h>
#include <ArduinoOTA.h>

// Anzahl der LEDs pro Board
#define NUM_LEDS_PER_BOARD 24

// GPIOs für die Daughterboards
const int boardPins[] = {2, 4, 12, 14, 15};
const int numBoards = sizeof(boardPins) / sizeof(boardPins[0]);

// LED-Arrays für jedes Board
CRGB leds[numBoards][NUM_LEDS_PER_BOARD];

// Art-Net-Objekt
ArtnetWifi artnet;

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

void setup() {
  // Serielle Ausgabe für Debugging
  Serial.begin(115200);
  Serial.println("Starte WT32-ETH01 mit Ethernet...");

  // FastLED-Controller für jedes Board initialisieren
  FastLED.addLeds<WS2812, 2, GRB>(leds[0], NUM_LEDS_PER_BOARD);
  FastLED.addLeds<WS2812, 4, GRB>(leds[1], NUM_LEDS_PER_BOARD);
  FastLED.addLeds<WS2812, 12, GRB>(leds[2], NUM_LEDS_PER_BOARD);
  FastLED.addLeds<WS2812, 14, GRB>(leds[3], NUM_LEDS_PER_BOARD);
  FastLED.addLeds<WS2812, 15, GRB>(leds[4], NUM_LEDS_PER_BOARD);

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
}

void loop() {
  // OTA-Handler
  ArduinoOTA.handle();

  // Art-Net-Daten empfangen
  int packetSize = artnet.read();
  if (packetSize) {
    // DMX-Daten auslesen
    uint8_t* dmxData = artnet.getDmxFrame();

    // LEDs für jedes Board basierend auf den DMX-Daten steuern
    for (int board = 0; board < numBoards; board++) {
      int startChannel = board * NUM_LEDS_PER_BOARD * 3 + 1;  // 3 Kanäle pro LED (R, G, B), Start bei Kanal 1
      for (int i = 0; i < NUM_LEDS_PER_BOARD; i++) {
        int channelIndex = startChannel + (i * 3) - 1; // -1, da DMX-Kanäle bei 1 beginnen
        leds[board][i] = CRGB(dmxData[channelIndex], dmxData[channelIndex + 1], dmxData[channelIndex + 2]);
      }
      FastLED.show();
    }
  }
}
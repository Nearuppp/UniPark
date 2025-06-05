#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#include "rgb_lcd.h"
rgb_lcd lcd;


// === Configuration WiFi ===
const char* ssid = "RIOC-TP-IoT";
const char* password = "GRIT-RIOC_2024";

// === Configuration MQTT ===
const char* mqtt_server = "10.19.5.101";
WiFiClient espClient;
PubSubClient client(espClient);

// === LED === //

// Configuration LEDs
const int led1Pins[3] = {25, 27, 26};  // R, G, B (LED 1)
const int led2Pins[3] = {33, 35, 34};  // R, G, B (LED 2)

struct LedState {
  bool magneticDetected;
  bool inYellowPhase;
  bool inGreenPhase;
  unsigned long yellowStartTime;
  unsigned long lastBlinkTime;
  bool ledOn;
};

LedState led1State = {false, false, false, 0, 0, false};
LedState led2State = {false, false, false, 0, 0, false};

// === Prototypes ===
void controlLED(int sensorPin, LedState &state, const int pins[3]);
void setRGB(const int pins[3], int r, int g, int b);


// === SENSOR === //

// Sensor ->
const int capteurPins[7] = {15, 4, 5, 18, 23, 2, 19}; // ⚠️ exemple, à adapter selon ton câblage
int capteurs[7] = {-1, -1, -1, -1, -1, -1, -1};
int lastStates[7] = {HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH}; // Pull-up actif → HIGH = pas d’aimant

// === Variables ===
int lastHallState = -1;  // État précédent du capteur

// === Callback pour réception MQTT ===
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message reçu [");
  Serial.print(topic);
  Serial.print("] ");
  for (unsigned int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connexion à : ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  int retry = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    retry++;
    if (retry > 20) {
      Serial.println("\nÉchec de connexion au Wi-Fi.");
      return;
    }
  }

  Serial.println("\nWiFi connecté !");
  Serial.print("Adresse IP : ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Connexion au broker MQTT...");
    if (client.connect("ESP32Client")) {
      Serial.println("Connecté !");
      // Réabonnement
      client.subscribe("capteur/out");
    } else {
      Serial.print("Échec, rc=");
      Serial.print(client.state());
      Serial.println(" nouvelle tentative dans 5s");
      delay(5000);
    }
  }
}


void setup() {

  // Initialisation de l’I2C sur les bons pins pour ESP32
  Wire.begin(21, 22); // SDA = 21, SCL = 22
  lcd.begin(16, 2);   // LCD 16 colonnes, 2 lignes
  lcd.setRGB(0, 255, 0); // Rétroéclairage vert au démarrage

  // Configuration LEDs
  for(int i=0; i<3; i++) {
    pinMode(led1Pins[i], OUTPUT);
    pinMode(led2Pins[i], OUTPUT);
  }
  setRGB(led1Pins, 0, 0, 255); // Bleu au démarrage
  setRGB(led2Pins, 0, 0, 255);

  // Sensor
  for (int i = 0; i < 7; i++) {
    pinMode(capteurPins[i], INPUT_PULLUP);
  }
  Serial.begin(115200);

  // Network
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);  // Ajout du callback
}

void afficheLCD(const String& ligne1, const String& ligne2, uint8_t r=0, uint8_t g=255, uint8_t b=0) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(ligne1);
  lcd.setCursor(0, 1);
  lcd.print(ligne2);
  lcd.setRGB(r, g, b);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  bool changementDetecte = false;

  int count = 0;
  // Boucle sur les 8 capteurs
  for (int i = 0; i < 7; i++) {
    int currentState = digitalRead(capteurPins[i]);
    count = count + ((currentState == LOW) ? 0 : 1);

    if (currentState != lastStates[i]) {
      Serial.print("🔄 Changement détecté sur capteur ");
      Serial.print(i);
      Serial.print(" → ");
      Serial.println(currentState == LOW ? "ACTIVÉ (aimant détecté)" : "LIBRE");

      lastStates[i] = currentState;
      changementDetecte = true;
    }

    capteurs[i] = currentState;

    // Contrôle LEDs pour capteurs 0 et 1
    if(i == 2) controlLED(capteurPins[i], led1State, led1Pins);
    if(i == 1) controlLED(capteurPins[i], led2State, led2Pins);

  }

  if (changementDetecte) {
    envoyerDonneesCapteurs();

    String state = "Nb place libre:" + String(count);

    // Exemple : Afficher l’état du capteur 0 et 1
    String etat0 = "Nb place:";
    String etat1 = String(count);
    afficheLCD(etat0, etat1);
  }

  delay(100); // anti-rebond global
}


// Fonction de contrôle LED générique
void controlLED(int sensorPin, LedState &state, const int pins[3]) {
  int sensorState = digitalRead(sensorPin);
  unsigned long currentTime = millis();

  if (sensorState == LOW) {
    if (!state.magneticDetected) {
      state.magneticDetected = true;
      state.inYellowPhase = true;
      state.inGreenPhase = false;
      state.yellowStartTime = currentTime;
      state.lastBlinkTime = currentTime;
      Serial.print("Capteur ");
      Serial.print(sensorPin);
      Serial.println(" activé → Phase jaune");
    }

    if (state.inYellowPhase && (currentTime - state.yellowStartTime < 5000)) {
      if (currentTime - state.lastBlinkTime >= 200) {
        state.ledOn = !state.ledOn;
        setRGB(pins, state.ledOn ? 255 : 0, state.ledOn ? 255 : 0, 0); // Jaune clignotant
        state.lastBlinkTime = currentTime;
      }
    } 
    else if (state.inYellowPhase) {
      state.inYellowPhase = false;
      state.inGreenPhase = true;
      state.lastBlinkTime = currentTime;
      setRGB(pins, 0, 0, 0); // Éteint avant transition
      Serial.print("Capteur ");
      Serial.print(sensorPin);
      Serial.println(" → Phase verte");
    }

    if (state.inGreenPhase && (currentTime - state.lastBlinkTime >= 200)) {
      state.ledOn = !state.ledOn;
      setRGB(pins, 0, state.ledOn ? 255 : 0, 0); // Vert clignotant
      state.lastBlinkTime = currentTime;
    }

  } else {
    if (state.magneticDetected) {
      state.magneticDetected = false;
      state.inYellowPhase = false;
      state.inGreenPhase = false;
      setRGB(pins, 0, 0, 255); // Retour au bleu
      Serial.print("Capteur ");
      Serial.print(sensorPin);
      Serial.println(" désactivé");
    }
  }
}

// Fonction utilitaire pour contrôle RGB
void setRGB(const int pins[3], int r, int g, int b) {
  analogWrite(pins[0], r);
  analogWrite(pins[1], g);
  analogWrite(pins[2], b);
}


void envoyerDonneesCapteurs() {
  int capteurs[7];
  for (int i = 0; i < 7; i++) {
    capteurs[i] = digitalRead(capteurPins[i]); // capteurPins[] contient les 8 pins
    Serial.print("capteur[");
    Serial.print(i);
    Serial.print("] = ");
    Serial.print(capteurPins[i]);
    Serial.print(", value=");
    Serial.println(capteurs[i]);
  }

  StaticJsonDocument<200> doc;
  JsonArray arr = doc.createNestedArray("capteurs");
  for (int i = 0; i < 7; i++) {
    arr.add(capteurs[i]);
  }
  doc["timestamp"] = millis() / 1000;

  char message[256];
  serializeJson(doc, message);
  client.publish("/parking123/parking_sensors_v3/attrs", message);
}

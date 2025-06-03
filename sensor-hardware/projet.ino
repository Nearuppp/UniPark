#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// === Configuration WiFi ===
const char* ssid = "RIOC-TP-IoT";
const char* password = "GRIT-RIOC_2024";

// === Configuration MQTT ===
const char* mqtt_server = "10.19.5.101";
WiFiClient espClient;
PubSubClient client(espClient);

// === Pins ===

// LED ->
const int hallPin = 22;
const int redPin = 10;
const int greenPin = 9;
const int bluePin = 8;

// Sensor ->
const int capteurPins[8] = {15, 4, 5, 18, 22, 21, 19, 23}; // ⚠️ exemple, à adapter selon ton câblage
int capteurs[8] = {-1, -1, -1, -1, -1, -1, -1, -1};
int lastStates[8] = {HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH}; // Pull-up actif → HIGH = pas d’aimant

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

  // Sensor
  for (int i = 0; i < 8; i++) {
    pinMode(capteurPins[i], INPUT_PULLUP);
  }
  Serial.begin(115200);


  // Network
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);  // Ajout du callback
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  bool changementDetecte = false;

  // Boucle sur les 8 capteurs
  for (int i = 0; i < 8; i++) {
    int currentState = digitalRead(capteurPins[i]);

    if (currentState != lastStates[i]) {
      Serial.print("🔄 Changement détecté sur capteur ");
      Serial.print(i);
      Serial.print(" → ");
      Serial.println(currentState == LOW ? "ACTIVÉ (aimant détecté)" : "LIBRE");

      lastStates[i] = currentState;
      changementDetecte = true;
    }

    capteurs[i] = currentState;
  }

  if (changementDetecte) {
    envoyerDonneesCapteurs();
  }

  delay(100); // anti-rebond global
}


void envoyerDonneesCapteurs() {
  int capteurs[8];
  for (int i = 0; i < 8; i++) {
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
  for (int i = 0; i < 8; i++) {
    arr.add(capteurs[i]);
  }
  doc["timestamp"] = millis() / 1000;

  char message[256];
  serializeJson(doc, message);
  client.publish("/parking123/parking_sensors/attrs", message);
}
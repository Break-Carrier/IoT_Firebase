#include <Arduino.h>
#include <DHT.h>
#include <WiFi.h>
#include <FirebaseESP32.h>
#include "credentials.h"  // Inclure le fichier de credentials

// Définition des constantes
#define DHTPIN 27
#define DHTTYPE DHT11

// Initialisation du capteur
DHT dht(DHTPIN, DHTTYPE);

// Objets Firebase
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// Variable pour gérer le temps
unsigned long previousMillis = 0;
const long interval = 1000;

void setup() {
  Serial.begin(115200);

  // Connexion WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connexion au WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connecté avec l'IP: ");
  Serial.println(WiFi.localIP());

  // Configuration Firebase
  config.database_url = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  // Initialisation du capteur
  dht.begin();
}

void loop() {
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    // Lecture de l'humidité et de la température
    float h = dht.readHumidity();
    float t = dht.readTemperature();

    // Vérification si la lecture a réussi
    if (isnan(h) || isnan(t)) {
      Serial.println("Échec de la lecture du capteur DHT!");
      return;
    }

    // Création d'un objet JSON pour les données
    FirebaseJson json;
    json.set("temperature", t);
    json.set("humidity", h);
    json.set("timestamp/.sv", "timestamp"); // Ajoute un timestamp serveur

    // Envoi des données à Firebase
    if (Firebase.pushJSON(fbdo, "/sensor_readings", json)) {
      Serial.println("Données envoyées avec succès");
      Serial.print("Température: ");
      Serial.print(t);
      Serial.print("°C, Humidité: ");
      Serial.print(h);
      Serial.println("%");
    } else {
      Serial.println("Échec de l'envoi");
      Serial.print("Raison: ");
      Serial.println(fbdo.errorReason());
    }
  }
}
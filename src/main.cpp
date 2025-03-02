#include <Arduino.h>
#include <DHT.h>
#include <WiFi.h>
#include <FirebaseESP32.h>
#include "credentials.h"  // Inclure le fichier de credentials

// Définition des constantes
#define DHTPIN 27
#define DHTTYPE DHT11
#define LED_PIN 2
#define BUTTON_PIN 4

// Seuil de température
#define TEMP_THRESHOLD 28.0  // Seuil de température en °C

// Initialisation du capteur
DHT dht(DHTPIN, DHTTYPE);

// Objets Firebase
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// Variable pour gérer le temps
unsigned long previousMillis = 0;
const long interval = 5000;  // Intervalle de 5 secondes pour les lectures

// Variables d'état
bool isOverThreshold = false;
bool lastButtonState = HIGH;
bool buttonState;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;

void resetWiFi() {
  Serial.println("Réinitialisation de la connexion WiFi...");
  WiFi.disconnect();
  delay(1000);

  // Reconnexion au WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Reconnexion au WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Reconnecté avec l'IP: ");
  Serial.println(WiFi.localIP());
}

void setup() {
  Serial.begin(115200);
  
  // Configuration des broches
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  
  // Initialisation de la LED (éteinte)
  digitalWrite(LED_PIN, LOW);

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
  
  Serial.println("Système de surveillance de température initialisé");
}

void loop() {
  // Gestion du bouton de réinitialisation WiFi
  int reading = digitalRead(BUTTON_PIN);
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }
  
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != buttonState) {
      buttonState = reading;
      if (buttonState == LOW) {  // Bouton pressé (logique inversée avec INPUT_PULLUP)
        resetWiFi();
      }
    }
  }
  lastButtonState = reading;

  // Lecture et envoi des données à intervalle régulier
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

    // Affichage des valeurs sur le moniteur série
    Serial.print("Température: ");
    Serial.print(t);
    Serial.print("°C, Humidité: ");
    Serial.print(h);
    Serial.println("%");

    // Création d'un objet JSON pour les données régulières
    FirebaseJson json;
    json.set("temperature", t);
    json.set("humidity", h);
    json.set("timestamp/.sv", "timestamp"); // Ajoute un timestamp serveur

    // Envoi des données régulières à Firebase
    if (Firebase.pushJSON(fbdo, "/sensor_readings", json)) {
      Serial.println("Données envoyées avec succès");
    } else {
      Serial.println("Échec de l'envoi des données régulières");
      Serial.print("Raison: ");
      Serial.println(fbdo.errorReason());
    }

    // Gestion du dépassement de seuil
    if (t > TEMP_THRESHOLD && !isOverThreshold) {
      // La température vient de dépasser le seuil
      isOverThreshold = true;
      digitalWrite(LED_PIN, HIGH);  // Allumer la LED (simulation du relais)
      
      // Enregistrer l'événement de dépassement dans Firebase
      FirebaseJson eventJson;
      eventJson.set("event", "threshold_exceeded");
      eventJson.set("temperature", t);
      eventJson.set("humidity", h);
      eventJson.set("threshold", TEMP_THRESHOLD);
      eventJson.set("timestamp/.sv", "timestamp");
      
      if (Firebase.pushJSON(fbdo, "/threshold_events", eventJson)) {
        Serial.println("Événement de dépassement enregistré");
      } else {
        Serial.println("Échec de l'enregistrement de l'événement de dépassement");
        Serial.print("Raison: ");
        Serial.println(fbdo.errorReason());
      }
    } 
    else if (t <= TEMP_THRESHOLD && isOverThreshold) {
      // La température est repassée sous le seuil
      isOverThreshold = false;
      digitalWrite(LED_PIN, LOW);  // Éteindre la LED
      
      // Enregistrer l'événement de fin de dépassement dans Firebase
      FirebaseJson eventJson;
      eventJson.set("event", "threshold_ended");
      eventJson.set("temperature", t);
      eventJson.set("humidity", h);
      eventJson.set("threshold", TEMP_THRESHOLD);
      eventJson.set("timestamp/.sv", "timestamp");
      
      if (Firebase.pushJSON(fbdo, "/threshold_events", eventJson)) {
        Serial.println("Événement de fin de dépassement enregistré");
      } else {
        Serial.println("Échec de l'enregistrement de l'événement de fin de dépassement");
        Serial.print("Raison: ");
        Serial.println(fbdo.errorReason());
      }
    }
  }
}
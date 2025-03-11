#include <Arduino.h>
#include <DHT.h>
#include <WiFi.h>
#include <FirebaseESP32.h>
#include "credentials.h"

#define DHTPIN 27
#define DHTTYPE DHT11
#define LED_PIN 26
#define BUTTON_PIN 18

#define TEMP_THRESHOLD_HIGH 21.0
#define TEMP_THRESHOLD_LOW 20.0

DHT dht(DHTPIN, DHTTYPE);
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

unsigned long previousMillis = 0;
const long interval = 5000;

bool isOverThreshold = false;
bool lastButtonState = HIGH;
bool buttonState;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;

void resetWiFi()
{
  Serial.println("Réinitialisation de la connexion WiFi...");
  WiFi.disconnect();
  delay(1000);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Reconnexion au WiFi");
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Reconnecté avec l'IP: ");
  Serial.println(WiFi.localIP());
}

void setup()
{
  Serial.begin(115200);

  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  digitalWrite(LED_PIN, LOW);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connexion au WiFi");
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connecté avec l'IP: ");
  Serial.println(WiFi.localIP());

  config.database_url = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  dht.begin();
}

void loop()
{
  int reading = digitalRead(BUTTON_PIN);

  if (reading != lastButtonState)
  {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay)
  {
    if (reading != buttonState)
    {
      buttonState = reading;

      if (buttonState == LOW)
      {
        Serial.println("✅ Bouton pressé - Réinitialisation WiFi");
        resetWiFi();
      }
      else
      {
        Serial.println("⏳ Bouton relâché");
      }
    }
  }

  lastButtonState = reading;

  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval)
  {
    previousMillis = currentMillis;

    float h = dht.readHumidity();
    float t = dht.readTemperature();

    if (isnan(h) || isnan(t))
    {
      Serial.println("Échec de la lecture du capteur DHT!");
      return;
    }

    Serial.print("Température: ");
    Serial.print(t);
    Serial.print("°C, Humidité: ");
    Serial.print(h);
    Serial.println("%");

    FirebaseJson json;
    json.set("temperature", t);
    json.set("humidity", h);
    json.set("timestamp/.sv", "timestamp");

    if (Firebase.pushJSON(fbdo, "/sensor_readings", json))
    {
      Serial.println("Données envoyées avec succès");
    }
    else
    {
      Serial.println("Échec de l'envoi des données");
      Serial.println(fbdo.errorReason());
    }

    if (t > TEMP_THRESHOLD_HIGH && !isOverThreshold)
    {
      isOverThreshold = true;
      digitalWrite(LED_PIN, HIGH);
      Serial.println("Seuil dépassé, LED ON");
      FirebaseJson eventJson;
      eventJson.set("event", "threshold_exceeded");
      eventJson.set("temperature", t);
      eventJson.set("humidity", h);
      eventJson.set("timestamp/.sv", "timestamp");
      Firebase.pushJSON(fbdo, "/threshold_events", eventJson);
    }
    else if (t <= TEMP_THRESHOLD_LOW && isOverThreshold)
    {
      isOverThreshold = false;
      digitalWrite(LED_PIN, LOW);
      Serial.println("Seuil redescendu, LED OFF");
      FirebaseJson eventJson;
      eventJson.set("event", "threshold_ended");
      eventJson.set("temperature", t);
      eventJson.set("humidity", h);
      eventJson.set("timestamp/.sv", "timestamp");
      Firebase.pushJSON(fbdo, "/threshold_events", eventJson);
    }

    FirebaseJson stateJson;
    stateJson.set("temperature", t);
    stateJson.set("humidity", h);
    stateJson.set("is_over_threshold", isOverThreshold);
    stateJson.set("threshold_high", TEMP_THRESHOLD_HIGH);
    stateJson.set("threshold_low", TEMP_THRESHOLD_LOW);
    stateJson.set("last_update/.sv", "timestamp");

    if (Firebase.setJSON(fbdo, "/current_state", stateJson))
    {
      Serial.println("État mis à jour dans Firebase");
    }
    else
    {
      Serial.println("Échec de la mise à jour");
      Serial.println(fbdo.errorReason());
    }
  }
}

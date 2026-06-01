#include <Arduino.h>
#include <WiFi.h>
#include "sensores.h"
#include "telegram.h"
#include "wserver.h"
#include "secret.h"

#define BUZZER 15
const float UMBRAL_TEMPERATURA = 28.9;

  void setup() {
    Serial.begin(115200);

    initSensores();

    WiFi.begin(SECRET_SSID, SECRET_OPTIONAL_PASS);

    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }

    Serial.println("\nWiFi conectado");

    initWebServer();
    initTelegram();

    xTaskCreatePinnedToCore(taskTelegram, "BotTask", 10000, NULL, 2, NULL, 0);

    pinMode(BUZZER, OUTPUT);
    digitalWrite(BUZZER, HIGH);
  }

  void loop() {
    leerSensores();

    if (humedad > 60) {
      digitalWrite(BUZZER, LOW);
    } else {
      digitalWrite(BUZZER, HIGH);
    }

    handleWebServer();
  }

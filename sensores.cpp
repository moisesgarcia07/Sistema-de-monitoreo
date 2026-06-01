#include "sensores.h"
#include <Arduino.h>
#include <Adafruit_MAX31865.h>
#include "DHT.h"

//----------------------------------------Configuración-DHT-----------------------------------------------//
#define DHTPIN 14
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);
//----------------------------------------Configuración--MAX31865----------------------------------------//
//SPI: CS, DI, DO, CLK
Adafruit_MAX31865 thermo = Adafruit_MAX31865(10, 11, 13, 12);
#define RREF 430.0
#define RNOMINAL 100.0
//-------------------------------------------------------------------------------------------------------//
volatile float temperaturaC = 0.0;
volatile float humedad = 0.0;
//-------------------------------------------------------------------------------------------------------//
void initSensores() {
    dht.begin();
    thermo.begin(MAX31865_3WIRE);
}
//-------------------------------------------------------------------------------------------------------//
void leerSensores() {
    //----------------------------------------------------RTD-------------------------------------------------------------//
  int time = millis();
  uint16_t rtd = thermo.readRTD();
  temperaturaC = (thermo.temperature(RNOMINAL, RREF));
  temperaturaC = (temperaturaC - 1.52); //correccion de temperatura
  Serial.println(temperaturaC);
  int d = (1000 / 16) - (millis() - time);
  if (d > 0) {
    delay(d);
  }
  //--------------------------------------------------------------------------------------------------------------------//
  static unsigned long lastRead = 0;
  if (millis() - lastRead > 2000) {
    float h = dht.readHumidity();
    if (!isnan(h)) {
      humedad = h;
      Serial.print("\nLa humedad es: " + String(humedad));
    } else {Serial.println("Error al leer el sensor DHT22!");}
    lastRead = millis();
  }
}

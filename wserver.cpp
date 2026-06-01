#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

#include "wserver.h"
#include "pagina.h"

extern volatile float temperaturaC;
extern volatile float humedad;

WebServer server(80);

void handleRoot() {
  server.send(200, "text/html", PAGINA_HTML);
}

void handleLeer() {
  String valor = String(temperaturaC, 1) + "," + String(humedad, 1);
  server.send(200, "text/plain", valor);
}

void initWebServer() {
  server.on("/", handleRoot);
  server.on("/leer", handleLeer);
  server.begin();
}

void handleWebServer() {
  server.handleClient();
}
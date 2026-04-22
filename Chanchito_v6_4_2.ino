#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include "secret.h"
#include "pagina.h"
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <Preferences.h>
#include <vector>
#include <Adafruit_MAX31865.h>
#include "DHT.h"

//----------------------------------------Configuración-DHT-----------------------------------------------//
#define DHTPIN 14
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);
//----------------------------------------Configuración--MAX31865----------------------------------------//
//SPI: CS, DI, DO, CLK
Adafruit_MAX31865 thermo = Adafruit_MAX31865(10, 11, 13, 12);
#define RREF      430.0
#define RNOMINAL  100.0
#define WINDOW_SIZE 128
int16_t results[WINDOW_SIZE] = {0};
int current_result = 0;
//------------------------------------------NGrok-sitio-web----------------------------------------------//
String web_url = "No configurado";
const char MASTER_CHAT_ID[] = id_master;
//------------------------------------------------------------------------------------------------------//
#define BUZZER 15
//------------------------------------------------------------------------------------------------------//
volatile float temperaturaC = 0.0;
volatile float humedad = 0.0;
String id = "";
volatile bool alertaEnviada = false;
//-------------------------------------------------------------------------------------------------------//
std::vector<String> alertChatIds;
Preferences preferences;
const char NVS_NAMESPACE[] = "telegram_ids";
const char NVS_KEY[] = "chat_list";
//-------------------------------------------------------------------------------------------------------//
const float UMBRAL_TEMPERATURA = 28.9;

const char ssid[] = SECRET_SSID;
const char password[] = SECRET_OPTIONAL_PASS;
WebServer server(80);
//-----------------------------------------------Bot-----------------------------------------------------//
const unsigned long BOT_MTBS = 1000;
unsigned long bot_lasttime;
WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);
//----------------------------------------------------------------------------------------------------------//
void saveChatIds() {
  String serializedList = "";
  for (size_t i = 0; i < alertChatIds.size(); ++i) {
      serializedList += alertChatIds[i];
      if (i < alertChatIds.size() - 1) {serializedList += ',';}
  }
  preferences.begin(NVS_NAMESPACE, false);
  preferences.putString(NVS_KEY, serializedList);
  preferences.end();
  Serial.print("IDs suscritos guardados: ");
  Serial.println(alertChatIds.size());
}

void loadChatIds() {
  preferences.begin(NVS_NAMESPACE, true);
  String serializedList = preferences.getString(NVS_KEY, "");
  preferences.end();
  alertChatIds.clear();
  if (serializedList.length() > 0) {
    int start = 0;
    int end = 0;
    while (end != -1) {
      end = serializedList.indexOf(',', start);
      if (end != -1) {
        alertChatIds.push_back(serializedList.substring(start, end));
        start = end + 1;
      } else {
        alertChatIds.push_back(serializedList.substring(start));
      }
    }
  }
  Serial.print("IDs suscritos cargados: ");
  Serial.println(alertChatIds.size());
}
//---------------------------------------------------------------------------------------------------------------------------//
void handleNewMessages(int numNewMessages){
  Serial.print("handleNewMessages ");
  Serial.println(numNewMessages);
  for (int i = 0; i < numNewMessages; i++) {
    telegramMessage &msg = bot.messages[i];
    Serial.println("Recibido: " + msg.text);
    String text = msg.text;
    text.toLowerCase();
    String answer = "";
    if (text == "/start") {answer = "Bienvenido al sistema de monitoreo del Cuarto Frio.";}
    else if (text == "/subscribe") {
      String chatIdStr = String(msg.chat_id);
      bool alreadySubscribed = false;
      for (const String& id : alertChatIds) {
        if (id == chatIdStr) {alreadySubscribed = true; break;}
      }
      if (!alreadySubscribed) {
        alertChatIds.push_back(chatIdStr);
        saveChatIds();
        answer = "¡Suscripción exitosa a las alertas!";
      } else {answer = "Ya estabas suscrito a las alertas.";}
    }
    else if (text == "/unsubscribe") {
      String chatIdStr = String(msg.chat_id);
      bool removed = false;
      for (size_t i = 0; i < alertChatIds.size(); ++i) {
        if (alertChatIds[i] == chatIdStr) {
          alertChatIds.erase(alertChatIds.begin() + i);
          saveChatIds();
          removed = true;
          break;
        }
      }
      if (removed) {answer = "Has sido dado de baja de las alertas.";}
      else {answer = "No estabas suscrito a las alertas.";}
    } else if (text == "/temperatura" || text.startsWith("/temperatura@")) {
      answer = String("La temperatura actual es: *") + String((float)temperaturaC, 1) + "* ºC";
    } else if (text == "/humedad" || text.startsWith("/humedad@")) {
      answer = String("El % de humedad es: *") + String((float)humedad, 1) + "* %HR";
    } else if (text == "/estado" || text.startsWith("/estado@")){
      answer = String("La temperatura actual es: *") + String((float)temperaturaC, 1) + "* ºC con una humedad de *" + String((float)humedad, 1) + "* %HR";
    } else if (text == "/myid" || text.startsWith("/myid@")){
      answer = "La ID del servidor es: *" + id + "*";
    } else if (text.startsWith("/setweb")) {  //Para ingresar el enlace del sitio web. ojo
        if (String(msg.chat_id) == MASTER_CHAT_ID) {
          int index = msg.text.indexOf(" ");
          if (index != -1) {
            String nuevaURL = msg.text.substring(index + 1);
            if (nuevaURL.startsWith("http")) {
              web_url = nuevaURL;
              answer = "URL actualizada:\n" + web_url;
            } else {answer = "URL inválida. Usa:\n/setweb https://...";}
          } else {answer = "Uso correcto:\n/setweb https://...";}
        } else {answer = "No tienes permisos.";}
    }
    else if (text == "/server" || text.startsWith("/server@")){
      answer = "La ID del servidor es:\n" + web_url;
    } else if (text.startsWith("/")) {answer = "Comando no reconocido.";}
    if (answer.length() > 0) {bot.sendMessage(msg.chat_id, answer, "Markdown");}
  }
}
//-----------------------------------------------------------------------------------------------------------------------------//
void bot_setup() {
  const String commands = F("["
                             "{\"command\":\"start\",     \"description\":\"Iniciacion del bot\"},"
                             "{\"command\":\"temperatura\", \"description\":\"Temperatura del refrigerador\"},"
                             "{\"command\":\"humedad\", \"description\":\"Humedad del circuito\"},"
                             "{\"command\":\"estado\",\"description\":\"Estado actual de los valores\"},"
                             "{\"command\":\"subscribe\",\"description\":\"Suscribirse a las alertas de temperatura\"},"
                             "{\"command\":\"unsubscribe\",\"description\":\"Darse de baja de las alertas\"},"
                             "{\"command\":\"server\",\"description\":\"Ver link del dashboard\"}"
                             "]");
  bot.setMyCommands(commands);
}
//------------------------------------------------------------------------------------------------------------------//
void TaskBot(void * parameter) {
  configTime(0, 0, "pool.ntp.org");
  time_t now = time(nullptr);
  
  unsigned long start_time = millis();
  while (now < 24 * 3600 && millis() - start_time < 5000) {
    vTaskDelay(pdMS_TO_TICKS(100)); 
    now = time(nullptr);
  }
  Serial.println("\nTime configured (Core 0).");
  
  bot_setup();
  for (;;) { 
    if (millis() - bot_lasttime > BOT_MTBS) {
      int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
      if (numNewMessages) {handleNewMessages(numNewMessages);}
      if (alertChatIds.size() > 0) {
        if (temperaturaC > UMBRAL_TEMPERATURA && !alertaEnviada) {
          String mensaje = "Precaución, riesgo de descongelación.\nLa temperatura ha subido a *" + String((float)temperaturaC, 1) + "* ºC.";
          for (const String& chatId : alertChatIds) {bot.sendMessage(chatId, mensaje, "Markdown");}
          Serial.println("Alerta de alta temperatura enviada a todos los chats suscritos.");
          alertaEnviada = true;
        }
        if (temperaturaC <= UMBRAL_TEMPERATURA) {
          alertaEnviada = false;
        }
      }
      bot_lasttime = millis();
    }
    vTaskDelay(pdMS_TO_TICKS(30)); 
  }
}
//----------------------------------------------------------------------------------------------------------------------//
void handleRoot() {server.send(200, "text/html", PAGINA_HTML); }

void handleLeer() {
  String valor;
  valor = String((float)temperaturaC, 1) + "," + String((float)humedad, 1);
  server.send(200, "text/plain", valor);
}

void handleNotFound() {server.send(404, "text/plain", "404: Not Found");}
//-----------------------------------------------------------------------------------------------------------------------//
void setup(void) {
  Serial.begin(115200);

  //----------------------------------------------------RTD-------------------------------------------------------------//
  Serial.begin(115200);
  Serial.println("Adafruit MAX31865 PT100 Sensor Test!");

  thermo.begin(MAX31865_3WIRE);
  //--------------------------------------------------------------------------------------------------------------------//
  dht.begin(); 

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT); 

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("\nConectado!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  id = WiFi.localIP().toString();
  loadChatIds();
  xTaskCreatePinnedToCore(TaskBot, "BotTask", 10000, NULL, 2, NULL, 0);

  if (MDNS.begin("esp32")) {
    Serial.println("MDNS iniciado (http://esp32.local)");
  }

  pinMode(BUZZER, OUTPUT);
  digitalWrite(BUZZER, HIGH);

  server.on("/", handleRoot);
  server.on("/leer", handleLeer); 
  server.onNotFound(handleNotFound);
  server.begin();
}
//---------------------------------------------------------------------------------------------------------------//
void loop(void) {
  //----------------------------------------------------RTD-------------------------------------------------------------//
  int time = millis();
  uint16_t rtd = thermo.readRTD();

  temperaturaC = (thermo.temperature(RNOMINAL, RREF));
  temperaturaC = (temperaturaC - 1.52); //correccion de temperatura
  Serial.print("Temperatura: ");
  Serial.print(temperaturaC);
  Serial.print(" °C");
  Serial.println("");


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
    } else {
      Serial.println("Error al leer el sensor DHT22!");
    }
    
    lastRead = millis();
  }

  if (temperaturaC > UMBRAL_TEMPERATURA) {
    digitalWrite(BUZZER, LOW);
  } else {
    digitalWrite(BUZZER, HIGH);
  }

  server.handleClient();
  yield(); 
}
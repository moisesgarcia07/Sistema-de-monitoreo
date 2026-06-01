#include "telegram.h"
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <Preferences.h>
#include <vector>
#include "secret.h"

extern volatile float temperaturaC;
extern volatile float humedad;

WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);

std::vector<String> alertChatIds;
Preferences preferences;

const char NVS_NAMESPACE[] = "telegram_ids";
const char NVS_KEY[] = "chat_list";
const char MASTER_CHAT_ID[] = id_master;

bool alertaEnviada = false;
unsigned long bot_lasttime;
const unsigned long BOT_MTBS = 1000;
String web_url = "No configurado";

//---------------------------------------------Guardar-IDs--------------------------------------------------//
void saveChatIds() {
  String serializedList = "";
  for (size_t i = 0; i < alertChatIds.size(); i++) {
    serializedList += alertChatIds[i];
    if (i < alertChatIds.size() - 1) serializedList += ",";
  }
  preferences.begin(NVS_NAMESPACE, false);
  preferences.putString(NVS_KEY, serializedList);
  preferences.end();
}
//----------------------------------------------------------------------------------------------------------//
void loadChatIds() {
  preferences.begin(NVS_NAMESPACE, true);
  String data = preferences.getString(NVS_KEY, "");
  preferences.end();
  alertChatIds.clear();
  int start = 0, end;
  while ((end = data.indexOf(',', start)) != -1) {
    alertChatIds.push_back(data.substring(start, end));
    start = end + 1;
  } if (start < data.length()) {alertChatIds.push_back(data.substring(start));}
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
//---------------------------------------------------------------------------------------------------------------------------//
void initTelegram() {
  secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT);
  loadChatIds();
}
//---------------------------------------------------------------------------------------------------------------------------//
void taskTelegram(void * parameter) {
  configTime(0, 0, "pool.ntp.org");
  for (;;) { 
    if (millis() - bot_lasttime > BOT_MTBS) {
      int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
      if (numNewMessages) {handleNewMessages(numNewMessages);}
        if (temperaturaC > 28.9 && !alertaEnviada) {
          String mensaje = "Precaución, riesgo de descongelación.\nLa temperatura ha subido a *" + String((float)temperaturaC, 1) + "* ºC.";
          for (const String& chatId : alertChatIds) {bot.sendMessage(chatId, mensaje, "Markdown");}
          Serial.println("Alerta de alta temperatura enviada a todos los chats suscritos.");
          alertaEnviada = true;
        }
        if (temperaturaC <= 28.9) {alertaEnviada = false;}
      bot_lasttime = millis();
    }
    vTaskDelay(pdMS_TO_TICKS(30)); 
  }
}
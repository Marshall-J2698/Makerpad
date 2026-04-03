/*
This firmware is to be used with makerspace esp32 macropad boards (makerspace
proj 24-002). It supports keyboard emulation,string based macros, along with
media control.

Before compiling, ensure that ArduinoJson and StringSplitter have been installed
from the arduino extensions section!

For help or support, go to makerspace.cc/Macropad.
*/
#include "Arduino.h"
#include "FS.h"
#include "StringSplitter.h"
#include <ArduinoJson.h>
#include <LittleFS.h>

#include "html.h"
#include <WebServer.h>
#include <WiFi.h>

#include "USB.h"
#include "USBHIDConsumerControl.h"
#include "USBHIDKeyboard.h"

#include "button.h"
#include "encoder.h"

int classifyOutputType(String userString);
void send_base_html();
void handlePost();
String base_html();
String success_html();

// Setting default values for local server
const char *ssid = "macroboard";
const char *password = "password";

IPAddress local_ip(192, 168, 1, 1);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
WebServer server(80);
String local_address = "192.168.1.1";

// Setting up the various buttons
USBHIDKeyboard Keyboard;
USBHIDConsumerControl Consumer;
Button k1but(33, 50, &Keyboard, &Consumer);
Button k2but(16, 50, &Keyboard, &Consumer);
Button k3but(18, 50, &Keyboard, &Consumer);
Button k4but(35, 50, &Keyboard, &Consumer);
Button rst8(8, 50, &Keyboard, &Consumer);
Button rst0(0, 50, &Keyboard, &Consumer);
Encoder enc;

// Filesystem setup
#define FORMAT_LITTLEFS_IF_FAILED true
StaticJsonDocument<256> prevConfig;
StaticJsonDocument<256> keyDictJson;
File configFile;
File keyFile;
File confRead;

unsigned long curTime;
static bool reset_mode = false;
static bool wifi_setup_has_run = false;

void send_base_html() { server.send(200, "text/html", base_html()); }

void handlePost() {

  configFile = LittleFS.open("/board_config.json", FILE_WRITE);
  StaticJsonDocument<256> configJson;
  configJson["k1output"] = server.arg("k1");
  configJson["k2output"] = server.arg("k2");
  configJson["k3output"] = server.arg("k3");
  configJson["k4output"] = server.arg("k4");
  configJson["LEDmode"] = server.arg("LEDmode");

  configJson["k1type"] = classifyOutputType(server.arg("k1"));
  configJson["k2type"] = classifyOutputType(server.arg("k2"));
  configJson["k3type"] = classifyOutputType(server.arg("k3"));

  serializeJson(configJson, configFile); // write out json to a stored file
  configFile.close();
  server.send(200, "text/html", success_html());
}

int classifyOutputType(String userString) {
  if (userString.length() == 1) {
    return 0;
  } else if (userString.indexOf("KEY_") != -1 &&
             userString.indexOf("+++") == -1) {
    return 1;
  } else if (userString.indexOf("+++") == -1) {
    return 2;
  } else if (userString.indexOf("+++") != -1) {
    return 3;
  } else {
    return 999;
  }
}

void writeKeyDict() {
  keyFile = LittleFS.open("/key_dict.json", FILE_WRITE);
  StaticJsonDocument<256> keyJson;
  String rawData =
      "{\"KEY_LEFT_CTRL\": 128, \"KEY_LEFT_SHIFT\": 129, \"KEY_LEFT_ALT\": "
      "130, \"KEY_LEFT_GUI\": 131, \"KEY_RIGHT_CTRL\": 132, "
      "\"KEY_RIGHT_SHIFT\": 133, \"KEY_RIGHT_ALT\": 134, \"KEY_RIGHT_GUI\": "
      "135, \"KEY_UP_ARROW\": 218, \"KEY_DOWN_ARROW\": 217, "
      "\"KEY_LEFT_ARROW\": 216, \"KEY_RIGHT_ARROW\": 215, \"KEY_MENU\": 254, "
      "\"KEY_SPACE\": 32, \"KEY_BACKSPACE\": 178, \"KEY_TAB\": 179, "
      "\"KEY_RETURN\": 176, \"KEY_ESC\": 177, \"KEY_INSERT\": 209, "
      "\"KEY_DELETE\": 212, \"KEY_PAGE_UP\": 211, \"KEY_PAGE_DOWN\": 214, "
      "\"KEY_HOME\": 210, \"KEY_END\": 213, \"KEY_NUM_LOCK\": 219, "
      "\"KEY_CAPS_LOCK\": 193, \"KEY_F1\": 194, \"KEY_F2\": 195, \"KEY_F3\": "
      "196, \"KEY_F4\": 197, \"KEY_F5\": 198, \"KEY_F6\": 199, \"KEY_F7\": "
      "200, \"KEY_F8\": 201, \"KEY_F9\": 202, \"KEY_F10\": 203, \"KEY_F11\": "
      "204, \"KEY_F12\": 205, \"KEY_F13\": 240, \"KEY_F14\": 241, \"KEY_F15\": "
      "242, \"KEY_F16\": 243, \"KEY_F17\": 244, \"KEY_F18\": 245, \"KEY_F19\": "
      "246, \"KEY_F20\": 247, \"KEY_F21\": 248, \"KEY_F22\": 249, \"KEY_F23\": "
      "250, \"KEY_F24\": 251, \"KEY_PRINT_SCREEN\": 206, \"KEY_SCROLL_LOCK\": "
      "207, \"KEY_PAUSE\": 208, \"LED_NUMLOCK\": 1, \"LED_CAPSLOCK\": 2, "
      "\"LED_SCROLLLOCK\": 4, \"LED_COMPOSE\": 8, \"LED_KANA\": 16}";
  deserializeJson(keyJson, rawData);
  serializeJson(keyJson, keyFile);
  keyFile.close();
}

void setup() {
  if (!LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED)) {
    return;
  }
  pinMode(3, OUTPUT);
  pinMode(5, OUTPUT);
  pinMode(7, OUTPUT);
  USB.begin();
  Keyboard.begin();
  Consumer.begin();
  // if you'd like to change the functionality of the encoder, change the
  // insides of the following if-else statement
  enc.setup(39, 37, [](int state) {
    if (state == 1) {
      Consumer.press(CONSUMER_CONTROL_VOLUME_INCREMENT);
      delay(10);
      Consumer.release();
    } else {
      Consumer.press(CONSUMER_CONTROL_VOLUME_DECREMENT);
      delay(10);
      Consumer.release();
    };
  });
  writeKeyDict();
  confRead = LittleFS.open("/board_config.json");
  DeserializationError conf_error = deserializeJson(prevConfig, confRead);
  File dictFile = LittleFS.open("/key_dict.json");
  DeserializationError dict_error = deserializeJson(keyDictJson, dictFile);

  if (conf_error) {
    StaticJsonDocument<256> configJson;
    configJson["k1output"] =
        "this is the default for k1; bridge pin8 and ground, connect to the "
        "wifi (password=password), then connect to 192.168.1.1";
    configJson["k2output"] = "this is the default for k2";
    configJson["k3output"] = "this is the default for k3";
    configJson["k4output"] = "226";

    configJson["k1type"] = 2;
    configJson["k2type"] = 2;
    configJson["k3type"] = 2;
    configJson["LEDmode"] = "1";
    serializeJson(configJson, configFile); // write out json to a stored file
    configFile.close();
  }
  if (dict_error) {
    writeKeyDict();
  }
  if (prevConfig["LEDmode"] == "3") {
    digitalWrite(3, HIGH);
    digitalWrite(5, HIGH);
    digitalWrite(7, HIGH);
  } else if (prevConfig["LEDmode"] == "0") {
    digitalWrite(3, LOW);
    digitalWrite(5, LOW);
    digitalWrite(7, LOW);
  }
}

void loop() {
  // Setting state upon booting

  if (!reset_mode) {
    curTime = millis();
    k1but.idle(prevConfig["k1type"], prevConfig["k1output"], keyDictJson);
    k2but.idle(prevConfig["k2type"], prevConfig["k2output"], keyDictJson);
    k3but.idle(prevConfig["k3type"], prevConfig["k3output"], keyDictJson);
    k4but.media_if_pressed(prevConfig["k4output"]);
    enc.idle();
    if (prevConfig["LEDmode"] == "1") {
      if (digitalRead(33) == LOW) {
        digitalWrite(3, HIGH);
      } else
        digitalWrite(3, LOW);
      if (digitalRead(18) == LOW) {
        digitalWrite(5, HIGH);
      } else
        digitalWrite(5, LOW);
      if (digitalRead(16) == LOW) {
        digitalWrite(7, HIGH);
      } else
        digitalWrite(7, LOW);
    } else if (prevConfig["LEDmode"] == "2") {
      analogWrite(3, 30 + (cos((curTime / 1000.) * 3.)) * 30);
      analogWrite(5, 30 + (cos((curTime / 1000.) * 3.)) * 30);
      analogWrite(7, 30 + (cos((curTime / 1000.) * 3.)) * 30);
    }
    if (rst8.resetKey() == 1 || rst0.resetKey() == 1) {
      // you can enter reset_mode via boot button or briefly bridging pin 8 and
      // ground this will make a wifi server appear, and the first key output an
      // address
      reset_mode = true;
    }

  } else {
    if (!wifi_setup_has_run) {
      Keyboard.releaseAll();
      WiFi.mode(WIFI_AP);
      WiFi.softAPConfig(local_ip, gateway, subnet);
      WiFi.softAP(ssid, password);
      server.begin();

      delay(3000);
      server.on("/", send_base_html);
      server.on("/sent!", HTTP_POST, handlePost);

      wifi_setup_has_run = true;
    }
    server.handleClient();
    k1but.print_if_pressed(local_address);
  }
}

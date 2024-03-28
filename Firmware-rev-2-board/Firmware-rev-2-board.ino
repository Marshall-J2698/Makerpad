/*
This firmware is to be used with makerspace esp32 macropad boards (makerspace proj 24-002). 
It supports keyboard emulation,string based macros, along with media control. 

Before compiling, ensure that ArduinoJson and StringSplitter have been installed from the arduino extensions section!

For help or support, go to makerspace.cc/Macropad or contact johnsonm3@carleton.edu.

This project took me a lot of research; here are some prominent resources:
https://lastminuteengineers.com/creating-esp32-web-server-arduino-ide/
https://forum.arduino.cc/t/cannot-find-webserver-h-of-esp32-core/961997
https://randomnerdtutorials.com/esp32-write-data-littlefs-arduino/
https://arduinojson.org/v6/api/json/serializejson/
*/
#include "FS.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "StringSplitter.h"

#include <WiFi.h>
#include <WebServer.h>

#include "USB.h"
#include "USBHIDKeyboard.h"
#include "USBHIDConsumerControl.h"

#include "button.h"
#include "encoder.h"
// Setting default values for local server
const char* ssid = "macroboard" ; 
const char* password = "password" ;

IPAddress local_ip(192,168,1,1);
IPAddress gateway(192,168,1,1);
IPAddress subnet(255,255,255,0);
WebServer server(80);
String local_address = "192.168.1.1";

// Setting state upon booting
bool reset_mode = false;
bool wifi_setup_has_run = false;

unsigned long breath_timer = 0;
unsigned long breath_delay = 70;
int LED_change = 1;
int LED_power = 0;


// Setting up the various buttons
USBHIDKeyboard Keyboard;
USBHIDConsumerControl Consumer;
Button k1but(1,50,&Keyboard,&Consumer,12);
Button k2but(2,50,&Keyboard,&Consumer);
Button k3but(3,50,&Keyboard,&Consumer);
Button k4but(4,50,&Keyboard,&Consumer);

Button rst(11,50,&Keyboard,&Consumer);
Encoder enc;

// Filesystem setup
#define FORMAT_LITTLEFS_IF_FAILED true
StaticJsonDocument<256> prevConfig;
StaticJsonDocument<256> keyDictJson;
File configFile;
File keyFile;
File confRead;


void setup() {
  // put your setup code here, to run once:
  if(!LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED)){
      // Serial.println("LittleFS Mount Failed");
      return;
   }
  USB.begin();
  Keyboard.begin();
  Consumer.begin();
  // if you'd like to change the functionality of the encoder, change the insides of the following if-else statement
  enc.setup(5,6,[](int state){
    if(state==1){
      Consumer.press(CONSUMER_CONTROL_VOLUME_INCREMENT);
      delay(10);
      Consumer.release();
    }else{
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

  if(conf_error){
    StaticJsonDocument<256> configJson;
    configJson["k1output"] = "this is the default for k1; bridge pin8 and ground, connect to the wifi (password=password), then connect to 192.168.1.1";
    configJson["k2output"] = "this is the default for k2";
    configJson["k3output"] = "this is the default for k3";
    configJson["k4output"] = "226";
    configJson["LEDMODE"] = 1;

    configJson["k1type"] = 2;
    configJson["k2type"] = 2;
    configJson["k3type"] = 2;
    serializeJson(configJson,configFile); //write out json to a stored file
    configFile.close();
  }
  if(dict_error){
    writeKeyDict();
  }
  if(prevConfig["LEDMODE"]=="3"|prevConfig["LEDMODE"]=="2"){
    // TODO: add all the LEDs
    pinMode(12,OUTPUT);
    if(prevConfig["LEDMODE"]=="3"){
      digitalWrite(12,HIGH);
    }
    
  }
}

void loop() {
  if(!reset_mode){

    k1but.idle(prevConfig["k1type"], prevConfig["k1output"], keyDictJson,atoi(prevConfig["LEDMODE"]));
    k2but.idle(prevConfig["k2type"], prevConfig["k2output"], keyDictJson,atoi(prevConfig["LEDMODE"]));
    k3but.idle(prevConfig["k3type"], prevConfig["k3output"], keyDictJson,atoi(prevConfig["LEDMODE"]));
    if(k4but.media_if_pressed(prevConfig["k4output"])==1&&k1but.resetKey()==1){ //if you press both the volume knob and key1 at once, it will enter reset mode
      reset_mode = true;
    }
    enc.idle();
    // TODO: fix the breathing thing
    if(prevConfig["LEDMODE"]=="2"){
      if(millis()-breath_timer>breath_delay){
        LED_power = LED_power + LED_change;
        analogWrite(12,LED_power);
        
        if(LED_power>251||LED_power<2){
          LED_change = LED_change * -1;
        }
        breath_timer = 0;
      }
    }
    

    if(rst.resetKey()==1){
      // you can also enter reset_mode via briefly bridging pin 8 and ground
      reset_mode = true;
    }



  }else{
    if(!wifi_setup_has_run){
      Keyboard.releaseAll();
      WiFi.begin(ssid,password);
      WiFi.mode(WIFI_AP);
      WiFi.softAP(ssid, password);
      WiFi.softAPConfig(local_ip, gateway, subnet);
      server.begin();
      delay(5000);
      server.on("/",send_base_html);
      server.on("/sent!",HTTP_POST,handlePost);
      wifi_setup_has_run = true;
    }
    server.handleClient();
    k1but.print_if_pressed(local_address);

  }

}
void send_base_html(){
  server.send(200,"text/html",base_html());
}

void handlePost(){
  configFile = LittleFS.open("/board_config.json",FILE_WRITE);
  StaticJsonDocument<256> configJson;
  configJson["k1output"] = server.arg("k1");
  configJson["k2output"] = server.arg("k2");
  configJson["k3output"] = server.arg("k3");
  configJson["k4output"] = server.arg("k4");
  configJson["LEDMODE"] = server.arg("LEDmode");

  configJson["k1type"] = classifyOutputType(server.arg("k1"));
  configJson["k2type"] = classifyOutputType(server.arg("k2"));
  configJson["k3type"] = classifyOutputType(server.arg("k3"));
  serializeJson(configJson,configFile); //write out json to a stored file
  configFile.close();
  server.send(200,"text/html",success_html());
}

int classifyOutputType(String userString){
  if(userString.length()==1){
    return 0;
  }else if(userString.indexOf("KEY_")!=-1&&userString.indexOf("+++")==-1){ 
    return 1;
  }else if(userString.indexOf("+++")==-1){
    return 2;
  }else if(userString.indexOf("+++")!=-1){
    return 3;
  }else{
    return 999;
  }
}



String success_html(){
  String output = "";
  output += "<!DOCTYPE html>\n";
output += "<html lang=\"en\">\n";
output += "  <head>\n";
output += "    <meta charset=\"UTF-8\">\n";
output += "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n";
output += "    <meta http-equiv=\"X-UA-Compatible\" content=\"ie=edge\">\n";
output += "    <title>Macropad UI</title>\n";
output += "    <link rel=\"stylesheet\" href=\"style.css\">\n";
output += "  </head>\n";
output += "\n";
output += "  <body>\n";
output += "    <style>\n";
output += "      body {background-image: linear-gradient(rgb(52, 47, 69),rgb(46, 41, 61))}\n";
output += "      body {min-height: 100vh}\n";
output += "      body {text-align: center}\n";
output += "      body {font-family: 'JetBrains Mono',Verdana, Geneva, Tahoma, sans-serif}\n";
output += "      body {color: lightgray}\n";
output += "    </style>\n";
output += "    <h1>\n";
output += "      Uploaded! unplug and replug macropad for changes to take effect.\n";
output += "    </h1>\n";
output += "  </body>\n";
output += "</html>\n";
  return output;
}

String base_html(){
  String output = "";
output += "<!DOCTYPE html>\n";
output += "<html lang=\"en\">\n";
output += "  <head>\n";
output += "    <meta charset=\"UTF-8\">\n";
output += "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n";
output += "    <meta http-equiv=\"X-UA-Compatible\" content=\"ie=edge\">\n";
output += "    <title>Macropad UI</title>\n";
output += "    <link rel=\"stylesheet\" href=\"style.css\">\n";
output += "  </head>\n";
output += "  <h1>Reprogram your Macropad!</h1>\n";
output += "  <body>\n";
output += "    <style>\n";
output += "      body {background-image: linear-gradient(rgb(52, 47, 69),rgb(46, 41, 61))}\n";
output += "      body {min-height: 100vh}\n";
output += "      body {text-align: center}\n";
output += "      body {font-family: 'JetBrains Mono',Verdana, Geneva, Tahoma, sans-serif}\n";
output += "      body {color: lightgray}\n";
output += "      a:link {color: yellow}\n";
output += "    </style>\n";
output += "    <form action=\"/sent!\" method=\"post\" enctype=\"multipart/form-data\">\n";
output += "    <label for=\"k1\">key 1 string:</label><br>\n";
output += "    <input type=\"text\" id=\"k1\" name=\"k1\" maxlength=\"100\"><br>\n";
output += "    <label for=\"k2\">key 2 string:</label><br>\n";
output += "    <input type=\"text\" id=\"k2\" name=\"k2\" maxlength=\"100\"><br>\n";
output += "    <label for=\"k3\">key 3 string:</label><br>\n";
output += "    <input type=\"text\" id=\"k3\" name=\"k3\" maxlength=\"100\"><br><br>\n";
output += "    <b>Knob Action:</b><br>\n";
output += "    <label for=\"Mute\">Mute</label>\n";
output += "    <input type = radio id=\"Mute\" name=\"k4\" value=\"226\"><br>\n";
output += "    <label for=\"Pause\">Pause</label>\n";
output += "    <input type = radio id=\"Pause\" name=\"k4\" value=\"205\"><br>\n";
output += "    <label for=\"Next Track\">Next Track</label>\n";
output += "    <input type = radio id=\"Next Track\" name=\"k4\" value=\"181\"><br>\n";
output += "    <br>\n";
output += "    <br><b>LED Mode:</b><br>\n";
output += "    <label for=\"No LED actions\">No LED actions</label>\n";
output += "    <input type = radio id=\"No LED actions\" name=\"LEDmode\" value=0><br>\n";
output += "    <label for=\"On Press\">On Press</label>\n";
output += "    <input type = radio id=\"On Press\" name=\"LEDmode\" value=1><br>\n";
output += "    <label for=\"Breath\">Breath</label>\n";
output += "    <input type = radio id=\"Breath\" name=\"LEDmode\" value=2><br>\n";
output += "    <label for=\"Always on\">Always on</label>\n";
output += "    <input type = radio id=\"Always on\" name=\"LEDmode\" value=3><br>\n";
output += "    <br>\n";
output += "    <label for=\"submit_button\">Save changes:</label>\n";
output += "    <input type = \"submit\" id = \"submit_button\" name=\"submit_button\">\n";
output += "\n";
output += "    </form>\n";
output += "    <h3>\n";
output += "      Welcome to the Makerpad reprogramming tool! <br>\n";
output += "      To use, simply enter the string or key that you're interested in sending for a given key, then press submit.<br><br>\n";
output += "      For more info, please visit <a href=\"www.makerspace.cc/macropad\">makerspace.cc/macropad</a> for more detailed instructions\n";
output += "    </h3>\n";
output += "     \n";
output += "    \n";
output += "  </body>\n";
output += "</html>\n";
  return output;

}

void writeKeyDict(){
  keyFile = LittleFS.open("/key_dict.json",FILE_WRITE);
  StaticJsonDocument<256> keyJson;
  String rawData = "{\"KEY_LEFT_CTRL\": 128, \"KEY_LEFT_SHIFT\": 129, \"KEY_LEFT_ALT\": 130, \"KEY_LEFT_GUI\": 131, \"KEY_RIGHT_CTRL\": 132, \"KEY_RIGHT_SHIFT\": 133, \"KEY_RIGHT_ALT\": 134, \"KEY_RIGHT_GUI\": 135, \"KEY_UP_ARROW\": 218, \"KEY_DOWN_ARROW\": 217, \"KEY_LEFT_ARROW\": 216, \"KEY_RIGHT_ARROW\": 215, \"KEY_MENU\": 254, \"KEY_SPACE\": 32, \"KEY_BACKSPACE\": 178, \"KEY_TAB\": 179, \"KEY_RETURN\": 176, \"KEY_ESC\": 177, \"KEY_INSERT\": 209, \"KEY_DELETE\": 212, \"KEY_PAGE_UP\": 211, \"KEY_PAGE_DOWN\": 214, \"KEY_HOME\": 210, \"KEY_END\": 213, \"KEY_NUM_LOCK\": 219, \"KEY_CAPS_LOCK\": 193, \"KEY_F1\": 194, \"KEY_F2\": 195, \"KEY_F3\": 196, \"KEY_F4\": 197, \"KEY_F5\": 198, \"KEY_F6\": 199, \"KEY_F7\": 200, \"KEY_F8\": 201, \"KEY_F9\": 202, \"KEY_F10\": 203, \"KEY_F11\": 204, \"KEY_F12\": 205, \"KEY_F13\": 240, \"KEY_F14\": 241, \"KEY_F15\": 242, \"KEY_F16\": 243, \"KEY_F17\": 244, \"KEY_F18\": 245, \"KEY_F19\": 246, \"KEY_F20\": 247, \"KEY_F21\": 248, \"KEY_F22\": 249, \"KEY_F23\": 250, \"KEY_F24\": 251, \"KEY_PRINT_SCREEN\": 206, \"KEY_SCROLL_LOCK\": 207, \"KEY_PAUSE\": 208, \"LED_NUMLOCK\": 1, \"LED_CAPSLOCK\": 2, \"LED_SCROLLLOCK\": 4, \"LED_COMPOSE\": 8, \"LED_KANA\": 16}";
  deserializeJson(keyJson,rawData);
  serializeJson(keyJson,keyFile);
  keyFile.close();
}




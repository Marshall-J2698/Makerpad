// Debounce code based heavily on arduino samples and advice from Aaron HG.
// also encludes methods for specific media/keyboard macros
// copying
#include "Arduino.h"
#include "USBHIDKeyboard.h"
#include <ArduinoJson.h>
#include "StringSplitter.h"

class Button {
public:

  int pin;
  int but_read;
  bool but_state;
  int fired = LOW;
  int last_state = LOW;
  USBHIDKeyboard* Keyboard;
  USBHIDConsumerControl* Consumer;
  unsigned long key_delay;
  unsigned long debounce_time;


  Button(int pin_num, unsigned long debounce_delay, USBHIDKeyboard* Keyboard_in, USBHIDConsumerControl* Consumer_in) {
    pin = pin_num;
    debounce_time = debounce_delay;
    Keyboard = Keyboard_in;
    Consumer = Consumer_in;
    pinMode(pin, INPUT_PULLUP);
  }

  void idle(int output_type, String output, StaticJsonDocument<256> key_dict) {
    // The output type tells us what type of data the key is sending to the computer:
    // 0 = single key press (standard key input)
    // 1 = single key press (modifier/special key)
    // 2 = string of standard characters
    // 3 = single press combination (ex: KEY_ARROW_DOWN+++a) - only sent once, no hold support
    // 999 = unknown type; please reinput your string in the website
    if (output_type == 0) {
      print_if_pressed(output);
    } else if (output_type == 1) {
      mod_if_pressed(output, key_dict);
      // print_if_pressed("we made it to elseif!");
    } else if (output_type == 2) {
      print_if_pressed(output);
    } else if (output_type == 3) {
      press_combo(output, key_dict);
    }
  }

  int media_if_pressed(String output) {
    if (digitalRead(pin) != but_state) {
      but_state = !but_state;
      key_delay = millis();
    }
    if (millis() - key_delay > debounce_time && but_state != fired) {
      fired = but_state;

      if (!but_state) {
        Consumer->press(output.toInt());
        return 1;
      } else {
        Consumer->release();
        return 0;
      }
    }
    return 0;
  }

  void mod_if_pressed(String output, StaticJsonDocument<256> key_dict) {
    if (digitalRead(pin) != but_state) {
      but_state = !but_state;
      key_delay = millis();
    }
    if (millis() - key_delay > debounce_time && but_state != fired) {
      fired = but_state;

      if (!but_state) {
        Keyboard->press(key_dict[output]);
      } else Keyboard->releaseAll();
    }
  }

  void press_combo(String user_arg, StaticJsonDocument<256> key_dict) {
    if (user_arg.indexOf("+++") != -1) {
      StringSplitter* splitter = new StringSplitter(user_arg, '+++', 5);
      int count = splitter->getItemCount();
      if (digitalRead(pin) != but_state) {
        but_state = !but_state;
        key_delay = millis();
      }
      if (millis() - key_delay > debounce_time && but_state != fired) {
        fired = but_state;

        if (!but_state) {
          for (int i = 0; i < count; i++) {
            if (key_dict.containsKey(splitter->getItemAtIndex(i))) {
              Keyboard->press(key_dict[splitter->getItemAtIndex(i)]);
            } else {
              for (int k = 0; k < splitter->getItemAtIndex(i).length(); k++) {
                Keyboard->press(splitter->getItemAtIndex(i)[k]);
                delay(10);
                Keyboard->release(splitter->getItemAtIndex(i)[k]);
                delay(10);
              }
            }
          }
          Keyboard->releaseAll();
        } else Keyboard->releaseAll();
      }
      delete splitter;
    }
  }





  void print_if_pressed(String output) {
    if (digitalRead(pin) != but_state) {
      but_state = !but_state;
      key_delay = millis();
    }
    if (millis() - key_delay > debounce_time && but_state != fired) {
      fired = but_state;
      if (output.length() == 1) {
        if (!but_state) Keyboard->press(output[0]);
        else Keyboard->releaseAll();
      } else {
        if (!but_state) {
          for (int i = 0; i < output.length(); i++) {

            Keyboard->press(output[i]);
            delay(10);
            Keyboard->release(output[i]);
            delay(10);
          }
        }
      }
    }
  }
  void send_one_special(String keycode, StaticJsonDocument<256> key_dict) {
    int keycode_num = key_dict[keycode];
    if (digitalRead(pin) != but_state) {
      but_state = !but_state;
      key_delay = millis();
    }
    if (millis() - key_delay > debounce_time && but_state != fired) {
      fired = but_state;
      if (!but_state) Keyboard->press(keycode_num);
      else Keyboard->releaseAll();
    }
  }
  void del_macro() {
    if (digitalRead(pin) != but_state) {
      but_state = !but_state;
      key_delay = millis();
    }
    if (millis() - key_delay > debounce_time && but_state != fired) {
      fired = but_state;

      if (!but_state) {
        Keyboard->press(KEY_DELETE);
      } else Keyboard->releaseAll();
    }
  }
  void copy_macro() {
    if (digitalRead(pin) != but_state) {
      but_state = !but_state;
      key_delay = millis();
    }
    if (millis() - key_delay > debounce_time && but_state != fired) {
      fired = but_state;

      if (!but_state) {
        Keyboard->press(KEY_LEFT_CTRL);
        Keyboard->press('c');
      } else Keyboard->releaseAll();
    }
  }

  void paste_macro() {
    if (digitalRead(pin) != but_state) {
      but_state = !but_state;
      key_delay = millis();
    }
    if (millis() - key_delay > debounce_time && but_state != fired) {
      fired = but_state;

      if (!but_state) {
        Keyboard->press(KEY_LEFT_CTRL);
        Keyboard->press('v');
      } else Keyboard->releaseAll();
    }
  }

  int resetKey() {
    if (digitalRead(pin) != but_state) {
      but_state = !but_state;
      key_delay = millis();
    }
    if (millis() - key_delay > debounce_time && but_state != fired) {
      fired = but_state;

      if (!but_state) {
        return 1;

      } else {
        return 0;
      }
    }
  }
};

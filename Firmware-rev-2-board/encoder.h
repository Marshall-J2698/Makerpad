// #Library provided by Aaron HG. Thanks!

#ifndef BUTTON
#define BUTTON

#include "arduino.h"


  template <typename CT, typename ... A> class fxn
  : public fxn<decltype(&CT::operator())()> {};
  
  class dEncFunc{
    public:
      virtual void operator()(int);
  };
  
  template <typename C> class fxn<C>: public dEncFunc {
  private:
      C mObject;
  
  public:
      fxn(const C & obj) : mObject(obj) {}
  
      void operator()(int a) {
          this->mObject(a);
      }
  };

  class Encoder;

  static int size = 0;
  static Encoder * instances[10];

void encoderPinChange();

class Encoder {
public:
  int state;
  int steps;
  int pins[2];
  int states[2];
  int old[2];
  bool lambda;
  
  void (*callback)(int state);
  dEncFunc* pressCB;

  Encoder(){
    instances[size++] = this;
    state = 1;
    steps = 0;
    pressCB = NULL;
    callback = NULL;
  }


  void setup(int p1, int p2, void (* cb)(int)) {
    setCallback(cb);
    pins[0] = p1;
    pins[1] = p2;
    pinMode(p1, INPUT_PULLUP);
    pinMode(p2, INPUT_PULLUP);
    state = digitalRead(p1);
    attachInterrupt(p1, encoderPinChange, CHANGE);
  }

  template<typename C>
  void setup(int p1, int p2, const C & cb) {
    setCallback(cb);
    pins[0] = p1;
    pins[1] = p2;
    pinMode(p1, INPUT_PULLUP);
    pinMode(p2, INPUT_PULLUP);
    state = digitalRead(p1);
    attachInterrupt(p1, encoderPinChange, CHANGE);
  }

  template<typename C> 
  void setCallback( const C & cb){
    pressCB = new fxn<C>(cb);
  }

  void setCallback( void (* cb)(int)){
    callback = cb;
  }

  void call(int state){
    if(pressCB) (*pressCB)(state);
    else if(callback) callback(state);
  }

  void check(){
    if(digitalRead(pins[0])!=state){
      state = !state;
      if(state != digitalRead(pins[1])) steps++;
      else steps--;
    }
  }
  
  void idle(){
    for(int i=0; i<abs(steps); i++){
      call(steps>0);
    }
    steps = 0;
  }
};

void encoderPinChange(){
  for(int i=0; i<size; i++){
    instances[i]->check();
  }
}

#endif

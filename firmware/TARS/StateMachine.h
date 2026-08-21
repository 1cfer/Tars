#ifndef STATEMACHINE_H
#define STATEMACHINE_H

#include <Arduino.h>
#include "State.h"
#include "ButtonHandler.h"

struct Flags { bool inicio = false; bool lectura = false; bool envio = false; bool dev = false; bool envio_programado = false; };
struct Clocks { unsigned long tiempo_lectura = 0; unsigned long tiempo_actual = 0; unsigned long ultima_interaccion = 0; unsigned long proximo_envio = 0; };
struct SensorData { float temp = 0.0; float hum = 0.0; float lux = 0.0; float dbValue = 0.0; float voltage = 0.0; };

class StateMachine {
private:
    State* currentState;
public:
    Flags flags;
    Clocks clocks;
    SensorData sensors;
    volatile int screenMode = 0;
    bool isDisplayOn = true;
    bool needsUpdate = false;
    
    // EL PUENTE: Los estados leerán esta variable sin alterar sus firmas originales
    ButtonHandler::Event currentEvent = ButtonHandler::NONE; 

    StateMachine();
    ~StateMachine();
    void begin(State* initialState);
    void update(); 
    void ChangeState(State* newState);
    State* getCurrentState() const { return currentState; }
    const char* getCurrentStateName();
};
#endif
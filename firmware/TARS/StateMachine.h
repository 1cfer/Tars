#ifndef STATEMACHINE_H
#define STATEMACHINE_H

#include <Arduino.h>

#include "State.h"

// Estructura de banderas
struct Flags {
  bool inicio = false;
  bool lectura = false;
  bool envio = false;
  bool dev = false;
  bool envio_programado = false;
};

// Estructura de tiempos
struct Clocks {
  unsigned long tiempo_lectura = 0;
  unsigned long tiempo_actual = 0;
  unsigned long ultima_interaccion = 0;
  unsigned long proximo_envio = 0;
};

// Estructura de datos de sensores
struct SensorData {
  float temp = 0.0;
  float hum = 0.0;
  float lux = 0.0;
  float dbValue = 0.0;
  float voltage = 0.0;
  uint16_t pm1_0 = 0;
  uint16_t pm2_5 = 0;
  uint16_t pm10 = 0;
};

class StateMachine {
 private:
  State* currentState;

 public:
  // Definimos las estructuras globales
  Flags flags;
  Clocks clocks;
  SensorData sensors;

  volatile int screenMode = 0;
  bool isDisplayOn = true;
  bool needsUpdate = false;

  // Constructor de la máquina de estados
  StateMachine();
  ~StateMachine();

  // Metodos principales
  void begin(State* initialState);
  void update();
  void ChangeState(State* newState);

  // Getters de los estados actual y previo
  State* getCurrentState() const { return currentState; }
  const char* getCurrentStateName();
};

#endif

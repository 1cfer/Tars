// Plantilla principal de las clases de estado

#ifndef STATE_H
#define STATE_H

#include <Arduino.h>

class StateMachine;

class State {
 protected:
  StateMachine* statemachine;  // Puntero a la máquina de estados

 public:
  virtual void setStateMachine(StateMachine* sm) { statemachine = sm; }

  // Funciones que se deben implementar en cada estado

  virtual void onEnter() = 0;  // Lógica al entrar en el estado
  virtual void execute() = 0;  // Lógica principal del estado, la que se ejecuta en cada iteración
  virtual void onExit() = 0;   // Lógica al salir del estado

  virtual const char* getName() = 0;  // Obtener el nombre del estado
};

#endif
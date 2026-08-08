#include "StateMachine.h"

StateMachine::StateMachine() {
  currentState = nullptr;
}

StateMachine::~StateMachine() {
  if (currentState) {
    currentState->onExit();
    delete currentState;
  }
}

void StateMachine::begin(State* initialState) {
  if (initialState) {
    currentState = initialState;
    currentState->setStateMachine(this);
    currentState->onEnter();

    Serial.print("Estado inicial: ");
    Serial.println(currentState->getName());
  }
}

void StateMachine::update() {
  if (currentState) {
    clocks.tiempo_actual = millis();
    currentState->execute();
  }
}

void StateMachine::ChangeState(State* newState) {
  if (newState && newState != currentState) {
    Serial.print("Transición: ");
    Serial.print(currentState ? currentState->getName() : "NULL");
    Serial.print(" -> ");
    Serial.println(newState->getName());

    if (currentState) {
      currentState->onExit();
      delete currentState;
    }

    currentState = newState;
    currentState->setStateMachine(this);
    currentState->onEnter();
  }
}

const char* StateMachine::getCurrentStateName() { return currentState ? currentState->getName() : "NULL"; }
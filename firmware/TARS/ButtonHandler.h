#ifndef BUTTONHANDLER_H
#define BUTTONHANDLER_H

#include <Arduino.h>

class ButtonHandler {
public:
  enum Event { NONE, SHORT_PRESS, LONG_PRESS };

private:
  uint8_t pin;
  bool wasPressed = false;       // Estaba presionado en el ciclo anterior?
  bool longPressHandled = false; // Ya disparamos el long press?
  unsigned long pressedAt = 0;   // Cuando empezo a presionar?

  static const unsigned long DEBOUNCE_TIME = 50;
  static const unsigned long LONG_PRESS_TIME = 4000;

public:
  ButtonHandler(uint8_t buttonPin) { pin = buttonPin; }

  void begin() { pinMode(pin, INPUT_PULLUP); }

  Event update() {
    bool pressed = digitalRead(pin) == LOW;
    Event event = NONE;

    // Acaba de presionar
    if (pressed && !wasPressed) {
      pressedAt = millis();
      longPressHandled = false;
    }

    // Está manteniendo presionado y ya pasaron 5 seg
    if (pressed && !longPressHandled &&
        (millis() - pressedAt >= LONG_PRESS_TIME)) {
      longPressHandled = true;
      event = LONG_PRESS;
    }

    // Acaba de soltar
    if (!pressed && wasPressed) {
      // Solo cuenta como short press si no fue long press y pasó el debounce
      if (!longPressHandled && (millis() - pressedAt >= DEBOUNCE_TIME)) {
        event = SHORT_PRESS;
      }
    }

    wasPressed = pressed;
    return event;
  }
};

extern ButtonHandler buttonHandler;

#endif
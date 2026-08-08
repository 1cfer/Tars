#ifndef DISPLAYMANAGER_H
#define DISPLAYMANAGER_H

#include <Arduino.h>

// Dibuja los 4 sensores en pantalla (modo resumen)
void drawAllSensors();

// Refresca la pantalla según el screenMode activo
void updateDisplay();

// Dibuja el header con estado actual e IP en la zona amarilla
void displayStateInfo(const char* estado);

// Pantalla especial del modo desarrollador
void displayDeveloperInfo();

#endif

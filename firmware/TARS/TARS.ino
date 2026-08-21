#include <Adafruit_GFX.h> 
#include <Adafruit_SSD1306.h> 
#include <ArduinoJson.h> 
#include <DFRobot_B_LUX_V30B.h> 
#include <EEPROM.h> 
#include <ESPmDNS.h> 
#include <HTTPClient.h> 
#include <WebServer.h> 
#include <WiFi.h> 
#include <Wire.h> 

#include "AppConfig.h" 
#include "ButtonHandler.h" 
#include "ClosedCube_HDC1080.h" 
#include "DisplayManager.h" 
#include "Estados.h" 
#include "PayloadBuilder.h" 
#include "SensorManager.h" 
#include "State.h" 
#include "StateMachine.h" 
#include "TokenManager.h" 
#include "WiFiManager.h"

#define SCREEN_WIDTH 128 
#define SCREEN_HEIGHT 64 
#define BUTTON_PIN 27 

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1); 
ClosedCube_HDC1080 hdc1080; 
DFRobot_B_LUX_V30B luxSensor(&Wire, 5); // Tu librería preferida
WebServer server(80); 
AppConfig appConfig; 
WiFiManager wifiManager; 
TokenManager tokenManager; 
ButtonHandler buttonHandler(BUTTON_PIN); 
StateMachine stateMachine; 
SensorManager sensorManager; 

SET_LOOP_TASK_STACK_SIZE(16384); 

void setup() { 
  Serial.begin(115200); 
  Wire.begin(21, 22);
  Wire.setTimeOut(1000); 
  
  hdc1080.begin(0x40); 
  Wire.setClock(50000); 
  luxSensor.begin(); 
  analogReadResolution(12);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) while (1); 

  buttonHandler.begin();
  appConfig.begin();
  sensorManager.begin();
  stateMachine.begin(new EstadoINICIO()); 
}

void loop() { 
  ButtonHandler::Event event = buttonHandler.update();

  if (event != ButtonHandler::NONE) { 
    // Actualizamos el reloj de interacción siempre que se presione algo
    stateMachine.clocks.ultima_interaccion = millis();
    
    // Si la pantalla estaba apagada, la despertamos
    if (!stateMachine.isDisplayOn) {
        stateMachine.isDisplayOn = true;
        stateMachine.needsUpdate = true;
        
        // REQUERIMIENTO: Si el evento fue un clic corto para despertar la pantalla, 
        // lo anulamos aquí mismo para que NO avance a la siguiente vista de sensores.
        if (event == ButtonHandler::SHORT_PRESS) {
            event = ButtonHandler::NONE; 
        }
    }
  }

  // Puente: Pasamos el evento a los estados para que ellos decidan qué hacer.
  // Si anulamos el clic corto arriba, a la máquina de estados llegará como "NONE"
  // y la pantalla se quedará exactamente donde la dejaste.
  stateMachine.currentEvent = event;
  stateMachine.update(); 
  stateMachine.currentEvent = ButtonHandler::NONE; // Limpiamos el evento
}
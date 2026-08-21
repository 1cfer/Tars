#ifndef WIFIMANAGER_H 
#define WIFIMANAGER_H

#include <Arduino.h> 
#include <Preferences.h> 
#include <WiFi.h>

class WiFiManager { 
private: 
    Preferences prefs;
    const char *apPass = "12345678";
    unsigned long lastReconnectAttempt = 0; 
    static const unsigned long RECONNECT_INTERVAL = 30000;

    // Busca si una red está en el llavero de 3 slots
    int findSlot(String targetSSID) {
        for (int i = 0; i < 3; i++) {
            String k = "s" + String(i);
            if (prefs.getString(k.c_str(), "") == targetSSID) return i;
        }
        return -1;
    }

public: 
    // Auto-conexión general al arrancar (Usa la última exitosa)
    bool connect(int maxAttempts = 16) { 
        prefs.begin("agevital", true); 
        String ssid = prefs.getString("lastSSID", ""); 
        String pass = prefs.getString("lastPASS", ""); 
        prefs.end();

        if (ssid.length() == 0) return false;

        Serial.printf("[WiFiManager] Autoconectando a %s...\n", ssid.c_str());
        WiFi.begin(ssid.c_str(), pass.c_str());
        
        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < maxAttempts) {
            delay(500);
            Serial.print(".");
            attempts++;
        }
        
        if (WiFi.status() == WL_CONNECTED) return true;
        return false;
    }

    // Conectar a una red ESPECÍFICA desde la lista OLED
    bool connectTo(String targetSSID, int maxAttempts = 16) {
        prefs.begin("agevital", true);
        int slot = findSlot(targetSSID);
        if (slot == -1) { prefs.end(); return false; }
        String pass = prefs.getString(("p" + String(slot)).c_str(), "");
        prefs.end();

        Serial.printf("[WiFiManager] Conectando a %s...\n", targetSSID.c_str());
        WiFi.begin(targetSSID.c_str(), pass.c_str());

        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < maxAttempts) {
            delay(500);
            Serial.print(".");
            attempts++;
        }
        
        if (WiFi.status() == WL_CONNECTED) {
            // Si tiene éxito, la marcamos como red prioritaria
            prefs.begin("agevital", false);
            prefs.putString("lastSSID", targetSSID);
            prefs.putString("lastPASS", pass);
            prefs.end();
            return true;
        }
        return false;
    }

    void createAP(const String &hostname) { 
        String apSSID = "TARS-" + hostname; 
        WiFi.mode(WIFI_AP); 
        WiFi.softAP(apSSID.c_str(), apPass); 
    }

    void maintainConnection() { 
        if (WiFi.status() == WL_CONNECTED) return;
        unsigned long now = millis();
        if (now - lastReconnectAttempt >= RECONNECT_INTERVAL) {
            lastReconnectAttempt = now;
            prefs.begin("agevital", true);
            if (prefs.getString("lastSSID", "").length() > 0) {
                WiFi.reconnect();
            }
            prefs.end();
        }
    }

    // Guarda múltiples redes sin borrar las anteriores
    void saveCredentials(const String &newSSID, const String &newPass) { 
        prefs.begin("agevital", false); 
        prefs.putString("lastSSID", newSSID); 
        prefs.putString("lastPASS", newPass); 
        
        bool found = false;
        int freeSlot = 0;
        for(int i=0; i<3; i++) {
            String s = prefs.getString(("s"+String(i)).c_str(), "");
            if(s == newSSID) { found = true; prefs.putString(("p"+String(i)).c_str(), newPass); break; }
            if(s == "") freeSlot = i;
        }
        if(!found) {
            prefs.putString(("s"+String(freeSlot)).c_str(), newSSID);
            prefs.putString(("p"+String(freeSlot)).c_str(), newPass);
        }
        prefs.end(); 
    }

    void reset() { 
        prefs.begin("agevital", false); 
        prefs.clear(); 
        prefs.end(); 
    }

    // Averigua si el llavero tiene la clave para dibujar el Asterisco (*)
    bool hasCredentialsFor(String targetSSID) { 
        prefs.begin("agevital", true); 
        int slot = findSlot(targetSSID);
        prefs.end(); 
        return slot != -1; 
    }

    bool isConnected() { return WiFi.status() == WL_CONNECTED; }
    String getIP() { 
        if (WiFi.status() == WL_CONNECTED) return WiFi.localIP().toString(); 
        if (WiFi.getMode() == WIFI_AP) return WiFi.softAPIP().toString(); 
        return "No conectado"; 
    }
    String getSSID() { 
        if (WiFi.status() == WL_CONNECTED) return WiFi.SSID(); 
        return ""; 
    } 
};

extern WiFiManager wifiManager;
#endif
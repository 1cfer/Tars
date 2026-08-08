# TARS — Sistema Modular de Monitoreo Ambiental ESP32

Plataforma embebida basada en ESP32 para monitoreo ambiental multisensor, con arquitectura de máquina de estados, dashboard web en tiempo real y diseño de hardware propio (PCB v1.5). Desarrollado en el Semillero AgeVital, Universidad Pontificia Bolivariana.

![Plataforma](https://img.shields.io/badge/plataforma-ESP32-blue)
![Lenguaje](https://img.shields.io/badge/lenguaje-C++-orange)
![Dashboard](https://img.shields.io/badge/dashboard-Flask%20%2B%20ApexCharts-lightgrey)
![PCB](https://img.shields.io/badge/hardware-PCB%20v1.5-9cf)
![Licencia](https://img.shields.io/badge/licencia-MIT-green)

## Descripción

TARS integra múltiples sensores ambientales en un único dispositivo ESP32, con una arquitectura de firmware modular (máquina de estados + separación de responsabilidades) y un dashboard web local para visualización, alertas y exportación de datos. El sistema evolucionó de un prototipo en breadboard a una PCB personalizada con conectores USB-C y JST, evaluando materiales de construcción en condiciones reales (Ecovilla UPB).

## Características

- 🌡️ Temperatura y humedad (doble bus I2C, SHT31)
- 💧 Temperatura de líquidos (DS18B20, OneWire)
- 🌊 Caudal de flujo (FS400A)
- 🌱 Humedad de suelo vía Modbus RS485 (JXBS-3001)
- 💡 Luminosidad calibrada (filtro de anomalías + mediana móvil)
- 🔊 Nivel de ruido (sensor analógico calibrado)
- 🖥️ Pantalla OLED con navegación multi-pantalla por botón físico
- 📡 Envío periódico a FIWARE Orion Context Broker (OAuth2 / Keyrock)
- 🌐 Dashboard web (Flask + ApexCharts) con widgets drag-and-drop, exportación CSV, alertas y estadísticas — funciona offline
- ⚙️ Modo desarrollador con portal de configuración OTA (WiFi, intervalos, servidor)
- 🔧 PCB propia (v1.5) con conectores USB-C y JST, diseñada en Altium Designer

## Arquitectura del firmware

```
StateMachine
├── EstadoINICIO       → conexión WiFi, primer boot
├── EstadoLECTURA       → lectura periódica de sensores + refresco de pantalla
├── EstadoENVIO         → PATCH a Orion / POST a agente Flask
└── EstadoDESARROLLADOR → portal web de configuración (AP o STA)
```

Cada sensor se lee y valida en `SensorManager` (con filtros de rango y detección de anomalías por sensor), se acumula para promedio en el intervalo de envío, y se serializa en `PayloadBuilder` siguiendo el modelo de entidades NGSI de FIWARE.

## Estructura del repositorio

```
TARS/
├── firmware/
│   └── TARS/              → sketch principal (Arduino/ESP32)
├── dashboard/              → interfaz web de visualización (HTML/CSS/JS)
├── hardware/                → esquemáticos y diseño de PCB (Altium)
├── docs/                    → capturas, fotos, diagramas
├── README.md
└── LICENSE
```

## Hardware

| Componente | Función | Bus/Conexión |
|---|---|---|
| ESP32 DevKit | Controlador principal | — |
| SHT31 (x2) | Temp/humedad ambiente | I2C dual bus |
| DS18B20 | Temp de líquidos | OneWire |
| FS400A | Caudal de flujo | Pulsos digitales |
| JXBS-3001 | Humedad de suelo | RS485 Modbus |
| Sensor de luz | Luminosidad | Analógico/I2C |
| Sensor de sonido | Nivel de ruido | Analógico |
| OLED SSD1306 128x64 | Interfaz visual | I2C |
| Botón pulsador | Navegación de pantallas | GPIO digital |

### PCB v1.5

Diseño propio en Altium Designer, evolucionado desde el prototipo en breadboard (TARS_V1, Código 106394). Incluye conectores USB-C para alimentación/programación y JST para conexión modular de sensores, facilitando el mantenimiento en campo.

![PCB TARS v1.5](docs/pcb-v1.5.jpg)
![Esquemático](docs/esquematico.png)

## Dashboard

Interfaz web local (arquitectura de 3 archivos: HTML/CSS/JS) que consume los datos enviados por TARS, con:

- Widgets arrastrables y personalizables
- Gráficas en tiempo real con ApexCharts
- Exportación de históricos a CSV
- Sistema de alertas por umbral
- Funcionamiento offline (sin dependencia de CDN externos)

![Dashboard TARS](docs/dashboard-preview.png)

## Instalación

### Firmware

1. Instala las librerías necesarias desde el Gestor de Librerías del IDE de Arduino:
   - `Adafruit GFX Library`, `Adafruit SSD1306`
   - `ArduinoJson`
   - `OneWire`, `DallasTemperature` (DS18B20)
   - Librería del sensor SHT31 correspondiente
   - Librería Modbus RS485 para JXBS-3001
2. Abre `firmware/TARS/TARS.ino`
3. Selecciona la placa ESP32 en `Herramientas > Placa`
4. Sube el código
5. En el primer arranque, conéctate al AP generado y configura tu red WiFi desde el portal de configuración

### Dashboard

```bash
cd dashboard
# instrucciones según tu setup (servidor estático, Flask, etc.)
```

## Créditos

Proyecto desarrollado por [Fer](https://github.com/1cfer) — Semillero AgeVital, Universidad Pontificia Bolivariana. Prototipo original (TARS_V1) supervisado por el tutor Henry Andrade Caicedo, evaluado en Ecovilla UPB.

## Licencia

MIT — ver [LICENSE](LICENSE)

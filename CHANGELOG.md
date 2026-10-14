# Cambios

## 4.1.1

- Sensor confirmado SEN0232: OLED/panel/telemetría en dBA, con ponderación A y constante de hardware 125 ms.
- Conversión de fábrica 50 dBA/V, ajustes locales conservados y valores fuera de rango como null.
- Calidad distingue conversión de fábrica y contraste de campo, manteniendo compatibilidad con el receptor.
- Promedio de muestras documentado como distinto de Leq; datos antiguos en cola conservados.
- Sin pruebas físicas de calibración.

## 4.1.0

- Vinculación AMERI pairing 1 desde el panel, con código temporal y petición persistente idéntica en reintentos.
- Validación estricta de firma HMAC ASCII, ID/MAC/perfil y payload JSON exacto.
- Commit atómico que conserva configuración; base OTA anclada en sidecar compatible con rollback.
- Histórico optativo en un destino libre, sin sustituir Orion ni redirigir pendientes.
- AP inicial solo para equipo sin ID/WiFi; claves existentes conservadas y clave individual tras pairing.
- Panel comprimido para caber en el slot OTA actual, sin cambiar particiones.
- Consulta privada de nueva clave de mantenimiento y estado redactado de vinculación/contacto.
- Criptografía control V2 y OTA AMERI1 conservada.

Pendiente de pruebas físicas y de despliegue del receptor en Raspberry.

## 4.0.4

- Botón 10 s fuerza AP 10 min; botón 4 s mantiene acceso normal.
- Firmware genérico, cuatro destinos, OLED/I2C, documentación y diagramas.

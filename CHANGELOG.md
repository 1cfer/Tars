## 4.1.8 — autenticación del panel

Restaura desafíos Digest en recursos auxiliares y mantiene nonce/opaque por sesión para evitar invalidación entre peticiones. Conserva CSRF, contraseña e interfaz. Sin cambios de pines o drivers. Pruebas host; navegador y PCB físicos pendientes.

## 4.1.7 — recuperación NVS

Pausa escrituras fallidas, conserva AP y sesión durante recuperación y publica ocupación NVS por namespace. Recuperación manual autenticada elimina únicamente copias legadas con blob válido. Conserva diarios/secuencias y reduce el diario de control después de confirmar entrega. No cambia particiones. Pruebas simuladas; validar en un equipo físico antes de desplegar.

# Cambios

## 4.1.6

- Configuración y copia de recuperación como blobs NVS multipágina, con commit y lectura de verificación.
- Lectura de cadenas antiguas sin borrar el original; copia de recuperación prioritaria y fallo cerrado ante blobs inválidos.
- Recuperación diferida hasta validación de imagen OTA, con reintentos limitados.
- Protección de OTA local/servidor contra versiones anteriores a 4.1.6 tras usar el nuevo formato.
- Configuración pública, secretos, calibración, WiFi, solicitud y colas conservados.
- Pruebas simuladas de lectura/migración, fallos de escritura y recuperación; falta prueba física.

## 4.1.5

- Compactación de configuración privada sin perder ajustes/secretos; JSON público intacto.
- Mismo namespace, clave y tipo NVS; compatibilidad de lectura con versiones anteriores.
- Errores NVS concretos de apertura/escritura/commit y comprobación de lectura antes de aplicar en RAM.
- Diagnóstico sin datos privados, sin borrados ni cambios de WiFi, credenciales, calibración o colas.
- Transporte TLS normal y refresco OLED conservados; se retira la prueba temporal NETTEST tras obtener HTTP 200.
- Pruebas simuladas de serialización y fallos de almacenamiento; falta verificar el guardado en hardware.

## 4.1.4

- Prueba TCP/TLS armada por NETTEST, ejecutada una vez por equipo/version con marcador NVS independiente.
- Cierre TCP antes de TLS; mismo cliente y CA para handshake y POST, con comprobación de identidad en HTTPClient.
- Diagnóstico separado de preparación TLS y handshake, previo a limpieza: UTC, timeouts, memoria y errores crudos.
- Conservados certificados, validación, hostname/SNI, límites de tiempo, configuración, petición pendiente y colas.
- Helper SHA-256 servido en gzip para reducir tamaño; bytes y vectores de hash verificados.
- Sin validación física de conexión/OTA; requiere registro del equipo para identificar el fallo.

## 4.1.3

- Diagnóstico de vinculación antes de limpiar TLS: DNS/IP usados, UTC, duración, heap libre y mayor bloque libre, error mbedTLS y errno. Sin credenciales en los mensajes.
- Conservación de CA/SNI, tiempos límite, solicitud pendiente, configuración y persistencia.
- Estado serie cada 10 s con vencimiento único y sin recuperación en ráfaga.
- Actualización OLED por regiones modificadas, sin transferencias de imágenes idénticas; reintento completo tras NACK, I²C a 50 kHz.
- Pruebas simuladas de orden de captura, redacción de registros, planificación del estado e imagen diferencial. Falta diagnóstico HTTPS y evaluación visual en hardware.

## 4.1.2

- Corrección de la inanición de IDLE0 observada al vincular por HTTPS: tareas de red y OTA a prioridad IDLE con planificación preventiva y reparto de tiempo requeridos en compilación.
- Watchdog, límites de tiempo TLS/HTTP, frecuencia de CPU y medición independiente conservados.
- Sin migraciones ni borrado de WiFi, credenciales, calibración, colas o petición pendiente.
- Prueba de regresión de prioridades y modelo de planificación. HTTPS en hardware pendiente de validar.

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

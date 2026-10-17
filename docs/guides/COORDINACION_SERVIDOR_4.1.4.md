# Texto para coordinar con el servidor

Estamos probando TARS 4.1.4 en ESP32-WROOM-DA con Arduino ESP32 3.3.12.
No cambies certificados, autenticación ni rutas por suposición. No pidas códigos,
claves HMAC, contraseñas o tokens en registros compartidos.

La prueba se arma con NETTEST en serie y se ejecuta una vez por equipo/version.
Primero abre TCP a la IP resuelta del hostname y al puerto 10000; lo cierra
sin enviar HTTP. Después prepara TLS y realiza el handshake con hostname
original para SNI/X.509 y la CA activa usada por la vinculación. El POST usa
ese mismo cliente si TLS tiene éxito. Por tanto, una primera conexión cerrada
sin petición HTTP forma parte de la prueba y no es un error de la aplicación.

Correlaciona UTC/IP/puerto del registro con conexiones TCP/TLS del punto que
termina realmente HTTPS. Los paquetes internos de la Raspberry no prueban
por sí solos que el TLS externo haya llegado desde la ESP. Distingue:
TCP sin respuesta, preparación TLS fallida, handshake fallido/caducado,
TLS correcto pero POST ausente, o POST recibido con respuesta HTTP.

Contrato vigente, sin introducir otro protocolo:
- POST https://infver.tail8e92a9.ts.net:10000/api/pairing/devices/tars_1/claim
  Vinculación protocol=1, sobre firmado y bytes canónicos existentes.
- Control firmado V2: POST {control_url}/api/devices/tars_1/commands/next
  y ACK en el mismo prefijo. Acciones open_maintenance, close_maintenance,
  update_config. Una orden enviada no debe mostrarse como ejecutada sin ACK.
- OTA AMERI1: conservar perfil TARS, board upb-tars-esp32-v1, proyecto TARS,
  chip_id=0 y las comprobaciones de imagen, SHA-256, firma y partición.
  Consultar CONTRATO_OTA_AMERI1.md; no convertirlo en descarga genérica sin validar.
- La telemetría y la vinculación son canales distintos. Vincular no debe
  implicar recepción de telemetría si no existe un destino habilitado.

La UI remota debe presentar configuración deseada, versión efectiva reportada,
orden pendiente/ACK y errores por separado. Usar update_config y
expected_config_version; persistir ajustes en servidor y confirmar su aplicación
con el equipo. Parámetros soportados: WiFi, intervalos de lectura/envío/reintento,
consulta de órdenes, pantalla, brillo, CPU/ahorro, calibraciones, NTP, OTA y hasta
4 destinos HTTPS, cada uno con URL, método, CA y autenticación none/bearer/OAuth.
No borrar ni mostrar secretos guardados al dejar un campo vacío. No renombrar
la identidad ya vinculada. Conservar las reglas de prueba/reversión de red.

Objetivo: una vez vinculada la unidad y confirmados control y telemetría,
configurar y actualizar desde el servidor sin operador junto al prototipo.
Que el dashboard exista no demuestra todavía el funcionamiento de esos canales.

Mantenimiento normal usa la IP de la WiFi actual; si pierde la red, utiliza
AP de respaldo. El botón de 10 segundos fuerza AP local. No interpretar
open_maintenance como AP forzado cuando la WiFi ya está conectada.

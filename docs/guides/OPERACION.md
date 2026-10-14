# Operación y mantenimiento TARS 4.0.4

## Actualizar por OTA local

Activar mantenimiento con el botón. Para acceso directo incluso en una red con aislamiento de clientes, mantener 10 s y usar el AP. Usuario admin y clave de mantenimiento guardada (inicial tars12345678). Entrar a http://192.168.4.1.

En Arduino IDE: Sketch → Export Compiled Binary. Subir solo el archivo TARS.ino.bin de aplicación, nunca merged.bin, bootloader ni particiones. Conservar particiones del equipo. En el panel seleccionar aplicación y esperar confirmación antes de desconectar. El cargador comprueba tamaño, SHA256, chip y proyecto. Tras reiniciar confirmar nueva versión, lecturas, destinos y cierre del AP. La ventana debe tener tiempo suficiente para completar la carga; al caducar se aborta una carga en curso.

Si no tiene firmware con OTA, primera instalación por USB. Una prueba OLED temporal tampoco incluye OTA: restaurar TARS por USB sin borrar toda la flash.

## Configuración

Una misma aplicación puede instalarse en varios TARS. Cada uno guarda configuración propia en NVS: identidad, WiFi, contraseña de mantenimiento, tiempos, calibración, CA, cuatro destinos y vinculación de control. Parámetros secretos vacíos conservan los guardados; usar open_wifi para quitar la contraseña WiFi. Los cambios se validan globalmente antes de guardarse.

Identidad: 1–24 caracteres en minúsculas, números, guion o guion bajo. El ID definitivo no se renombra desde el panel. Clave del panel/AP: 12–63 caracteres. WiFi: SSID hasta 32 bytes; contraseña vacía para red abierta o 8–63 caracteres. No usar contraseñas reales en archivos del repositorio.

Cambios de red/control/CA tienen prueba de 180 s; requieren éxito de los canales configurados. Un destino activado e inaccesible puede causar reversión aunque la asociación WiFi funcione. Para probar solamente WiFi, desactivar destinos y no configurar control. La primera vinculación de identidad y HMAC no se deshace con esa reversión.

## Integración del servidor

Cada destino tiene URL completa HTTPS, método POST/PATCH, none/bearer/oauth_password y headers FIWARE configurables. Orion debe tener la entidad creada en el mismo servicio/path. No asumir que conectar WiFi crea la entidad automáticamente.

Distinguir tres protocolos: telemetría NGSIv2 schema_version=1; control firmado protocol_version=2; OTA AMERI1. Control exige URL base y clave HMAC por equipo, distinta de tokens. Las claves deben generarse/provisionarse desde el servidor. El archivo de identidad vincula ID y HMAC para OTA. No existe alta anónima automática.

El receptor de telemetría deduplica por device_id/boot_id/sequence. HTTP 2xx confirma un destino; el firmware conserva registros hasta ese éxito. Colas de 32 registros/destino; si no hay espacio se pierden nuevas ventanas y se contabilizan. No reenviar atributos NGSI directamente al receptor histórico de pluviómetros.

OTA remota requiere perfil TARS, chip ESP32 clásico y comprobación de bootloader y layout de particiones por AMERI1. Un equipo puede permitir OTA local y rechazar OTA de servidor si su bootloader no es compatible. Los contratos de control y OTA se incluyen junto a esta guía cuando están disponibles; respetar su serialización HMAC exacta.

## Diagnóstico

Monitor serie 115200. WIFI abre mantenimiento normal y permite consultar clave local; para recuperación forzada usar botón 10 s. El primer arranque inicia el panel cerrado; no hace falta puerto serie para configurar usando el botón.

I2C esperado: OLED 0x3C/0x3D, HDC1080 0x40, luz 0x4A. Un ACK identifica presencia, no el controlador. Error I2C=2 es NACK de dirección; revisar alimentación, pinout y continuidad antes de declarar daño. Si temperatura/humedad invalidas, comprobar el HDC1080: fallos=0x3 representa esas dos lecturas. No inventar valores.

La pantalla de información muestra la IP STA al estar conectada. En recuperación forzada siempre puede abrirse http://192.168.4.1 aunque también exista IP STA. AP=ON en serie ahora indica interfaz AP real; mantenimiento activo puede ser STA sin AP.

## Datos sensibles y almacenamiento

La contraseña tars12345678 es un valor inicial público. Cualquier persona que conozca esa clave y pueda alcanzar el panel mientras está abierto puede autenticarse; el acceso temporal limita duración, no convierte una clave común en una identidad individual. Cambiarla en instalaciones que lo necesiten.

Configuración privada permanece en NVS y no está protegida por cifrado de flash en este proyecto. No subir exportaciones privadas, tokens, claves HMAC ni binarios compilados con credenciales reales. Retirar archivos del último commit no los elimina del historial público: revisar y rotar credenciales antiguas si alguna se publicó.

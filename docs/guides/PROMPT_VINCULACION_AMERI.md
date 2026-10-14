# Implementar primera vinculación simple y conservar configuración durante OTA

Trabaja sobre el firmware TARS más reciente que ya controla sensores, AP inicial,
destinos HTTPS, control v2 y OTA AMERI1. No cambies sus contratos criptográficos existentes.
El servidor prepara una extensión de vinculación inicial; todavía debe instalarse y
probarse en la Raspberry. No anuncies un dispositivo físico vinculado sin comprobarlo.

## Flujo de operador

La primera vez, TARS abre su AP y el operador visita `http://192.168.4.1`.
Conserva la configuración manual de WiFi/red abierta, lectura/envío, ID y destinos.
Añade una sección **Vincular con AMERI**, sin importación de archivos:

1. ID definitivo (el mismo creado en Devices).
2. CONTROL_BASE_URL, sin barra final.
3. Código temporal de vinculación generado por el operador autorizado en Devices.
4. Botón Vincular, estado de conexión y resultado real.

El servidor actualmente usa `https://infver.tail8e92a9.ts.net:10000`. No sustituyas
este dominio por `api.moreha.com.co` sin confirmar DNS, proxy y rutas de control/OTA.
La URL de telemetría Orion y la base de control son campos independientes.
Si el ID ya es definitivo, debe coincidir con el servidor; no renombres otro equipo.

## Contrato de primera vinculación, AMERI pairing 1

Solicitud HTTPS con validación CA y reloj listo:

`POST {CONTROL_BASE_URL}/api/pairing/devices/{device_id}/claim`

Content-Type: application/json. Sin Authorization adicional: el código secreto,
limitado a ese dispositivo y a diez minutos, autoriza únicamente esta vinculación.
No existe alta anónima ni generación de claves a partir de una MAC pública.

El cuerpo debe contener exactamente:

```json
{
  "protocol": 1,
  "device_id": "tars_eafit_01",
  "pairing_code": "32 caracteres hexadecimales minúsculos",
  "nonce": "32 caracteres hexadecimales minúsculos",
  "physical_id": "AA:BB:CC:DD:EE:FF",
  "family": "tars",
  "board": "upb-tars-esp32-v1",
  "project": "TARS",
  "chip_id": 0
}
```

Genera el nonce criptográficamente y conserva la petición antes del primer intento.
Ante error de red, reintenta la MISMA petición (incluyendo nonce y MAC); no cambies
nonce por cada reintento. El servidor admite el mismo resultado hasta tres minutos
desde su consumo, sin superar la caducidad del código. Un nonce/MAC distinto con el
mismo código se rechaza. Tras caducar, solicita al operador otro código; el servidor
no regenera la clave HMAC ni desvincula otro equipo. No guardes el código permanentemente
después de confirmar el guardado de las credenciales; nunca lo escribas en logs.

Respuesta HTTP 200:

```json
{
  "protocol": 1,
  "device_id": "tars_eafit_01",
  "nonce": "el nonce exacto de la petición",
  "payload_json": "STRING JSON exacto, NO un objeto reserializado",
  "signature": "64 caracteres hexadecimales minúsculos"
}
```

Firma: HMAC-SHA256, clave = código de vinculación ASCII, mensaje UTF-8:

```text
ameri-pairing-1\n<device_id>\n<nonce>\n<SHA256_hex_del_STRING_payload_json>
```

No incluye salto final. El servidor firma los bytes exactos de `payload_json`.
El paquete del servidor incluye `backend/fixtures/VECTORES_VINCULACION_PUBLICOS.json`:
son vectores ficticios para comprobar la firma, no credenciales de un equipo real.
Verifica firma en tiempo constante, protocolo, ID, nonce y tamaño antes de aplicar
nada. Rechaza campos duplicados, datos truncados y cambios de familia/placa/chip.

El payload contiene:

- device_id, physical_id, family, board, project, chip_id;
- control_url y ota_base_url;
- control_key: clave HMAC ASCII de 64 hex para el control v2 ya existente;
- ap_password: contraseña individual de AP, que sustituye la de fábrica;
- history_url y history_bearer: receptor durable NGSIv2 opcional;
- preserve_local_configuration=true.

No devuelve SSID, contraseña STA, calibraciones ni tiempos para sobrescribirlos.
No mostrar control_key, history_bearer o el payload bruto en HTML, diagnósticos,
pantalla, puerto serie ni logs. El JSON de la respuesta es interno al firmware;
el operador únicamente copia un código temporal, no un archivo de configuración.

## Aplicación y persistencia

Usa una actualización atómica de la configuración existente, con su versión local
real y la política de vinculación de ID/HMAC. Conserva WiFi STA, contraseñas,
certificados, calibraciones, lectura/envío, pantalla, cuatro destinos y sus secretos.
No hagas factory reset y no borres Preferences/NVS ni las colas.

Si ya existe una clave de control, no la reemplaces silenciosamente. Rechaza una
identidad distinta o una MAC distinta. Un equipo ya vinculado se configura usando
update_config; no mediante un nuevo pairing.

Al confirmar el guardado, empieza las consultas de control v2 y OTA AMERI1 usando
la misma clave de control y la derivación OTA exacta existente. El vínculo se considera
confirmado en el servidor tras recibir el reporte HMAC de control, no solo HTTP 200
de pairing. La identidad de MAC/ID reservada no es prueba de contacto firmado.

## Destinos y telemetría

Mantén la URL Orion original, como `.../v2/entities/tars_eafit_01/attrs`, y cualquier
destino secundario. NO reemplaces todos los destinos al vincular.

Ofrece una elección explícita **Añadir histórico AMERI**: elige un slot libre y configura
history_url con POST, mismo cuerpo NGSIv2 y Bearer individual. Si no hay slot libre,
no sobrescribas otro sin consentimiento; indica que la vinculación quedó hecha pero
el histórico requiere elegir destino. Si cambias un slot con pendientes, conserva
la política existente de autorización `reroute_pending`; no borres ni redirijas colas
por defecto. El receptor durable proyecta a Orion: evita envíos duplicados a la misma
entidad cuando el operador elige ese receptor como destino principal.

## Reporte y cambios desde el servidor

Conserva `status_json.config` redacted con SSID, tiempos read_ms/send_ms/retry_ms,
control_poll_ms, CPU/pantalla, calibraciones, ota_enabled, URLs/métodos/auth/slots,
cabeceras y banderas de presencia de secretos. Reporta RSSI, wifi_connected, dirección
STA, estado de colas por destino, HTTP/fallos, config_version y network_probation.
No reportes contraseñas, Bearer, control_key, oauth_password ni client_secret.

Mantén update_config con expected_config_version reportada, ACK y reporte posterior.
WiFi/CA/control erróneos deben revertir con la política de probación de 180 segundos.
Un ACK ejecutado no significa que la probación de red haya finalizado.

## OTA totalmente remota

El operador sube una aplicación .bin y confirma desde Devices. La ESP32 consulta
automáticamente; no necesita GPIO, AP ni presencia física. Mantén AMERI1 intacto:
firma/contador/nonce, descarga HTTPS por ticket, SHA256, placa/chip/proyecto/versión,
bootloader verificado, tamaño de slot, recuperación, colas vacías y AP cerrado.

El servidor puede guardar una solicitud hasta 24 horas mientras no se entregue.
Esto no cambia el contrato firmado AMERI1: al entregar crea una concesión corta
de descarga y vuelve a comprobar los reportes recientes y las condiciones de seguridad.
No reenvía órdenes consumidas ni deduce éxito de descarga/reinicio.

Escribe únicamente la partición de aplicación. Conserva las particiones y namespaces
de configuración/colas/control/OTA existentes. Si cambia el formato de NVS, migra
versionadamente sin descartar campos desconocidos o secretos. No uses prefs.clear(),
nvs_flash_erase() ni defaults incondicionales en el arranque de una OTA.
Tras reiniciar, self-test y boot_valid antes de ACK confirmed con versión/hash correctos.
Un fallo debe permitir rollback sin perder configuración. Si el bootloader actual no
admite recuperación segura, reporta false y no fuerces la instalación.

## Keyrock

Conserva autenticación por destino none/Bearer/Keyrock OAuth password. El operador
remoto puede configurar correo (oauth_user), contraseña (oauth_password), client_id,
client_secret y token_url. Los campos vacíos preservan secretos; auth=none deja de
enviar Authorization. Los tokens de acceso OAuth son privados y su renovación ocurre
en el dispositivo. No confundas estas credenciales con los roles de las PERSONAS
en la web ni con la clave HMAC de control/OTA. La web todavía usa sesión privada,
pendiente de integrar Keyrock como proveedor de identidad de operadores.

## Validación y entrega

Prueba con un solo TARS: primera vinculación, código equivocado/expirado, interrupción
antes/después de guardar NVS, reintento idéntico, MAC duplicada, primer reporte firmado,
datos e histórico, cambios de tiempos y WiFi con reversión, destinos secundarios,
Keyrock opcional y OTA de una versión a otra. Compara configuración privada antes y
después en el equipo SIN divulgar valores; confirma que WiFi, URLs, tiempos y secretos
se conservan. Prueba arranque OTA y rollback. Entrega fuentes, versión y app.bin real,
sin afirmar pruebas físicas que no hiciste.

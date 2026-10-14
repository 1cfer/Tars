> TARS 4.0.4: el panel local permite STA durante mantenimiento y AP de recuperación forzado por botón 10 s. No cambia el formato firmado de control/OTA; esas acciones del botón son locales. Las referencias de interfaz a versiones anteriores deben interpretarse con esa actualización.

# Control TARS: protocolo v2 compatible con AMERI

Este es el protocolo de **control**, independiente del histórico V2 de lluvia. Implementar dispatch por familia sin cambiar los contratos actuales de celdas ni pluviómetros.

## Clave y canónicas

Clave por dispositivo: 64 caracteres hex minúscula, usados como **64 bytes ASCII**, no decodificar a 32 bytes. HMAC-SHA256, resultado hex minúscula. JSON interno como cadena UTF-8 exacta. No reserializar antes de verificar. Separadores LF, sin LF final. Rechazar claves JSON duplicadas, números no finitos, tamaños excesivos y tipos incorrectos.

## Poll

`POST {control_url}/api/devices/{device_id}/commands/next`, JSON, sin Bearer.

Campos: `device_id`, `last_sequence` entero seguro, `maintenance_active` booleano, `remaining_seconds` entero, `nonce` de 48 hex aleatorios, `status_json` **string**, `device_signature` de 64 hex. Incluye `firmware_protocol=2` y `protocol_version=2`.

Canónica:

```text
v2-status
{device_id}
{last_sequence}
{maintenance_active:1|0}
{remaining_seconds}
{nonce}
{sha256(status_json exacto)}
```

Verificar antes de actualizar contacto/estado. Dentro del objeto firmado `status_json`: `device_id`, `physical_id`, `time_utc`, `clock_ready`, `config_version`, configuración reportada **sin secretos**, manifiesto público, sensores/almacenamiento, energía y estado de las cuatro colas. `config` contiene parámetros editables; `destinations` en el nivel raíz contiene estadísticas de entrega. No confundirlos.

El servidor debe guardar nonce/resultado y hacer idempotente una petición repetida; no renovar contacto usando un replay. Validar reloj firmado y tratar recepción, medición y mantenimiento como estados distintos.

Respuesta sin orden: HTTP 200, `{"command":null}`. Con orden: HTTP 200, objeto `command` con `protocol_version=2`, `device_id`, `command_id` ASCII alfanumérico/guion/underscore de 1–64 caracteres, `sequence`, `action`, `expires_at` Unix en segundos, `duration_seconds`, `config_json` **string**, `signature`.

```text
v2
{device_id}
{command_id}
{sequence}
{action}
{expires_at}
{duration_seconds}
{sha256(config_json exacto)}
```

Secuencia estrictamente mayor que la consumida y ≤9007199254740991. Caducidad futura y **no superior a 120 s** desde el reloj de TARS. Entregar una sola orden a la vez; conservar la petición/orden en almacenamiento del servidor.

| Acción TARS | Duración | Configuración |
|---|---:|---|
| `open_maintenance` | 60–900 s | `"{}"` |
| `close_maintenance` | 0 | `"{}"` |
| `update_config` | 0 | JSON exacto, ≤8192 bytes |

TARS **no tiene electroválvula** en este hardware/firmware. No admite `open_valve` ni `close_valve`.

## ACK

`POST {base original}/api/devices/{device_id}/commands/ack`. Campos: `protocol_version=2`, `device_id`, `command_id`, `sequence`, `status` (`executed`/`failed`), `maintenance_active`, `remaining_seconds`, `nonce` de 48 hex, `result_json` **string**, `device_signature`.

```text
v2-ack
{device_id}
{command_id}
{sequence}
{status}
{maintenance_active:1|0}
{remaining_seconds}
{nonce}
{sha256(result_json exacto)}
```

`result_json` contiene `reason` y `config_version`. Verificar antes de registrar ejecución y responder 2xx **tras commit**. El firmware reintenta el ACK persistente. Una interrupción entre reservar la secuencia y completar la acción se reporta como fallo/interrupción; no reejecuta automáticamente la orden. `ap_ssid`/`ap_url` son informativos y no están cubiertos por esta canónica: no usarlos para confirmar ejecución ni tomar decisiones de autorización. Estado real mediante campos firmados y contacto reciente.

Si una orden cambia la URL de control, su ACK vuelve al servidor que la emitió, con la CA anterior guardada para ese ACK. Nunca reenviar a otro receptor el resultado privado por conveniencia.

## Patch de configuración

Siempre `expected_config_version` igual a la reportada. `config_version` lo asigna la ESP32; no lo asigna el navegador.

Campos de identidad/red: `device_id` (solo mientras sea `new`, 1–24 caracteres minúscula/números/guion/underscore), `ssid`, `wifi_password`, `open_wifi`, `control_url`, `control_key` (solo primera vinculación), `ap_password` (12–63 bytes), `ca_pem`, `ntp_server`, `ntp_fallback`.

Tiempos/energía: `read_ms` [1000,3600000], `send_ms` [5000,86400000], `retry_ms` [1000,3600000], `control_poll_ms` [5000,300000], `display_timeout_ms` [0,86400000], `page_ms` 0 o [2000,60000], `cpu_mhz` 80/160/240, `brightness` [1,255], `wifi_sleep`, `ota_enabled`. `read_ms<=send_ms`.

Calibración: `temperature_offset` [−30,30], `humidity_offset` [−50,50], `lux_offset` [−10000,10000], `lux_slope` [0.0001,10000], `noise_slope` [0.0001,1000], `noise_offset` [−200,200], `noise_verified` booleano.

`destinations` es un array de parches con `slot` 0–3, único. Cada uno puede contener `enabled`, `url` HTTPS, `method` POST/PATCH, `auth` none/bearer/oauth_password, `bearer`, `token_url`, `oauth_user`, `oauth_password`, `client_id`, `client_secret`, `fiware_service`, `fiware_servicepath`, `ca_pem`, `reroute_pending` booleano.

Una URL de destino con pendientes solo cambia si `reroute_pending=true` autoriza enviar esos mismos registros al nuevo receptor. Desactivar no borra la cola. Secretos ausentes o vacíos conservan los actuales; `open_wifi=true` elimina la contraseña WiFi. Desactivar autenticación con `auth=none` evita enviar Bearer sin borrar su valor almacenado. Las claves HMAC y el ID definitivos no se cambian remotamente.

Ejemplo de patch:

```json
{
  "expected_config_version": 5,
  "send_ms": 60000,
  "read_ms": 5000,
  "brightness": 32,
  "wifi_sleep": true,
  "destinations": [{"slot": 1, "enabled": false}]
}
```

Cambios de WiFi/CA global/control tienen prueba de 180 s. La versión y el estado reportados pueden volver a los anteriores si la prueba falla. Mostrar «guardado, comprobando red» hasta verificar contacto posterior estable; no afirmar éxito final solo porque llegó `executed`.

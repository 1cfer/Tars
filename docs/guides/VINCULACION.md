# Primera vinculación AMERI pairing 1 · TARS 4.1.0

## Lo que cambia

Panel: ID definitivo, CONTROL_BASE_URL, código temporal y elección Añadir histórico AMERI. Sin archivos de instalación, sin copiar control_key. Un equipo nuevo sin ID/SSID abre AP 10 min; un equipo ya configurado conserva el botón de 4 s/10 s. La clave del AP de fábrica es tars12345678; tras vincular se conserva la individual del servidor.

## Contrato

POST `{base}/api/pairing/devices/{id}/claim`, HTTPS validado, UTC listo, JSON y sin Authorization. Nueve campos exactos: protocol=1, device_id, pairing_code (32 hex minúsculas), nonce (32 hex minúsculas generado con esp_fill_random), physical_id (MAC STA), family=tars, board=upb-tars-esp32-v1, project=TARS, chip_id=0.

El cuerpo completo se persiste antes del primer POST y permanece idéntico al reintentar o reiniciar: misma MAC, nonce y bytes JSON. La sesión conserva metadatos separados de la petición. Si no hay WiFi/UTC, no empieza un nuevo claim. El equipo limita reintentos a 3 min desde el primer intento (conservador respecto a la ventana del receptor) y 10 min desde crear la petición; el servidor impone además la caducidad real del código. Solicitar otro código cuando caduque. El código pendiente es privado en NVS y no aparece en logs o estado público.

Respuesta 200: protocol=1, mismo ID/nonce, payload_json como STRING y signature de 64 hex. HMAC-SHA256 con código **ASCII**, mensaje `ameri-pairing-1\n<ID>\n<nonce>\n<SHA256_hex(payload_json)>`, UTF-8, sin salto final. Verifica los bytes exactos antes de parsear el payload; rechaza duplicados, truncado, tamaño, tipos, ID, MAC, familia, placa, proyecto, chip, preserve_local_configuration falso y claves/URLs inválidas.

El payload puede incluir una base OTA distinta; se conserva sin modificar la criptografía AMERI1. La base control debe coincidir con aquella a la que el operador autorizó el claim. El histórico es opcional, pero URL/Bearer deben ser una pareja válida si se entregan.

## Persistencia y apagados

No cambia el formato de `tars4-config/config` para mantenerlo legible por el firmware anterior durante rollback. Se copia la configuración real y únicamente se cambian ID si era new, control_url, control_key y ap_password; histórico se añade solo con elección explícita y slot vacío de configuración/secretos/cola. No se borran colas, NVS ni contadores.

Namespace adicional `tars-pair-v1`: pending (petición temporal y metadatos) y ota_binding (base OTA + huella de identidad/MAC/control/key). El sidecar se prepara antes del único commit de configuración y solo es efectivo cuando su huella coincide con la configuración confirmada. El journal guarda esa huella: al arrancar reconoce si el commit ya ocurrió y elimina la petición temporal, sin consumir otro código. Antes del commit puede repetir la misma petición dentro de la ventana del receptor. Si falla limpiar NVS, insiste sin volver a vincular ni sustituir claves.

No se introducen defaults incondicionales en el arranque de OTA. La migración antigua de contraseña no se reaplica en 4.1.0. La configuración de WiFi/CA/control normal sigue usando probación de 180 s y expected_config_version. Pairing no sobrescribe una clave existente: un equipo vinculado usa update_config. Un ACK de update_config no certifica que haya concluido la probación.

## Privacidad del panel

El panel gzip obtiene CSRF de /session; todos los recursos están autenticados y sin caché. Los códigos se limpian del formulario tras guardar la petición. /status y status_json.config permanecen redacted. /pairing/access, solo durante una sesión local autenticada, entrega únicamente la nueva clave del AP para que el operador la guarde; no se incluye en diagnósticos ni reportes. No devuelve control_key, history_bearer ni payload_json.

La sesión que inició la vinculación conserva su contraseña de panel hasta cerrarse; el próximo acceso usa la nueva. El AP actual puede seguir usando la contraseña con la que se abrió hasta su cierre; la siguiente apertura usa la individual guardada. Evitar cerrar/reabrir antes de guardar esa clave.

## Interpretación del resultado

Credenciales guardadas: commit local confirmado, todavía sin prueba de contacto en Devices. Reporte HMAC aceptado: el endpoint de control devolvió HTTP 200 y un sobre JSON de comandos reconocible al estado firmado; comprobar además Último contacto en la web. No afirma una prueba física o instalación del servidor por haber obtenido el binario.

El receptor durable usa el mismo cuerpo NGSIv2 que Orion, no registros históricos V2 del pluviómetro. Mantener POST/Bearer individual, deduplicación y política reroute_pending existente. No redirigir pendientes ni desactivar Orion sin elección explícita.

## Validación realizada y pendiente

Realizadas localmente: vectores ficticios del paquete del servidor; guardas extraídas del código con tipos Arduino simulados (firma, JSON exacto, identidad/MAC/perfil, nonce, campos, duplicados, truncado, expiración y reconocimiento de commit); regresión de pantalla/botón; nueve pruebas del receptor suministrado en una base temporal. Esas pruebas no ejecutan NVS, WiFi o flash reales de una ESP32.

Pendiente en un solo TARS y Raspberry: servidor desplegado; código correcto/erróneo/caducado; desconexión y reinicio antes/después del commit; replay idéntico; MAC duplicada; primer contacto HMAC; histórico y destinos secundarios; guardar/recargar tiempos; WiFi incorrecta y reversión; Keyrock opcional; OTA desde una versión a otra y rollback; comparación privada de configuración antes/después sin divulgar secretos.

Primero actualizar un único equipo por OTA local con app.bin, conservar particiones y comprobar versión 4.1.0. No hacer borrado completo. Instalar el receptor en la Raspberry siguiendo su propio paquete, no este firmware. No actualizar un lote hasta terminar estas pruebas.

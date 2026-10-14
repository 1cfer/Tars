> TARS 4.0.4: el panel local permite STA durante mantenimiento y AP de recuperación forzado por botón 10 s. No cambia el formato firmado de control/OTA; esas acciones del botón son locales. Las referencias de interfaz a versiones anteriores deben interpretarse con esa actualización.

# Extension TARS 4.0.0

El protocolo siguiente se conserva. Para esta entrega añadir family=tars, board=upb-tars-esp32-v1, project=TARS y chip_id=0 al catalogo del servidor. Los ejemplos de cell/rainfall son del contrato anterior; no describen la placa TARS.

# Contrato OTA remota AMERI 1

Estado: backend/frontend preparados para instalación en Raspberry; firmware nuevo y prueba física pendientes. Esta extensión NO modifica control v1 de pluviómetros, control v2 de celdas ni histórico V2 de lluvia. No usar `commands/next` para entregar OTA.

## Direcciones y alcance

`OTA_BASE_URL = https://infver.tail8e92a9.ts.net:10000` (sin barra final).

Dispositivos: `celda_1` a `celda_4`, y `pluviometro_1` a `pluviometro_5`. Un dispositivo consulta únicamente su propio ID.

Rutas nuevas:

- `POST /api/ota/devices/{device_id}/poll`: capacidades y consulta.
- `POST /api/ota/devices/{device_id}/ack`: progreso y resultado.
- `GET /api/ota/devices/{device_id}/firmware/{job_id}`: binario privado; URL entregada dentro de respuesta firmada.

El firmware inicia todas las conexiones salientes. No hay puerto entrante en la ESP, ni necesidad de conectarse a su AP. Comprobar HTTPS/DNS real desde UPBWiFi: Tailscale en la Raspberry no garantiza conectividad de la ESP.

Se conserva la telemetría de celdas:
`https://infver.tail8e92a9.ts.net/v2/entities/celda_N/attrs`.

Se conserva HISTORY_URL de lluvia:
`https://infver.tail8e92a9.ts.net:10000/__ameri_rainfall_bfd0febb65a14e85254414b0c543e6e895457574c5215febd8c7671311868767/api/telemetry/rainfall/records`.

## Claves y firma EXACTA

Usar la clave privada de CONTROL que ya tiene cada equipo. NO usar Bearer histórico, contraseña AP ni contraseña del operador.

`control_key` es el texto ASCII de 64 caracteres existente: NO decodificarlo como hexadecimal. Derivar:

```
K_OTA = HMAC-SHA256(key=UTF8(control_key), message=UTF8("AMERI-OTA-1"))
```

K_OTA son los **32 bytes binarios** del resultado. No usar sus 64 caracteres hex como clave de la siguiente firma.

Cada POST y cada respuesta exitosa son un sobre JSON:

```json
{
  "protocol": 1,
  "device_id": "celda_1",
  "counter": 1,
  "timestamp": 1791500000,
  "nonce": "00000000000000000000000000000001",
  "payload_json": "{\"busy\":false}",
  "signature": "64 caracteres hex minúscula"
}
```

El payload del ejemplo es ilustrativo y no es un reporte completo. `payload_json` es una STRING que contiene un objeto JSON válido, UTF-8, máximo 8192 bytes, sin claves duplicadas ni NaN/Infinity. Firmar sus bytes EXACTOS, antes de parsear. No reordenar/reformatear al verificar. No firmar el JSON exterior completo.

Mensaje canónico de firma: seis líneas, separador LF (`0x0A`), sin CR y SIN salto final:

```
ameri-ota-1-{kind}
{device_id}
{counter decimal}
{timestamp decimal}
{nonce}
{SHA256(UTF8(payload_json)) hex minúscula}
```

`kind` = `poll`, `ack` o `response`; la firma de respuesta sirve tanto para poll como para ack.

`signature = hex_lower(HMAC-SHA256(K_OTA, UTF8(canonical)))`.

- `protocol` entero exactamente 1.
- `counter`: entero de 1 a 9007199254740991. Es un contador OTA independiente de las secuencias de control/histórico, global para poll Y ack, monotónico entre reinicios.
- `timestamp`: UTC Unix en segundos, entero; tolerancia servidor ±120 segundos. Sin reloj sincronizado no hacer OTA. Mantener funcionando los otros canales según sus contratos.
- `nonce`: exactamente 32 caracteres hex minúscula, aleatorio criptográfico por solicitud NUEVA.
- Reservar bloques de contadores en NVS antes de usarlos, o persistir cada incremento de forma durable. Al reiniciar saltar a un contador superior al último reservado, nunca volver a cero. No desgastar NVS con escrituras continuas innecesarias.
- Reintento de una solicitud: reutilizar EXACTAMENTE el mismo sobre (counter, timestamp, nonce, payload y firma). El servidor devuelve la respuesta original, sin ejecutar/entregar otra vez. Después de la ventana de 120 s crear un sobre nuevo con contador superior, y reconciliar el estado del job.
- Respuesta: verificar HMAC, ID, protocol, counter y nonce iguales a la petición, y timestamp reciente. No aceptar respuesta asociada a otra petición. HTTP distinto de 200 no es una orden firmada y nunca dispara instalación.
- `Content-Type: application/json`, Content-Length real, sin chunked request. Poll/ack no llevan Cookie ni Bearer histórico. Certificado HTTPS obligatorio, sin `setInsecure()`.

## Poll: reporte completo obligatorio

```json
{
  "ota_supported": true,
  "rollback_supported": true,
  "clock_ready": true,
  "valve_closed": true,
  "busy": false,
  "family": "cell",
  "board": "upb-cell-v1",
  "chip_id": 0,
  "slot_bytes": 1966080,
  "pending_records": 0,
  "version": "2.0.0",
  "project": "CeldaAmbiental"
}
```

El ejemplo de placa/partición NO sustituye la inspección de hardware. El otro chat debe definir IDs estables reales. `family` = `cell` o `rainfall` según dispositivo. `board`, `version`, `project`: 1–64 caracteres ASCII `[A-Za-z0-9_.-]`. `project` y `version` deben corresponder al descriptor ESP-IDF de la app compilada; no usar una versión inventada distinta de `esp_app_desc.version`.

`chip_id`: ID de formato esptool/ESP image, NO enum `esp_chip_model_t`: ESP32=0, ESP32-S2=2, ESP32-C3=5, ESP32-S3=9. Confirmar el objetivo real de compilación; servidor compara con bytes 12–13 del .bin.

`slot_bytes`: bytes reales de la partición OTA inactiva. `pending_records`: registros de sensores pendientes de ACK durable. `valve_closed` es condición de seguridad reportada, NO sensor físico de posición. Pluviómetros sin válvula pueden reportar true. `busy` debe ser true si llueve, hay mantenimiento/OTA activa, no se han vaciado registros o no se puede actualizar sin perder mediciones.

`rollback_supported` SOLO true si bootloader, particiones y aplicación soportan recuperación efectiva. Tener OTA local no demuestra rollback. Revisar primero la tabla de particiones y el bootloader de los equipos instalados. Un .bin de aplicación NO cambia bootloader ni particiones. Puede requerirse primera instalación física por USB; no anunciar que un primer .bin OTA lo habilita si no es cierto.

Consulta recomendada cada 60 segundos con jitter, sin bloquear mediciones ni el control. No depender del intervalo de envío de sensores. Durante actualización, progreso cada 5–10 s y cambios de etapa. Límite 60 solicitudes por minuto por dispositivo.

## Respuesta al poll

`payload_json` normalmente contiene `{"action":"none"}`. Puede incluir `latest_job:{job_id,state}` para reconciliar. **latest_job no es una orden de instalación**.

Si hay orden compatible, su payload firmado contiene:

```json
{
  "action": "install",
  "job_id": "32 caracteres hex",
  "release_id": "32 caracteres hex",
  "family": "cell",
  "board": "upb-cell-v1",
  "chip_id": 0,
  "version": "2.1.0",
  "project": "CeldaAmbiental",
  "size": 1400000,
  "sha256": "64 caracteres hex minúscula",
  "expires_at": 1791500900,
  "url": "https://infver.tail8e92a9.ts.net:10000/api/ota/devices/celda_1/firmware/JOB_ID",
  "download_token": "64 caracteres hex minúscula"
}
```

Servidor exige reporte compatible ≤120 s, rollback real, reloj listo, cola vacía, válvula cerrada, busy false, mismo family/board/chip/project, versión distinta y tamaño que quepa. Una orden pendiente caduca tras 10 minutos. Al entregarse concede 15 minutos para descargar/verificar/reiniciar/confirmar. Se entrega una sola vez; reintento idéntico de poll devuelve el mismo sobre cacheado. No reinstalar un job ya consumido aunque se repita el sobre.

Antes de escribir, la ESP repite TODAS las comprobaciones localmente; persiste job_id, release_id, hash, versión destino y estado en NVS. Marcar consumida de forma durable ANTES de descargar. Si aparece lluvia/actividad o cambia la condición de seguridad, rechazar con ACK `failed`/error_code, sin reiniciar y conservando datos. Rechazar órdenes de apertura/configuración que entren en conflicto con una OTA activa; conservar cierres de emergencia y no reabrir válvula tras reiniciar.

## Descarga y aplicación

- GET a `url`, con `Authorization: Bearer {download_token}`. Es un permiso de descarga por job/equipo, temporal; NO es el Bearer histórico.
- No colocar tokens en querystring/logs/front-end/NVS permanente. No redirecciones; solo HTTPS del host/puerto de OTA_BASE_URL y ruta exacta del propio ID/job. No transferir Authorization a otro origen.
- HTTP 200, Content-Length=`size`, descarga completa; servidor no admite Range (416). Si hay corte, abortar la escritura y reiniciar descarga del MISMO job mientras la concesión siga vigente. No pedir otra instalación automáticamente.
- Verificar tamaño total y SHA256 exacto de TODO el archivo descargado antes de cambiar partición de arranque, además de validación nativa de imagen/chip/revisión. Usar APIs OTA de ESP-IDF/Arduino adecuadas al firmware. Nunca tocar NVS, bootloader, particiones ni registros históricos.
- Instalar únicamente aplicación en partición inactiva. Servidor acepta app `.bin` ESP estándar de hasta 8 MiB; valida segmentos, XOR checksum, digest ESP si está presente, descriptor y versión. Rechaza imágenes combinadas, bootloader y anexos Secure Boot en esta primera implementación. Secure Boot requiere extensión específica; no deshabilitarlo para usar este sistema.
- Mantener medición de vuelcos/caudal durante descarga cuando sea técnicamente seguro. Antes del reinicio cerrar/durabilizar un intervalo real, conservar timestamps/contadores/cola/calibración/credenciales y enviar o almacenar duramente lo pendiente. Si no se puede garantizar captura durante escritura/reinicio, explicitar la ventana de indisponibilidad y no actualizar durante lluvia. No declarar datos completos si hubo un hueco.
- Tras cambiar partición, ACK `rebooting` y reiniciar. La app nueva debe recuperar contexto OTA antes de gestionar otras órdenes, arrancar válvula cerrada, conservar mediciones y validar configuración, conectividad y funcionamiento básico. Solo marcar app válida tras self-test suficiente. Con rollback habilitado, error/reinicio inesperado debe volver a app anterior.
- No esperar al servidor indefinidamente para validar bootloader; diseñar autoprueba local y límites de tiempo compatibles con rollback. Reportar la versión al servidor después. No quemar eFuses ni alterar Secure Boot como parte de esta tarea.

## ACK: payload firmado de progreso/resultado

```json
{"job_id":"32 caracteres hex","state":"downloading","progress":40,"error_code":"none"}
```

Estados aceptados: `accepted`, `downloading`, `verifying`, `rebooting`, `confirmed`, `failed`, `rolled_back`. Progreso entero 0–100 y monotónico; no retroceder etapas para reintentar descarga. `error_code` opcional, texto ASCII `[A-Za-z0-9_.-]` 1–64, valor por defecto `none`; jamás incluir claves, tokens o contraseñas.

`confirmed` requiere obligatoriamente:

```json
{"job_id":"...","state":"confirmed","progress":100,"version":"2.1.0","sha256":"hash del .bin verificado","self_test_ok":true,"boot_valid":true,"error_code":"none"}
```

No enviar confirmed desde el firmware viejo antes de reiniciar. El nuevo arranque debe realmente haber validado la app. Guardar el resultado hasta recibir respuesta firmada de servidor con `acknowledged:true,job_id,state`; ACK HTTP 200 se envía solo tras commit. Si hay corte, reintentar idéntico sobre o emitir nuevo counter con el MISMO resultado. La app vieja debe conservar contexto suficiente para reportar `rolled_back` si vuelve tras fallo.

Servidor no marca ejecutada al entregar ni al descargar. Tras 15 minutos sin resultado: `unknown`, revoca descarga y bloquea nuevas órdenes hasta resultado final firmado. Un resultado final confirmado/fallido/rollback tardío puede reconciliarlo. ACK antiguo/progreso inferior no reemplaza estados actuales, ni otro job. Resultados terminales no se sobreescriben por otro estado.

HTTP 400 mensaje inválido, 403 firma/reloj/permisos inválidos, 409 conflicto/contador antiguo/progreso incorrecto, 429 límite, 503 persistencia no confirmada. Backoff con jitter; nunca resetear el contador para sortear un 409 ni borrar NVS.

## Operador y almacenamiento

Devices → Firmware remoto: sesión privada existente, subir .bin indicando versión/placa, elegir release y confirmar UN dispositivo. Metadata de upload `family,board,version`; no se envían claves al navegador. API operador: `/api/maintenance/ota/{device_id}/status|releases|jobs|cancel`. Cookie HttpOnly existente + Origin permitido en escritura + permiso por dispositivo.

Servidor guarda binarios, hashes, reportes, contador, deduplicación, jobs, operador, tiempos, progreso y auditoría en SQLite durable/archivos privados de Raspberry. No localStorage. Cancelar solo órdenes no entregadas; no cortar a distancia una escritura en marcha. Cuota inicial 256 MiB; no se borran versiones automáticamente.

## Validación obligatoria con hardware

1. Comparar `VECTORES_OTA_AMERI_PUBLICOS.json` en C++ byte por byte antes de probar firmware.
2. Inspeccionar hardware/bootloader/particiones; primera habilitación física según resultado.
3. Habilitar una sola `celda_1`; confirmar reporte OTA y sensores al menos 15 minutos.
4. Subir app de prueba real de esa placa y proyecto; confirmar entrega/descarga/hash, nueva versión/arranque válido. Nada de órdenes de válvula para probar OTA.
5. Probar pérdida de conexión sin perder cola, binario incompatible, hash inválido, job repetido, reinicio de backend y rollback efectivo en un equipo de laboratorio. Nunca simular fallos peligrosos en una válvula en servicio.
6. Verificar que nuevos sensores/histórico siguen deduplicando sample_id, secuencia y timestamps originales. Después celda_3; los demás uno por uno.
7. En pluviómetros hacer prueba en seco, conservar vuelcos y cola, comparar control v1 y receptor histórico V2 antes/después.

Pruebas de servidor con datos ficticios no equivalen a esta validación física.

Referencias: https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/ota.html y https://docs.espressif.com/projects/esptool/en/latest/esp32/advanced-topics/firmware-image-format.html

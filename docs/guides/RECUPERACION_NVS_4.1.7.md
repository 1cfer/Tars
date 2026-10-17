# Recuperación NVS — TARS 4.1.7

## Diagnóstico y límites

El panel muestra el nombre del error nativo de la escritura (por ejemplo ESP_ERR_NVS_NOT_ENOUGH_SPACE).

126 entradas libres no equivalen a 126 bytes ni garantizan espacio para actualizar un blob. Una actualización mantiene el valor anterior hasta completar la nueva escritura; cambiar WiFi también crea un respaldo. La recolección de páginas necesita margen. El contador global no revela por sí solo la distribución por página.

La implementación 4.1.6 conservaba el registro legado `tars4-config/config`, el blob principal y potencialmente respaldo legado/blob. Al fallar la escritura tras crear un respaldo, configTick reintentaba cada 5 segundos. Además, el diario de control retenía URL y CA después de entregar su ACK.

No tenemos un volcado físico de NVS del equipo: la causa exacta de cada página no está confirmada. `/status.persistence` reporta entradas globales y ocupación por namespace: configuración, control, pairing, OTA y outbox, sin valores secretos. No confundir storage_ready (cola SPIFFS) con persistencia de configuración.

## Cambios

- Las escrituras fallidas de configuración, diario de control, pairing y contexto/contador/replay OTA congelan los siguientes intentos de esas operaciones. Las lecturas y el reporte de diagnóstico continúan. No se ejecutan nuevas órdenes de control sin persistencia de su reserva.
- Una recuperación pendiente al arrancar carga el respaldo válido en RAM y requiere intervención explícita antes de volver a escribir.
- Mientras haya fallo, el AP se mantiene incluso con STA conectado; no caduca hasta resolver la pausa. Conserva la contraseña individual existente y la sesión abierta. Después vuelve a mantenimiento temporal.
- La renovación de mantenimiento no rota contraseña de sesión ni CSRF. Solo la página raíz emite el desafío Digest; las peticiones auxiliares rechazadas devuelven 403 y el panel detiene el sondeo. Una clave incorrecta aún requiere introducir la correcta, no se omite autenticación.
- El botón autenticado Recuperar persistencia NVS llama POST /storage/recover con CSRF. Solo borra `config` cuando `cfg_blob` ha sido leído y validado, y `backup` cuando `backup_blob` ha sido leído y validado. Nunca elimina el blob principal ni el respaldo blob durante esa limpieza. Si hay restauración pendiente, escribe/verifica la configuración seleccionada antes de retirar su respaldo.
- No se borran namespaces completos, identidad, claves, secuencias, reservas, pendientes, historial de jobs, lista consumed ni calibraciones. No se modifica el layout de flash.
- Tras entregar un ACK, el diario conserva su secuencia pero deja de copiar URL/CA ya innecesarias. Un ACK pendiente conserva ambos.
- Un error devuelve HTTP 409 y no actualiza versión ni anuncia guardado. La revisión de copias redundantes tampoco anuncia que una nueva configuración esté guardada o que el espacio alcance.

La recuperación no garantiza espacio si no existen copias redundantes. Si vuelve a fallar, detenerse y revisar `/status.persistence`; no repetir guardados, borrar NVS ni aumentar particiones. No se migra a ciegas el contexto OTA ni se elimina una secuencia para liberar espacio.

## Instalación y prueba en un equipo

1. Conservar alimentación estable. Obtener la contraseña individual de mantenimiento ya guardada o con WIFI por serie; no usar otra vinculación.
2. Instalar únicamente una compilación 4.1.7 verificada para ESP32 clásico y el layout actual. Por USB, mantener Erase All Flash Before Sketch Upload en Disabled y la misma partición; por OTA local usar solo app.bin. No usar OTA remota si ota_supported es false.
3. Abrir mantenimiento con el botón 10 s si se necesita AP. Entrar a http://192.168.4.1 con admin y su clave individual. Si hay pausa de persistencia, ese AP permanece disponible.
4. Revisar Diagnóstico completo → persistence y guardar esos datos sin contraseñas. Pulsar Recuperar persistencia NVS una sola vez. Si devuelve error, no continuar con cambios de WiFi.
5. Tras revisión exitosa, guardar un único cambio. Confirmar respuesta de guardado y config_version nueva, recargar y verificar el valor. Para un cambio de red, esperar su prueba de conectividad y la confirmación del servidor o la reversión al respaldo.
6. Reiniciar y comprobar identidad, WiFi, calibración, configuración, control firmado y colas. Comparar secuencias sin reiniciarlas.
7. Solo tras esa prueba actualizar otros equipos. Probar rechazo de contraseña, expiración normal tras recuperación, un fallo de servidor y OTA separadamente.

## Verificación

Las pruebas Node usan funciones de fuente convertidas y mocks: fallos de lectura, commit, borrado y restauración; conservación de la única copia legada; mantenimiento de blobs y diarios; AP persistente y sesión. No son una prueba física de fragmentación, brownout ni corte de corriente. El autotest embebido conserva el vector de derivación HMAC; los vectores completos y el parser estricto se verifican en la suite del repositorio. Las verificaciones reales de firmas, imágenes OTA y certificados permanecen activas.

## Integración del servidor

Mostrar status_json.persistence.paused/error/free_entries/used_entries/total_entries/records. storage_ready describe SPIFFS y no confirma guardado NVS. Mantener visibles los errores del ACK de configuración; aceptar una orden no equivale a ejecutarla. Confirmar cambios mediante ACK executed y la nueva config_version reportada; ante fallo, no mostrar guardado ni repetir automáticamente update_config. La recuperación local requiere sesión autenticada y CSRF; no exponer su endpoint directamente a Internet.

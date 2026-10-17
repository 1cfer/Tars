# TARS 4.1.8 — panel local y PCB 1.5

Corrige un error introducido en 4.1.7: los recursos auxiliares recibían 403 en vez del desafío Digest necesario para autenticar cada ruta. /config no cargaba y los campos WiFi, generados desde esa respuesta, no aparecían. Ahora todas las rutas requieren autenticación y comparten el desafío de su sesión; una petición no invalida las otras. Al cerrar mantenimiento se renueva el desafío. Se conservan contraseña individual, CSRF, restricciones de interfaz y recuperación NVS.

No cambia conexiones, sensores, certificados, credenciales, configuración ni particiones. Los GPIO y control EN siguen siendo los del perfil anterior; no se ha definido un perfil específico para PCB 1.5. Esta placa nueva utiliza WROOM-32E, SDA/SCL previsiblemente 21/22, sin sensores conectados ni EN de luz conectado a un GPIO. Confirmar conexiones reales, incluido si GPIO 5 está libre, antes de conectar periféricos.

Sin sensores, el registro de lectura puede mostrar 0x17: temperatura 0x01, humedad 0x02, luz 0x04 y sonido 0x10. Es ausencia de lecturas válidas, no prueba de fallo de alimentación. Uptime y timestamps ascendentes muestran ejecución durante ese intervalo. Caracteres ilegibles justo al reiniciar no identifican por sí solos la causa; capturar el arranque completo a 115200.

## Instalación en un equipo de banco

1. El panel 4.1.7 puede impedir su propia actualización al fallar autenticación. Para salir de ese caso, cargar 4.1.8 por USB, sin borrar flash ni cambiar particiones.
2. Abrir TARS/TARS.ino. Para WROOM-32E elegir ESP32 Dev Module (ESP32 clásico), core 3.3.12. Conservar la partición y tamaño de flash correspondientes al equipo; Erase All Flash Before Sketch Upload: Disabled. Todos los archivos deben estar juntos.
3. Reiniciar. Cerrar las pestañas antiguas y abrir una ventana privada. Abrir mantenimiento con botón físico si está conectado o WIFI desde serie. La pulsación de 10 s fuerza AP; WIFI abre mantenimiento normal, por STA si ya tiene WiFi.
4. En equipo nuevo sin configurar, el AP TARS-NEW-... aparece automáticamente durante 10 min. Entrar a http://192.168.4.1 con admin y su contraseña actual. De fábrica es tars12345678; después de pairing es la individual guardada. Consultarla por WIFI en serie si hace falta, sin compartirla.
5. Confirmar que Estado del equipo se actualiza y que aparece Identidad y red con Red WiFi y Contraseña WiFi. No es necesario vincular primero para configurar WiFi.
6. Guardar WiFi, comprobar confirmación y conectividad; luego vincular. No pulsar recuperar NVS si no hay error de persistencia. Conservar el diagnóstico si falla.

Las pruebas host incluyen desafíos Digest compartidos, contraseña errónea, desafío antiguo y cierre de sesión. No se ha probado físicamente el navegador del celular ni la PCB 1.5. El esquema de autoverificación OTA exige lecturas válidas: usar USB para el banco sin sensores. Esta corrección no habilita OTA remota en un bootloader no compatible ni modifica el ahorro de energía o la UPS.

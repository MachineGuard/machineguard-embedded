# Perfil electrónico offline preparado

Este perfil no ejecuta red ni incluye secretos; activa el piezo opcional y
telemetría de sensor, botón y recursos. Compilar desde la raíz:

```powershell
./scripts/Build-Firmware.ps1 -OfflineSimulation
```

El resultado queda en `build/simulation-offline/`. Para una herramienta Wokwi
ya licenciada, abre esta carpeta `simulation/`: `wokwi.toml` señala ese binario,
y el circuito es el mismo de F6a con analizador lógico adicional. El build falla
si hay `secrets.h`. No reutilices el perfil para operación ni añadas credenciales.

`offline-scenario.yaml` prepara cambios reales del componente virtual DHT22 y
pulsaciones cortas/largas. Serial permite contrastar adquisición, mute y hold;
el VCD permite observar LEDs/piezo. Las marcas `tick_ms` son tiempo del ESP32
virtual; mídelo aparte del tiempo de pared de la herramienta. `SIM_RESOURCES`
expone heap libre/mínimo y mínimo de pila del loop en bytes. La tarea de IO no
arranca en este perfil: `IO_STACK_NOT_STARTED=1` no es una medición de su pila.

Estado de esta entrega: archivos compilados/preparados, ejecución electrónica
pendiente de licencia. Sin NTP virtual, reset/NVS virtual ni datos físicos
medidos. Para fallos DHT deberá desconectarse su DATA en una copia del circuito;
no se inventa un fallo del componente cambiando solo su temperatura.

Reinicio de simulación y reinicio de CPU pueden conservar memorias diferentes.
Antes de afirmar persistencia, documenta qué conserva la herramienta: NVS,
flash, RTC o ninguna. No equivalen a un apagado físico.

El perfil operativo mantiene su política de contraseña/TLS/red dedicada. No se
habilita Wokwi-GUEST ni gateway público como atajo. El ensayo de backend del
driver PC es distinto de un enlace ESP32 virtual→Edge, que sigue no probado.

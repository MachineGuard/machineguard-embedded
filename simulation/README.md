# Simulación offline (Wokwi)

Este perfil no usa red ni secretos. Activa el piezo opcional y la telemetría del
sensor, el botón y los recursos.

## Compilar

Desde la raíz del repositorio:

```powershell
./scripts/Build-Firmware.ps1 -OfflineSimulation
```

El resultado queda en `build/simulation-offline/`. El build falla si existe
`secrets.h`. No uses este perfil para operar el nodo ni le añadas credenciales.

## Cargar en Wokwi web

1. Abre una plantilla ESP32 sin guardarla.
2. Copia `diagram.json` de esta carpeta al editor.
3. Usa F1 → `Upload Firmware and Start Simulation` para cargar `MachineGuardNode.ino.bin`.
4. No pulses Save ni Share.

Este flujo no necesita cuenta. Ya se probó con el binario compilado en local, con
varios archivos. El botón Restart recompila el sketch del editor. Para repetir el
binario, vuelve a cargarlo con F1.

En la prueba, una ejecución nueva volvió al contador de arranque 1. Por eso no
demuestra que NVS o RTC conserven datos.

Para Wokwi local o en CI hace falta la licencia o el token correspondiente.
`wokwi.toml` indica el binario, y `offline-scenario.yaml` prepara la automatización.
Ese escenario YAML no se ha ejecutado todavía. La comprobación en la web se hizo
con los controles del componente.

## Escenario automatizado

`offline-scenario.yaml` cambia la temperatura y la humedad del DHT22 virtual, y
simula pulsaciones cortas y largas.

- Serial permite contrastar la adquisición, el silencio (mute) y la pulsación larga (hold).
- El archivo VCD muestra los LED y el piezo.
- Las marcas `tick_ms` son tiempo del ESP32 virtual. Mídelas aparte del tiempo
  de pared de la herramienta.
- `SIM_RESOURCES` muestra el heap libre y mínimo, y el mínimo de pila del loop, en bytes.
- La tarea de IO no arranca en este perfil. `IO_STACK_NOT_STARTED=1` no es una
  medida de su pila.

## Qué se probó en la web

- Cambios de temperatura y humedad, pulsación corta con mute, y pulsación larga.
- Fallo del DHT al desconectar DATA en una copia del circuito. Al restaurarlo,
  volvieron las lecturas.
- `SIM_BOOT` muestra el contador y la identidad. Sin NTP virtual, no se acredita
  la sincronización del reloj ni un reset con NVS conservada.

## Persistencia

Reiniciar la simulación y reiniciar la CPU pueden conservar memorias distintas.
Antes de afirmar que algo persiste, documenta qué conserva la herramienta: NVS,
flash, RTC o nada. Un reinicio de simulación no equivale a un apagado físico.

## Límite

El ensayo del backend con el driver de PC no equivale a un enlace ESP32 virtual
con Edge. Ese enlace todavía no está probado.

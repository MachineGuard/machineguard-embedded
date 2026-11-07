# Compilar y probar

## Herramientas

Instala las herramientas en un directorio propio con `Setup-Tools.ps1`:

```powershell
./scripts/Setup-Tools.ps1 -ToolsRoot "$env:LOCALAPPDATA/MachineGuard/embedded-tools"
$env:MACHINEGUARD_EMBEDDED_TOOLS = "$env:LOCALAPPDATA/MachineGuard/embedded-tools"
```

Versiones fijadas (también están en `firmware/MachineGuardNode/sketch.yaml`):

| Componente | Versión |
|---|---|
| Arduino CLI | 1.5.1 |
| Core ESP32 | 3.3.12 |
| FQBN | `esp32:esp32:esp32doit-devkit-v1` |
| ArduinoJson | 6.21.5 |
| DHT sensor library | 1.4.6 |
| Adafruit Unified Sensor | 1.1.15 |

Arduino IDE puede abrir `firmware/MachineGuardNode/MachineGuardNode.ino` con
esas mismas dependencias.

## Pruebas nativas

```powershell
./scripts/Test-Native.ps1
```

Necesitas los C++ Build Tools x64 de Visual Studio. Si están en otra ruta, usa
`-VcVarsPath`. Las pruebas compilan la misma lógica C++ del sketch, no una
versión escrita en otro lenguaje.

## Compilar el firmware

```powershell
./scripts/Build-Firmware.ps1
```

Los resultados quedan en `build/esp32/`.

Perfil para simulación sin red (Wokwi):

```powershell
./scripts/Build-Firmware.ps1 -OfflineSimulation
```

El resultado queda en `build/simulation-offline/`. Falla si existe `secrets.h`.
Ver [simulación offline](../simulation/README.md).

## Driver de PC para ensayos

`scripts/Build-NativeDriver.ps1` compila un driver para Windows x64. Comparte con
el firmware la lógica de `Node`, la serialización y `Ack`.

- Su transporte solo permite HTTP en `127.0.0.1`, para ensayos desechables.
- No es el adaptador Wi-Fi/TLS del ESP32 y no prueba un enlace virtual con Edge.
- Recibe su configuración por stdin y nunca imprime la credencial.
- No es un producto. No lo uses para operar.

## CI en GitHub

El workflow `.github/workflows/ci.yml` hace tres cosas:

- Las pruebas nativas C++ con MSVC x64.
- Compila el firmware con la configuración de ejemplo, sin secretos, y el perfil de simulación.
- Revisa los diagramas de Wokwi con lint sin conexión.

Las herramientas se instalan con las versiones fijadas de arriba.

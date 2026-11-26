# MachineGuard Embedded

Es el firmware de los nodos de sensores de MachineGuard. Cada nodo es un ESP32
DevKit V1 de 30 pines con un sensor DHT22. Mide temperatura y humedad cada 10 s y entrega cada
muestra a Edge usando solo la credencial de su nodo. Edge guarda las lecturas y
las envía a Core.

El nodo ya se probó sobre hardware físico: sensor, pulsador, LEDs y envío de
lecturas a Edge por Wi-Fi, todavía sin Core. Ver [banco físico](docs/banco-fisico.md). Lo que falta probar se describe en
[límites actuales](docs/limites.md).

## Compilar y probar

Estos comandos usan un directorio de herramientas propio, fuera de las instalaciones globales:

```powershell
./scripts/Setup-Tools.ps1 -ToolsRoot "$env:LOCALAPPDATA/MachineGuard/embedded-tools"
$env:MACHINEGUARD_EMBEDDED_TOOLS = "$env:LOCALAPPDATA/MachineGuard/embedded-tools"
./scripts/Test-Native.ps1
./scripts/Build-Firmware.ps1
```

- Las pruebas necesitan los C++ Build Tools x64 de Visual Studio. Si están en
  otra ruta, usa `-VcVarsPath`.
- `Test-Native.ps1` prueba la misma lógica C++ que usa el sketch.
- Los resultados de compilación quedan en `build/esp32/`.

Detalles de versiones y herramientas en [compilar y probar](docs/compilar-y-probar.md).

## Configurar el banco

Sin `secrets.h`, el firmware compila con un ejemplo ficticio y con la red
deshabilitada. Para tu instalación, copia `secrets.example.h` a `secrets.h` en la
misma carpeta y configura el `deviceCode` y la credencial del nodo.

- No uses la clave administrativa ni la credencial del gateway en un nodo.
- No publiques `secrets.h` ni un binario compilado con secretos reales.

Reglas de HTTPS, HTTP y NTP en [configuración del banco](docs/configuracion-banco.md).

## Cómo encaja

```text
ESP32 con DHT22 (ESTE REPOSITORIO)  ->  Edge (gateway local)  ->  Core (API central)  ->  Web
```

El nodo solo usa su credencial individual. Edge guarda la credencial del gateway
para hablar con Core.

## Estructura

```text
firmware/MachineGuardNode/  Sketch del ESP32 (.ino), su código en src/ y la configuración
scripts/                    Herramientas, pruebas y builds (PowerShell)
tests/                      Pruebas nativas en C++
tools/                      Driver de PC para ensayos (no es producto)
simulation/                 Perfil offline para Wokwi
docs/                       Documentación detallada
```

## Más detalle

- [Compilar y probar](docs/compilar-y-probar.md): herramientas, versiones y pruebas nativas.
- [Configuración del banco](docs/configuracion-banco.md): `secrets.h`, HTTPS, HTTP y NTP.
- [Funcionamiento](docs/funcionamiento.md): lecturas, identidad, cola y estados de entrega.
- [Hardware y señales](docs/hardware.md): pines, indicadores y Wokwi.
- [Banco físico](docs/banco-fisico.md): montaje en protoboard y resultados sobre hardware.
- [Límites actuales](docs/limites.md): qué no está probado todavía.
- [Simulación offline](simulation/README.md): perfil para Wokwi sin red.
- [Simulación en Wokwi](docs/simulacion-wokwi.md): firmware real con red virtual, NTP y reinicio.
- [Dependencias y licencias](THIRD_PARTY.md).

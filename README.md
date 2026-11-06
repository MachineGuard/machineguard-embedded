# MachineGuard Embedded

F6b añade un driver de contrato de PC que comparte `Node`, serialización y `Ack`
con el firmware: `scripts/Build-NativeDriver.ps1` lo compila para Windows x64.
Su transporte solo permite HTTP en `127.0.0.1` para ensayos desechables; no es
el adaptador Wi-Fi/TLS del ESP32 ni demuestra enlace virtual con Edge. Recibe
configuración por stdin y nunca imprime la credencial. No usarlo como producto.
El perfil electrónico offline y sus pasos preparados están en `simulation/`.

Firmware ESP32 DevKit V1 de 30 pines con DHT22. Mide cada 10 s y entrega a Edge
usando solo la credencial de su nodo. Edge conserva SQLite y la credencial de
gateway hacia Core. Este incremento prepara la simulación; aún no acredita Wi-Fi
virtual, sensor físico ni montaje.

## Compilar y probar en Windows

Usa un directorio de herramientas propio, fuera de instalaciones globales:

```powershell
./scripts/Setup-Tools.ps1 -ToolsRoot "$env:LOCALAPPDATA/MachineGuard/embedded-tools"
$env:MACHINEGUARD_EMBEDDED_TOOLS = "$env:LOCALAPPDATA/MachineGuard/embedded-tools"
./scripts/Test-Native.ps1
./scripts/Build-Firmware.ps1
```

Las pruebas requieren los C++ Build Tools x64 de Visual Studio; permite otra ruta
con `-VcVarsPath`. Se compila la misma lógica C++ del sketch, no una versión en
otro lenguaje. El build usa Arduino CLI **1.5.1**, core ESP32 **3.3.12**, FQBN
`esp32:esp32:esp32doit-devkit-v1`, ArduinoJson **6.21.5**, DHT sensor library
**1.4.6** y Adafruit Unified Sensor **1.1.15**. Las versiones también están en
`sketch.yaml`. Arduino IDE puede abrir `firmware/MachineGuardNode/MachineGuardNode.ino`
con esas dependencias. Los resultados de build quedan en `build/esp32/`.

## Configurar el banco

Sin `secrets.h`, compila con el ejemplo ficticio, red deshabilitada y vinculación
local visible. Copia `secrets.example.h` a `secrets.h` en la misma carpeta solo
para tu instalación. Configura el `deviceCode` y la credencial individual emitida
por el administrador de Edge. No uses clave administrativa ni credencial gateway.
No publiques `secrets.h` ni un binario compilado con secretos reales.

HTTPS requiere host, certificado CA y hora NTP válida para verificar el certificado.
Nunca se desactiva esa verificación. `MG_NTP_SERVER` queda vacío hasta configurar
un servidor del banco. HTTP exige `MG_EDGE_TLS=false`, `MG_DEDICATED_BANK_HTTP=true`,
IP RFC1918 explícita y Wi-Fi con contraseña: solo para el punto de acceso dedicado.
No hay servicios de provisión, app móvil ni endpoint de pairing implementados.
Después de cambiar la configuración se recompila y carga por el procedimiento
local acordado con el responsable de hardware.

## Funcionamiento

- `Node` posee las muestras y la cola; `Ack` valida identidad, valores y acuse;
  `HttpHeaders` acota el encuadre. El sketch compone DHT/GPIO/reloj y la tarea `EdgeIo`.
- Una transacción DHT produce temperatura y humedad de un mismo frame. El mínimo
  entre lecturas es 2,1 s. Pulsación corta agenda la siguiente lectura disponible
  y silencia la alarma; no reutiliza caché como otra muestra. Cinco segundos
  activan la indicación local de vinculación durante 60 s, sin cambiar incidentes.
- Cada arranque incrementa un contador NVS antes de adquirir. BootId combina MAC,
  contador y bytes RNG. Si NVS falla o el contador/secuencia se agota, la adquisición
  se detiene sin reutilizar identidades. Un NVS borrado pierde su garantía de contador;
  RNG añade separación, pero no debe borrarse como operación normal.
- Cada muestra congela sampleId, valores y captura. Sin hora sincronizada se omite
  `capturedAt`; NTP posterior no cambia la muestra. Reloj repetido o hacia atrás
  también omite la captura nueva y cuenta la anomalía, evitando timestamps duplicados.
- La cola RAM tiene 60 muestras, más una copia de envío. Se entrega de antigua a nueva.
  Al llenarse pierde la más antigua fuera de vuelo; si solo cabe la de vuelo,
  pierde la nueva. Los contadores distinguen pérdida y entrega. Una tarea de red
  con colas de un elemento no bloquea el muestreo ni el botón mientras espera IO.
- Solo `LOCAL_COMMITTED` con identidad/valores y estado coherentes libera la muestra.
  `PENDING` acredita persistencia Edge; `CONFIRMED` acredita además entrega Core.
  DISCARDED, QUARANTINED y DROPPED_OVERFLOW son terminales separados, no éxitos Core.
  Red/timeout/5xx/credencial mantienen muestra y backoff 1–60 s.
- Un 400/409 con JSON de error Edge válido retira la muestra imposible y guarda
  original/motivo en un historial RAM de ocho entradas; no la modifica ni bloquea
  a las siguientes. Se cuenta también la expulsión de evidencia al llenar ese historial.
  Respuestas malformadas o contradictorias nunca confirman.

Los estados y pérdidas se resumen por Serial sin valores secretos. El payload no
persiste en flash: reinicio descarta RAM. Cuando un reinicio de software conserva
un checkpoint RTC completo, se informa cuántas pendientes se descartaron. En corte
eléctrico, watchdog o checkpoint incompleto se declara pérdida **desconocida**;
pueden haberse guardado ya algunas en Edge. No se promete conservación al apagar.

## Pines y circuito preparado

| Elemento | GPIO / conexión |
|---|---|
| DHT22 | 4, VCC 3V3, GND y pull-up 10 kΩ hacia 3V3 |
| LED verde / ámbar / rojo / azul | 25 / 26 / 27 / 33; cada uno con 220 Ω |
| Pulsador | 14, INPUT_PULLUP, otra terminal a GND |
| Piezo pasivo opcional | 32, resistencia serie 1 kΩ, retorno GND; deshabilitado por defecto |

Verde: sensor válido y Edge accesible. Ámbar lento: sin enlace, retry o pérdida.
Rojo fijo: fallo de sensor. Azul rápido: vinculación. Los límites locales de
demostración son configurables en `Config.h` y no sustituyen umbrales de Core.
El buzzer es opcional y solo suena con alarma local; no reconoce alertas.

`diagram.json` y `wokwi.toml` preparan Wokwi con el binario compilado. El diagrama
puede validarse con `wokwi-cli lint --offline --warnings-as-errors diagram.json`.
El DevKit V1 es un modelo no oficial de Wokwi. Lint no ejecuta la simulación ni
valida corrientes. El piezo virtual no autoriza conectar un buzzer activo o inductivo
directamente: su modelo, corriente y etapa de manejo requieren revisión humana.

La simulación web offline puede cargar el binario sin guardar el proyecto; ver
`simulation/README.md`. La red virtual pública no llega a Edge local. El ensayo
nativo en PC es independiente y no acredita comunicación Wi-Fi del ESP32 virtual.

## Límites de esta base

La simulación offline ejecutó DHT, GPIO y botón; Wi-Fi, TLS, NTP y persistencia NVS
entre reinicios de CPU siguen pendientes en un banco adecuado. HTTP limita
cabecera a 2048 B y cuerpo a 1536 B; no acepta chunked ni redirecciones. Conexión
y lectura usan 1,5 s; handshake TLS, 3 s. Los 6 s del intercambio son un presupuesto
cooperativo: DNS y escrituras internas del core pueden superarlo. No es un máximo
global demostrado. La tarea de red no posee ni modifica la cola del nodo.
Las medidas virtuales no acreditan memoria, latencia ni temporización físicas.

# Banco físico

Pruebas del firmware sobre hardware real. El nodo se armó en protoboard.

- **24/11/2026:** prueba **sin red**, sin `secrets.h`. Sensor, pulsador y LEDs.
- **26/11/2026:** enlace por Wi-Fi con Edge, sin Core. Ver [Enlace con Edge](#enlace-con-edge).

## Componentes

| Componente | Detalle |
|---|---|
| Placa | ESP32 DevKit V1 de 30 pines, USB-C, puente USB-serie CP2102 |
| Sensor | DHT22 en módulo, con su resistencia de pull-up incluida |
| Indicadores | LEDs de 5 mm verde, amarillo (ámbar), rojo y azul |
| Resistencias | 220 Ω para verde, amarillo y rojo; 100 Ω para el azul, que con 220 Ω casi no se ve |
| Pulsador | Mini pulsador de 2 pines |
| Montaje | Protoboard de 400 puntos, jumpers macho-macho y macho-hembra, cable USB-A a USB-C |

El buzzer pasivo KY-006 no se conectó. El firmware normal lo deja deshabilitado y
[hardware.md](hardware.md) pide una resistencia en serie de 1 kΩ que no está en el kit.

## Montaje

La ESP32 queda fuera de la protoboard porque es demasiado ancha para la de 400
puntos. Sus pines se llevan a la protoboard con jumpers macho-hembra.

| Pin de la ESP32 | Conexión |
|---|---|
| 3V3 | Riel + |
| GND | Riel − |
| D4 | Señal del DHT22. Sus pines + y − van a los rieles |
| D25 / D26 / D27 / D33 | Pata larga del LED verde / amarillo / rojo / azul. La pata corta va al riel − a través de su resistencia |
| D14 | Una pata del pulsador. La otra va al riel − |

Cada LED ocupa dos filas seguidas del mismo lado de la protoboard: el jumper del GPIO y
la pata larga comparten una fila, y la pata corta y la resistencia comparten la siguiente.

## Carga del firmware

- En Windows, el CP2102 necesitó el *CP210x Universal Windows Driver* de Silicon Labs.
  Windows Update no lo encontró.
- Se compiló y cargó primero el firmware normal sin `secrets.h`. El contador `RAM=`
  subió con cada lectura.
- Para ver los valores, se cargó el perfil de prueba sin red (`Build-Firmware.ps1
  -OfflineSimulation`), que imprime cada lectura y cada pulsación.

## Resultados

| Prueba | Resultado |
|---|---|
| Lectura del DHT22 | Correcta cada 10 s: entre 30,1 y 30,4 °C y entre 68,6 y 72,9 % de humedad, en interior |
| Pulsación corta | Detectada (`SIM_SHORT`) y adelanta la siguiente lectura |
| Pulsación de 5 s | Detectada (`SIM_HOLD`) |
| LED azul (D33) | Parpadeo rápido continuo: sin red, el nodo se mantiene en modo vinculación |
| LED amarillo (D26) | Parpadeo de aproximadamente 1 s: sin enlace con Edge |
| LED rojo (D27) | Fijo al desconectar la señal del DHT22 de D4; se apaga al reconectarla |
| LED verde (D25) | Probado el 26/11 con el enlace con Edge: encendido fijo. Ver abajo |

El registro serie está en [evidencia/registro-serie-banco-2026-11-24.txt](evidencia/registro-serie-banco-2026-11-24.txt).
El identificador de arranque quedó redactado porque incluye la MAC de la placa.

## Enlace con Edge

Prueba del 26/11/2026 con el firmware normal, sin Core.

### Configuración

- **Red:** zona con cobertura móvil de Windows, dedicada al banco, con contraseña
  (WPA2) y en 2,4 GHz. La laptop tiene la IP `192.168.137.1` en esa red.
- **Edge:** corre en la laptop con `EDGE_BIND_HOST=192.168.137.1`, así que solo
  escucha en la red del banco. No tiene `MACHINEGUARD_API_URL`.
- **Nodo:** se registró en Edge como `ESP32-BANCO-01`, con un intervalo de 10 s,
  mediante `POST /api/v1/edge/sensor-nodes`.
- **Credenciales:** la credencial del nodo se copió directamente a `secrets.h`,
  que no se publica. La clave administrativa y la base de Edge quedaron fuera del
  repositorio.
- **Firmware:** se compiló con HTTP de banco dedicado: `MG_EDGE_TLS=false`,
  `MG_DEDICATED_BANK_HTTP=true` y `MG_EDGE_HOST` con la IP privada. NTP queda
  vacío, así que el firmware omite `capturedAt` y Edge asigna la hora.

### Resultados

| Prueba | Resultado |
|---|---|
| Conexión Wi-Fi | El nodo se unió a la red del banco (IP `192.168.137.147`) |
| Estado en Edge | `ESP32-BANCO-01` aparece `ONLINE` |
| Lecturas | Una cada 10 s, unos 29,6 °C y 71–73 % de humedad, con estado `CALIBRATED` y `LOCAL_COMMITTED` |
| Entrega a Core | `PENDING`, como se espera: no hay Core configurado |
| Cola del nodo | `RAM=0`. `LOCAL_PENDING` sube con cada acuse de Edge |
| LED verde (D25) | Encendido fijo. El azul y el amarillo se apagan |

La evidencia está en [evidencia/edge-banco-2026-11-26.txt](evidencia/edge-banco-2026-11-26.txt).
El `bootId` de cada `sampleId` quedó redactado porque incluye la MAC de la placa.

### Problema encontrado: la banda de 5 GHz

Con el firmware cargado, el nodo no se conectaba: `RAM=` subía y `LOCAL_PENDING`
seguía en 0. Un escaneo desde la placa mostró que no veía la red del banco.

La laptop estaba conectada al Wi-Fi de la casa por 5 GHz, y Windows emitía la
zona con cobertura en esa misma banda. La ESP32 solo usa 2,4 GHz. Se resolvió
cambiando la banda preferida del adaptador Wi-Fi a 2,4 GHz y reiniciando la zona.

## Observaciones

- En una prueba se contaron unas 23 pulsaciones cortas en 6 s. Falta confirmar si
  el pulsador rebota y una pulsación se cuenta varias veces.
- La temperatura no se contrastó con un termómetro de referencia.
- El firmware no informa por el puerto serie si el Wi-Fi está conectado. Para
  diagnosticarlo hubo que cargar temporalmente un escáner de redes.

## Pendiente

- Entrega de Edge a Core con la credencial del gateway (`CORE_CONFIRMED`).
- TLS, NTP y persistencia de NVS entre reinicios sobre hardware.

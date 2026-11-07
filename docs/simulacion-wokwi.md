# Simulación del firmware en Wokwi

Esta simulación ejecuta el firmware real en un ESP32 virtual. Sirve para
comprobar arranque, hora por NTP, reinicio con NVS y, cuando esté disponible,
el enlace con un Edge local. No prueba hardware físico.

Usa la red `Wokwi-GUEST`, que no tiene contraseña. Esa configuración solo existe
en el perfil `MG_WOKWI_NETWORK_PROFILE`. La política de producción no cambia: el
perfil operativo sigue exigiendo TLS, o HTTP solo en el punto de acceso dedicado.

## Requisitos

- Herramientas del firmware instaladas con `scripts/Setup-Tools.ps1`.
- `wokwi-cli` v0.29.1, descargado desde el repositorio oficial de Wokwi. Su
  SHA-256 debe ser `45292e3c4f32ba4cfbcc7a457a6df48aad95fdb3269a218cb83423da9dc483b2`.
  Ponlo fuera del repositorio, por ejemplo en `%USERPROFILE%\.wokwi\bin`, o indica
  su ruta con `WOKWI_CLI_PATH`.
- Token de Wokwi en la variable `WOKWI_CLI_TOKEN`. El script no lo imprime ni lo guarda.

## Comandos

```powershell
./scripts/wokwi-sim.ps1 -Action lint          # diagrama, sin conexión
./scripts/wokwi-sim.ps1 -Action boot-ntp      # arranque, NTP y primera muestra
./scripts/wokwi-sim.ps1 -Action reset         # reinicio por software con NVS
```

Los logs del puerto serie quedan en `build/wokwi-sim/logs/`. Ese directorio no
se sube a Git.

## Escenarios

| Acción | Qué comprueba | Señal en el puerto serie |
|---|---|---|
| `boot-ntp` | Arranque del perfil de red, reloj por NTP y lectura del DHT | `SIM_BOOT counter=1`, `SIM_NTP synced`, `SIM_SENSOR_OK` |
| `reset` | Contador en NVS y checkpoint RTC tras un reinicio por software | `SIM_RESTART`, `SIM_BOOT counter=2`, `resetKnown=1` |
| `edge-down` | Reintentos con Edge apagado y entrega al volver | `SIM_EDGE outcome=Retry`, luego `LocalPending` |
| `edge-link` | Muestra enviada a un Edge local y guardada | `SIM_EDGE outcome=LocalPending` y fila en `GET /readings` |

`edge-down` y `edge-link` necesitan el repositorio `machineguard-edge-api` al
lado de este repositorio. Hoy fallan porque la pasarela privada no llega al
simulador (ver la sección siguiente).

## Estado de la verificación

Probado con `wokwi-cli` v0.29.1 y el firmware compilado del perfil de red:

- `boot-ntp`: aprobado. Arranque con `counter=1`, NTP sincronizado y lectura del DHT.
- `reset`: aprobado. Reinicio por software real (`SW_CPU_RESET`) a los 45 s; el
  contador de NVS pasa de 1 a 2 y el segundo arranque reconoce el reinicio con
  `resetPending=5`, es decir, cinco muestras en RAM que se declaran perdidas.
- `edge-link`: FALLA. El firmware solo registra `SIM_EDGE outcome=Retry`, nunca
  `LocalPending`, y el escenario termina con código 1. Edge guardó 0 lecturas.
- `edge-down`: FALLA. Con Edge apagado hay `SIM_EDGE outcome=Retry` (14 líneas) y
  el firmware conserva la muestra en RAM. Al arrancar Edge no llega ningún
  `LocalPending` en 240 s y Edge guardó 0 lecturas.

La causa es que `wokwi-cli` 0.29.1 no soporta la pasarela privada. El binario no
contiene ninguna clave de gateway, y `wokwigw` v2.0.1 (con su checksum verificado)
no registró ningún cliente conectado. No hay mensaje de plan denegado. La
última release de `wokwi-cli` es la misma v0.29.1 y no publica notas sobre la
pasarela.

La ruta Edge se comprobó aparte, con un Edge local y una petición con el formato
del firmware: 201, `status=CALIBRATED`, `delivery=PENDING`,
`persistence=LOCAL_COMMITTED` y una fila guardada. No sustituye la prueba con el
firmware virtual.

Las pruebas nativas pasan (35 casos). Los perfiles de producción y offline siguen
compilando y `wokwi-cli lint` pasa.

## Edge local y pasarela privada

El ESP32 virtual no puede llegar a `127.0.0.1` de tu PC. La forma documentada de
llegar a servicios del equipo es la pasarela privada de Wokwi (`wokwigw`), con
`host.wokwi.internal`. Esa pasarela puede requerir plan de pago. Con
`wokwi-cli` 0.29.1 no hay forma de activarla desde la línea de comandos: ni
`[net] gateway` en `wokwi.toml` ni otra clave tienen efecto. Mientras no haya
una forma soportada, los escenarios `edge-down` y `edge-link` no pasan.

Edge se arranca solo durante el escenario, con una clave administrativa generada
para esa ejecución y una base SQLite dentro de `build/wokwi-sim/`. La credencial
del nodo también es local y de prueba; se graba en `build/` y no se publica.

## Límites

- Las medidas virtuales no sirven para memoria, latencia ni temporización físicas.
- Un reinicio de simulación no equivale a un apagado físico. Por eso el
  escenario `reset` reinicia la CPU en la misma sesión.
- No se prueba el sensor real ni el montaje. Ver [límites actuales](limites.md).
- El job de CI `wokwi-simulation` solo corre si existe el secreto
  `WOKWI_CLI_TOKEN`. Sin ese secreto aparece como omitido.

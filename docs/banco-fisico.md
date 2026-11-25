# Banco físico

Primera prueba del firmware sobre hardware real, el 24/11/2026. El nodo se armó en
protoboard y se probó **sin red**: no hay `secrets.h`, Wi-Fi ni enlace con Edge.

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
| LED verde (D25) | No probado: solo enciende con enlace con Edge |

El registro serie está en [evidencia/registro-serie-banco-2026-11-24.txt](evidencia/registro-serie-banco-2026-11-24.txt).
El identificador de arranque quedó redactado porque incluye la MAC de la placa.

## Observaciones

- En una prueba se contaron unas 23 pulsaciones cortas en 6 s. Falta confirmar si
  el pulsador rebota y una pulsación se cuenta varias veces.
- La temperatura no se contrastó con un termómetro de referencia.

## Pendiente

- Enlace Wi-Fi con Edge usando la credencial del nodo, y el LED verde.
- TLS, NTP y persistencia de NVS entre reinicios sobre hardware.

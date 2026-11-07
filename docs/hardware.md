# Hardware y señales

## Pines y circuito

| Elemento | GPIO / conexión |
|---|---|
| DHT22 | GPIO 4, VCC a 3V3, GND y pull-up de 10 kΩ hacia 3V3 |
| LED verde / ámbar / rojo / azul | GPIO 25 / 26 / 27 / 33, cada uno con resistencia de 220 Ω |
| Pulsador | GPIO 14, INPUT_PULLUP, otra terminal a GND |
| Piezo pasivo (opcional) | GPIO 32, resistencia serie de 1 kΩ, retorno a GND. Deshabilitado por defecto |

## Indicadores

- **Verde**: sensor válido y Edge accesible.
- **Ámbar lento**: sin enlace, reintento o pérdida de datos.
- **Rojo fijo**: fallo del sensor.
- **Azul rápido**: modo de vinculación.

Los límites locales de demostración se configuran en `Config.h`. No reemplazan
los umbrales de Core.

El buzzer es opcional. Solo suena con una alarma local y no reconoce alertas.

## Simulación en Wokwi

`diagram.json` y `wokwi.toml` preparan la simulación en Wokwi con el binario compilado.
Para validar el diagrama sin conexión:

```bash
wokwi-cli lint --offline --warnings-as-errors diagram.json
```

Ten en cuenta estos límites:

- El DevKit V1 es un modelo no oficial de Wokwi.
- El lint no ejecuta la simulación ni valida corrientes.
- El piezo virtual **no autoriza** conectar un buzzer activo o inductivo
  directamente. Su modelo, su corriente y la etapa de manejo deben revisarse a mano.

Para el perfil de simulación offline paso a paso, ver [simulation/README.md](../simulation/README.md).

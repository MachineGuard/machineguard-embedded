# Dependencias y fuentes

No se incluyen copias de bibliotecas; Arduino CLI las obtiene con versiones fijadas.
Conserva sus licencias y avisos al redistribuir código o binarios.

| Dependencia | Versión | Licencia / fuente |
|---|---|---|
| Arduino CLI (herramienta) | 1.5.1 | GPL-3.0, [Arduino](https://github.com/arduino/arduino-cli) |
| Arduino core ESP32 | 3.3.12 | LGPL-2.1, con componentes de ESP-IDF y avisos propios; [Espressif](https://github.com/espressif/arduino-esp32/tree/3.3.12) |
| ArduinoJson | 6.21.5 | MIT, [Benoît Blanchon](https://github.com/bblanchon/ArduinoJson/tree/v6.21.5) |
| DHT sensor library | 1.4.6 | MIT, [Adafruit](https://github.com/adafruit/DHT-sensor-library/tree/1.4.6) |
| Adafruit Unified Sensor | 1.1.15 | Apache-2.0, [Adafruit](https://github.com/adafruit/Adafruit_Sensor/tree/1.1.15) |

Referencias: [sketch Arduino](https://docs.arduino.cc/arduino-cli/sketch-specification/),
[DHT22 y periodo de adquisición](https://www.adafruit.com/product/385),
[Wi-Fi ESP32](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/wifi.html),
[RNG ESP32](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/random.html),
[Wokwi y artefactos compilados](https://docs.wokwi.com/vscode/project-config).

Las APIs y licencias se verifican contra las versiones fijadas; el toolchain y
ESP-IDF incluyen componentes adicionales con sus avisos. Wokwi CLI es una
herramienta de validación aparte; su licencia/servicio no queda concedida por este repositorio.

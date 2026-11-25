# Límites actuales

Esto es lo que **no** está comprobado todavía. No lo tomes como funcionando.

## Hardware y red

- Sobre hardware físico solo se probó el nodo sin red: lectura del DHT22, pulsador
  y LEDs azul, amarillo y rojo. Ver [banco físico](banco-fisico.md).
- El enlace con Edge, el LED verde, Wi-Fi, TLS, NTP y la persistencia de NVS entre
  reinicios de CPU siguen pendientes sobre hardware.
- La red virtual pública de Wokwi no llega a un Edge local.
- El driver de PC demuestra el contrato con el backend, pero no demuestra la
  comunicación Wi-Fi de un ESP32 virtual ni un enlace virtual con Edge.

## Simulación offline

- La simulación ejecutó DHT, GPIO y botón.
- Sin NTP virtual no se comprueba la sincronización de reloj, ni un reset con NVS
  conservada. Ver [simulación offline](../simulation/README.md).

## HTTP

- La cabecera acepta como máximo 2048 bytes y el cuerpo 1536 bytes.
- No acepta respuestas `chunked` ni redirecciones.
- Conexión y lectura usan 1,5 s. El handshake TLS, 3 s.
- Los 6 s del intercambio son un presupuesto cooperativo. El DNS y las escrituras
  internas pueden superarlo. No es un máximo global demostrado.
- La tarea de red no posee ni modifica la cola del nodo.

## Medidas

Las medidas virtuales no sirven para afirmar memoria, latencia ni temporización
físicas.

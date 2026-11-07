# Funcionamiento del firmware

El ESP32 ejecuta varias **tareas** (hilos de ejecución que corren en paralelo).
Las piezas principales son:

- `Node`: guarda las muestras y la cola de envío.
- `Ack`: valida la identidad, los valores y el acuse de Edge.
- `HttpHeaders`: acota el tamaño de las cabeceras HTTP.
- El sketch une el sensor DHT, los GPIO, el reloj y la tarea `EdgeIo`, que habla con Edge.

## Lecturas y botón

- Una transacción del DHT produce temperatura y humedad del mismo frame.
- Deja al menos 2,1 s entre lecturas.
- Una pulsación corta agenda la siguiente lectura disponible y silencia la alarma.
  No reutiliza una caché como si fuera otra muestra.
- Cinco segundos de pulsación activan la indicación de vinculación durante 60 s.
  No cambia incidentes.

## Identidad y arranque

- Cada arranque incrementa un contador en NVS (la memoria no volátil del ESP32)
  antes de adquirir datos.
- El `bootId` combina la MAC, el contador y bytes del generador aleatorio (RNG).
- Si NVS falla, o el contador o la secuencia se agotan, la adquisición se detiene.
  Así no se reutilizan identidades.
- Si se borra NVS, se pierde la garantía del contador. El RNG añade separación,
  pero no debe borrarse como operación normal.

## Muestras y hora

- Cada muestra congela su `sampleId`, sus valores y su captura.
- Sin hora sincronizada se omite `capturedAt`. Un NTP posterior no cambia la muestra.
- Si el reloj se repite o retrocede, también se omite la captura nueva y se cuenta
  la anomalía. Así no hay marcas de tiempo duplicadas.

## Cola y entrega

- La cola en RAM tiene 60 muestras, más una copia que está en envío.
  Se entregan de la más antigua a la más nueva.
- Si la cola se llena, se pierde la más antigua que no esté en envío. Si solo cabe
  la que está en envío, se pierde la nueva.
- Los contadores distinguen lo perdido de lo entregado.
- La tarea de red no bloquea el muestreo ni el botón mientras espera la E/S.

## Estados de respuesta

- Solo `LOCAL_COMMITTED`, con identidad, valores y estado coherentes, libera la muestra.
- `PENDING` acredita que Edge la guardó. `CONFIRMED` acredita además que Core la recibió.
- `DISCARDED`, `QUARANTINED` y `DROPPED_OVERFLOW` son estados finales y separados.
  Ninguno cuenta como éxito de Core.
- Red, timeout, 5xx o credencial mantienen la muestra. Se reintenta con espera de 1 a 60 s.
- Un 400 o 409 con JSON de error válido de Edge retira la muestra imposible. Guarda
  el original y el motivo en un historial en RAM de 8 entradas. No modifica la
  muestra ni bloquea las siguientes. Si ese historial se llena, también se cuenta
  la expulsión de esa evidencia.
- Las respuestas malformadas o contradictorias nunca confirman una muestra.

## Estados y pérdidas por Serial

- El firmware resume estados y pérdidas por Serial (la salida del puerto USB),
  sin valores secretos.
- El payload no se guarda en flash. Un reinicio descarta la RAM.
- Si hay un reinicio por software y un checkpoint RTC completo, se informa cuántas
  pendientes se descartaron.
- Ante un corte de energía, un watchdog o un checkpoint incompleto, la pérdida se
  declara **desconocida**. Algunas muestras pudieron guardarse ya en Edge.
- No se promete conservar datos al apagar el nodo.

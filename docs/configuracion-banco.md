# Configuración del banco

## secrets.h

- Sin `secrets.h`, el firmware compila con el ejemplo ficticio. La red queda
  deshabilitada y la vinculación local se ve.
- Para tu instalación, copia `firmware/MachineGuardNode/secrets.example.h` a
  `secrets.h`, en la misma carpeta.
- Configura el `deviceCode` y la credencial individual del nodo. Esa credencial
  la emite el administrador de Edge.
- No uses la clave administrativa de Edge ni la credencial del gateway.
- No publiques `secrets.h` ni un binario compilado con secretos reales.

Después de cambiar la configuración, recompila y carga el firmware con el
procedimiento local acordado con el responsable de hardware.

## HTTPS

- Necesita el host de Edge, un certificado de la autoridad (CA) y una hora NTP
  válida para verificar el certificado.
- La verificación del certificado **nunca se desactiva**.
- `MG_NTP_SERVER` queda vacío hasta que configures un servidor NTP del banco.

## HTTP (solo punto de acceso dedicado)

HTTP sin TLS solo se permite con todas estas condiciones:

- `MG_EDGE_TLS=false`
- `MG_DEDICATED_BANK_HTTP=true`
- Una IP privada RFC1918, escrita explícitamente
- Wi-Fi con contraseña

Úsalo solo en el punto de acceso dedicado.

## Qué no existe todavía

- No hay servicios de provisión, app móvil ni endpoint de emparejamiento (pairing).
- No se usa Wokwi-GUEST ni un gateway público como atajo para probar. El perfil
  operativo mantiene su política de contraseña, TLS y red dedicada.

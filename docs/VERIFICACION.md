# Verificación v0.2

Fecha: 21 de octubre de 2026. Entorno Linux x86_64, C++17 y Python 3.

Se ejecutó `make -C server -j2 test examples` desde la raíz reorganizada con las fuentes locales de XmlRpc++ y del TP2, sin xmlrpc-c-config ni flags de enlace de xmlrpc-c.

| Grupo | Resultado |
| --- | --- |
| Servidor C++ y bibliotecas estáticas XmlRpc++/TP2 logger | Compilación correcta. |
| Ejemplos originales server, client y clientcmd | Compilación correcta; no se ejecutaron. |
| Dominio/servicio C++ | 11 comprobaciones correctas. |
| Pruebas unitarias Python | 6 pruebas correctas. |
| Pruebas de integración y cliente | 15 pruebas correctas, con subcasos. |

Se probaron ping, eco con UTF-8 y escape XML, contenido vacío y límite de bytes, mensajes inválidos, parámetros inválidos, método/operación desconocidos, introspección, puerto ocupado, escucha exclusiva en 127.0.0.1, CLI real, ausencia de servidor, timeout y correlación de ID. Doce solicitudes de cuatro clientes concurrentes conservaron sus respuestas. El ejecutable C++ cerró con SIGTERM en la limpieza de las pruebas.

Los warnings de parámetros no utilizados provienen de las fuentes originales; no se modificaron para silenciarlos. Se compararon los archivos incorporados con los ZIP proporcionados (ver ORIGENES.md).

Esta verificación histórica cubre comunicación local. En esa versión no se probaron
dos computadoras, Windows, uso efectivo del logger, conexión serie ni ejecución del
simulador. La autenticación por solicitud ya está implementada en US-03 y el control
serie se incorporó después; ver las actualizaciones y ESTADO_PROYECTO.md.

## Actualización: incorporación de la clase Robot (27 de octubre de 2026)

Se conectó `ServicioRobot` al servidor, se agregó `Robot.cpp` a la compilación y se verificó la comunicación XML-RPC en Windows con MinGW-w64:

| Grupo | Resultado |
| --- | --- |
| Compilación nativa del servidor y prueba C++ | Correcta. |
| Dominio/servicio C++ | Comprobaciones correctas. |
| Pruebas unitarias Python | 10 pruebas correctas. |
| Pruebas de integración | 20 pruebas correctas. |

Las pruebas cubren la validación de parámetros, el estado explícito cuando el robot está desconectado, los datos no disponibles, la ayuda sin comandos de movimiento y el rechazo de un puerto serie inexistente. No se verificó la ejecución de movimientos sobre el robot.

Según el registro incorporado desde ConState, se verificó físicamente el 27 de octubre
de 2026 con el Arduino en `COM5`: conectar, consultar `M114` y desconectar respondieron
`OK`. El firmware informó modo absoluto, motores desactivados y ventilador desactivado;
la posición se devolvió como no disponible por no contener `CURRENT POSITION`.
No se enviaron comandos de activación de motores ni de movimiento. No se repitió esa
prueba física en US-23. La autenticación y autorización por rol ya están implementadas
en ServicioProtegido; US-23 agrega la consulta de identidad para el menú.

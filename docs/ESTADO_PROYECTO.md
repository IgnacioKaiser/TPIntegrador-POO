# Estado del proyecto

Fecha de revision: 27/10/2026. Base local revisada: `0add6a504cf021ea46beff06da5ac64e9500aa30`.
Las historias US-10 a US-13 estan en cambios locales sin commit.
Este documento es una fotografia, no una declaracion permanente. Antes de una nueva tarea, verificar el commit actual y actualizar lo que cambio.

## Producto
Aplicacion OO para manipulacion remota de un robot RRR de tres grados de libertad con efector simple. Cliente Python, servidor C++ mediante XML-RPC y controlador con firmware provisto por la catedra. Consigna principal: consignas/TPI_2026.pdf.

## Publicado y verificado
- Base XML-RPC con MensajeRPC de seis campos; ping y eco.
- Compatibilidad de compilacion con Windows/Linux.
- US-04: SQLite para usuarios y roles; credenciales con libsodium. Herramienta usuarios_admin para inicializar, agregar y listar. Pruebas y uso manual verificados por el equipo en Windows.
- US-03: Autenticador y ServicioProtegido validan cada solicitud. Pruebas originales del dominio (15), cliente Python (10), usuarios, autenticacion e integracion (18 en ese hito) correctas en Windows segun salidas del equipo. No asumir que esos conteos permanecen iguales tras nuevos cambios.
- US-05: integrado en master mediante PR #1. Agrega usuario.habilitar y usuario.deshabilitar para administradores. Se revisaron codigo y documentacion; en esta revision del 27/10 no se ejecutaron nuevamente sus pruebas.
- US-06 a US-09: la base local `0add6a5` ya incluye conexion serie, consulta de estado, comandos de motores y ayuda. No se infiere que el commit por si solo certifique pruebas con Arduino.
- US-10 a US-13: modos, origen y pinza verificados con el Arduino simulado en COM5 (CH340); `G28`, `M3`, `M5`, `G90` y `G91` respondieron correctamente. La consulta serie detectó que `M114` no devuelve sus coordenadas, por lo que `G1` aún no se puede validar con `Robot::mover` y no se envió. En Windows/MSYS2 también pasaron las pruebas C++ de dominio, las 17 de cliente, las temporales de usuarios/autenticación y las 24 de integración XML-RPC. La simulación no verifica movimiento físico.

## Arquitectura vigente
- Cliente: cli.py presenta; cliente.py transporta y construye solicitudes; mensaje.py representa datos.
- Servidor: ServidorXmlRpc conecta adaptador, acceso protegido y servicios.
- `Robot` conserva la conexion serie y el modo de trabajo y coordenadas; el servidor valida y traduce las solicitudes a comandos soportados por el firmware.
- Usuario, RepositorioUsuarios, Autenticador y ServicioProtegido son las piezas de identidad y acceso.
- Logger_396/File_396/LogRecord_396 estan incorporados como base, pero el log completo del TPI no esta integrado.
- data/db/usuarios.sqlite es local y no esta en Git. Cada instalacion inicializa su base. No sobrescribirla ni incluir claves reales en documentacion.
- firmware/arduino/robotArm_v0.62sim contiene el codigo provisto. No equivale a tener el puerto serie conectado.

## Trabajo paralelo y pendientes
La solicitud actual incorpora US-10 a US-13 sobre la base local que ya contiene el control serie. No se presume que exista implementacion de ejecucion automatica, trayectorias o log completo.
Pendientes generales: trayectorias G-Code, propiedad y ejecucion automatica; log e informes; CLI administrativa completa; coordinacion de acceso; entrega y diagramas.
US-18 (log CSV) y US-15/US-16 (almacenamiento/listado de trayectorias) son opciones para el siguiente trabajo, no asignaciones confirmadas.

## Entorno y comandos base
Windows del usuario: PowerShell, g++ MSYS2 UCRT64 en C:/msys64/ucrt64/bin, mingw32-make y Python 3.14. Dependencias: SQLite y libsodium UCRT64.
Desde la raiz: mingw32-make -C server y mingw32-make -C server test. Linux: make y python3, con dependencias de desarrollo instaladas.
Preparacion local: server/build/usuarios_admin.exe inicializar (solo base vacia). Servidor: server/build/robot_server.exe. Cliente: python client/main.py. Ver README para sintaxis de PowerShell.
La escucha sigue en 127.0.0.1; no se ha verificado aqui comunicacion entre dos computadoras ni con Arduino.

## Al cambiar de chat o herramienta
Indicar historia y objetivo, rama/commit de partida, trabajo paralelo confirmado y resultado esperado. Leer documentos y fuentes; no copiar toda la conversacion ni asumir acceso a adjuntos de otro chat. Registrar al terminar archivos modificados, pruebas, limites y siguiente paso.

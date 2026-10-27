# Estado del proyecto

Fecha de revision: 27/10/2026. Base revisada en GitHub: cd3d4bc (master).
Este documento es una fotografia, no una declaracion permanente. Antes de una nueva tarea, verificar el commit actual y actualizar lo que cambio.

## Producto
Aplicacion OO para manipulacion remota de un robot RRR de tres grados de libertad con efector simple. Cliente Python, servidor C++ mediante XML-RPC y controlador con firmware provisto por la catedra. Consigna principal: consignas/TPI_2026.pdf.

## Publicado y verificado
- Base XML-RPC con MensajeRPC de seis campos; ping y eco.
- Compatibilidad de compilacion con Windows/Linux.
- US-04: SQLite para usuarios y roles; credenciales con libsodium. Herramienta usuarios_admin para inicializar, agregar y listar. Pruebas y uso manual verificados por el equipo en Windows.
- US-03: Autenticador y ServicioProtegido validan cada solicitud. Pruebas originales del dominio (15), cliente Python (10), usuarios, autenticacion e integracion (18 en ese hito) correctas en Windows segun salidas del equipo. No asumir que esos conteos permanecen iguales tras nuevos cambios.
- US-05: integrado en master mediante PR #1. Agrega usuario.habilitar y usuario.deshabilitar para administradores. Se revisaron codigo y documentacion; en esta revision del 27/10 no se ejecutaron nuevamente sus pruebas.

## Arquitectura vigente
- Cliente: cli.py presenta; cliente.py transporta y construye solicitudes; mensaje.py representa datos.
- Servidor: ServidorXmlRpc conecta adaptador, acceso protegido y servicios.
- Usuario, RepositorioUsuarios, Autenticador y ServicioProtegido son las piezas de identidad y acceso.
- Logger_396/File_396/LogRecord_396 estan incorporados como base, pero el log completo del TPI no esta integrado.
- data/db/usuarios.sqlite es local y no esta en Git. Cada instalacion inicializa su base. No sobrescribirla ni incluir claves reales en documentacion.
- firmware/arduino/robotArm_v0.62sim contiene el codigo provisto. No equivale a tener el puerto serie conectado.

## Trabajo paralelo y pendientes
El usuario informa que un companero trabaja en tareas del robot, posiblemente US-07 a US-13; el alcance exacto no esta confirmado. No tomar los responsables/estados del CSV Jira como confirmacion. Consultar el reparto antes de editar la conexion, el modelo del robot, movimientos y modos.
En master revisado no se encontro aun implementacion de control serie del robot. Puede existir trabajo local no publicado.
Pendientes generales: conexion/estado/control del robot; tareas G-Code, propiedad y ejecucion; log e informes; CLI administrativa completa; coordinacion de acceso; entrega y diagramas.
US-18 (log CSV) y US-15/US-16 (almacenamiento/listado de trayectorias) son opciones para el siguiente trabajo, no asignaciones confirmadas.

## Entorno y comandos base
Windows del usuario: PowerShell, g++ MSYS2 UCRT64 en C:/msys64/ucrt64/bin, mingw32-make y Python 3.14. Dependencias: SQLite y libsodium UCRT64.
Desde la raiz: mingw32-make -C server y mingw32-make -C server test. Linux: make y python3, con dependencias de desarrollo instaladas.
Preparacion local: server/build/usuarios_admin.exe inicializar (solo base vacia). Servidor: server/build/robot_server.exe. Cliente: python client/main.py. Ver README para sintaxis de PowerShell.
La escucha sigue en 127.0.0.1; no se ha verificado aqui comunicacion entre dos computadoras ni con Arduino.

## Al cambiar de chat o herramienta
Indicar historia y objetivo, rama/commit de partida, trabajo paralelo confirmado y resultado esperado. Leer documentos y fuentes; no copiar toda la conversacion ni asumir acceso a adjuntos de otro chat. Registrar al terminar archivos modificados, pruebas, limites y siguiente paso.

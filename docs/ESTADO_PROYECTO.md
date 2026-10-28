# Estado del proyecto

Fecha de revision: 27/10/2026. Rama de trabajo: `us-15-16-24-tareas`, base `08b0f4e`.
Hito A guardado en ese commit; Hito B implementado localmente, sin commit/push/merge.
Integra el control manual de US-10 a US-13 con identidad, autorizacion y menu
por rol de US-23, además de las operaciones de robot de US-06 a US-09.
Este documento es una fotografia, no una declaracion permanente. Antes de una nueva tarea, verificar el commit actual y actualizar lo que cambio.

## Producto
Aplicacion OO para manipulacion remota de un robot RRR de tres grados de libertad con efector simple. Cliente Python, servidor C++ mediante XML-RPC y controlador con firmware provisto por la catedra. Consigna principal: consignas/TPI_2026.pdf.

## Publicado y verificado
- Base XML-RPC con MensajeRPC de seis campos; ping y eco.
- Compatibilidad de compilacion con Windows/Linux.
- US-04: SQLite para usuarios y roles; credenciales con libsodium. Herramienta usuarios_admin para inicializar, agregar y listar. Pruebas y uso manual verificados por el equipo en Windows.
- US-03: Autenticador y ServicioProtegido validan cada solicitud. Pruebas originales del dominio (15), cliente Python (10), usuarios, autenticacion e integracion (18 en ese hito) correctas en Windows segun salidas del equipo. No asumir que esos conteos permanecen iguales tras nuevos cambios.
- US-06 a US-09: la base local `0add6a5` ya incluye conexion serie, consulta de estado, comandos de motores y ayuda. No se infiere que el commit por si solo certifique pruebas con Arduino.
- US-10 a US-13: modos, origen y pinza verificados con el Arduino simulado en COM5 (CH340); `G28`, `M3`, `M5`, `G90` y `G91` respondieron correctamente. La consulta serie detectó que `M114` no devuelve sus coordenadas, por lo que `G1` aún no se puede validar con `Robot::mover` y no se envió. En Windows/MSYS2 también pasaron las pruebas C++ de dominio, las 17 de cliente, las temporales de usuarios/autenticación y las 24 de integración XML-RPC. La simulación no verifica movimiento físico.
- US-05: integrado en master mediante PR #1. Agrega usuario.habilitar y usuario.deshabilitar para administradores. Sus pruebas de cliente, usuarios y autenticacion se ejecutaron nuevamente con la suite completa de esta etapa.
- US-15/US-16, Hito A implementado en esta rama: Tarea y RepositorioTareas guardan archivos
  normalizados por ID de propietario y listan lo propio para OPERADOR o todo para
  ADMINISTRADOR. Hito A no agrego tabla de tareas ni RPC y se conserva en 08b0f4e.
  Pruebas nativas Windows/MSYS2 UCRT64/g++ 15.2.0/Python 3.14.0: 59 comprobaciones
  de tareas correctas en directorios temporales. Dos casos de enlaces simbolicos
  omitidos por permisos/plataforma. Linux no ejecutado.
  La suite existente tambien paso: dominio, usuarios, autenticacion, 33 pruebas del
  cliente y 27 de integracion con servidor C++ real y bases temporales. No es una
  integracion RPC de tareas; esos conteos corresponden al Hito A.
- US-15/US-16/US-24, Hito B local: tareas con carpeta configurable --tareas-dir,
  tarea.subir y tarea.listar autenticadas, propietario exclusivamente del Usuario
  validado, comandos subir/tareas y opciones 17/18 del menu para ambos perfiles.
  Reutiliza el repositorio y sus reglas sin cambios; no invoca Robot ni ejecuta ordenes.
  Suite completa correcta en Windows nativo/MSYS2 UCRT64/g++ 15.2.0/Python 3.14.0:
  dominio, usuarios, autenticacion, 59 comprobaciones de tareas, 43 pruebas del cliente
  simuladas y 32 de integracion real con C++. Bases y carpetas temporales explicitas
  en todas las pruebas del servidor. Se probaron subida desde CLI, reinicio/persistencia,
  propiedad, rechazos y cuatro subidas concurrentes: un OK y tres duplicados sin
  sobrescritura. Dos pruebas de enlaces siguen omitidas; Linux no ejecutado.
  Ver backlog/US-15.md, backlog/US-16.md y backlog/US-24.md para alcance y limites.
- US-23 (avance integrado en la rama de trabajo): cliente con conectar (puerto opcional),
  desconectar, estado, activar_motores, desactivar_motores y ayuda, por terminal y menu.
  Muestra datos anidados, disponibilidad y errores parciales. Agrega usuario.actual
  en ServicioProtegido y consulta el rol autenticado antes de abrir el menu; las
  opciones administrativas solo aparecen a ADMINISTRADOR. Cada pedido sigue protegido.
  Compilacion y pruebas por grupo correctas en Windows/MSYS2 UCRT64/Python 3.14.0:
  dominio, usuarios, autenticacion, 43 pruebas del cliente simuladas y 32 de integracion
  con el ejecutable C++ real y bases temporales. El usuario verifico manualmente
  contra el servidor real ayuda, ping, estado desconectado y rechazo de ambos comandos
  de motores sin conexion; el menu continua correctamente. Sin verificacion de Arduino.
  Tambien verifico conexion fallida como administrador y rechazo de conectar como
  operador. Desconexion, permisos, identidad, cuenta deshabilitada y clave incorrecta
  estan cubiertos por las pruebas automaticas reales.
  La simulacion no verifica movimiento fisico ni se ha probado el cliente contra Arduino.
  La US-23 completa sigue abierta. Ver backlog/US-23.md.

## Arquitectura vigente
- Cliente: cli.py presenta; cliente.py transporta y construye solicitudes; mensaje.py representa datos.
- Servidor: ServidorXmlRpc conecta adaptador, acceso protegido y servicios.
- `Robot` conserva la conexion serie y el modo de trabajo y coordenadas; el servidor valida y traduce las solicitudes a comandos soportados por el firmware.
- Usuario, RepositorioUsuarios, Autenticador y ServicioProtegido son las piezas de identidad y acceso.
- Tarea y RepositorioTareas son el almacenamiento por propietario, conectados a
  ServicioProtegido. El listado no lee contenidos ni devuelve rutas internas.
- Logger_396/File_396/LogRecord_396 estan incorporados como base, pero el log completo del TPI no esta integrado.
- data/db/usuarios.sqlite es local y no esta en Git. Cada instalacion inicializa su base. No sobrescribirla ni incluir claves reales en documentacion.
- Conservar y respaldar juntos la base de usuarios y las carpetas de tareas por ID.
  Reemplazar o reinicializar la base puede reutilizar IDs y alterar sus propietarios.
- firmware/arduino/robotArm_v0.62sim contiene el codigo provisto. No equivale a tener el puerto serie conectado.

## Trabajo paralelo y pendientes
La implementacion actual incorpora US-10 a US-13 y US-23. No se presume que exista
ejecucion automatica de trayectorias ni log completo. No tomar responsables/estados
de Jira como evidencia ni asumir que lo publicado representa todo el trabajo local
de otro integrante.
Pendientes generales: ensenanza de trayectorias y ejecucion automatica; log e
informes; CLI administrativa completa; coordinacion de acceso; entrega y diagramas.
El usuario asigno US-15/US-16/US-24 como bloque. Hitos A y B implementados y
verificados en Windows en esta rama; no integrados a master. US-18 no esta asignada aqui.

## Entorno y comandos base
Windows del usuario: PowerShell, g++ MSYS2 UCRT64 en C:/msys64/ucrt64/bin, mingw32-make y Python 3.14. Dependencias: SQLite y libsodium UCRT64.
Desde la raiz: mingw32-make -C server y mingw32-make -C server test. Linux: make y python3, con dependencias de desarrollo instaladas.
Solo almacenamiento de tareas: mingw32-make -C server test-tareas (Linux: make).
Durante la regresion inicial habia objetos .o anteriores a la cabecera actual de
Robot: aparecieron estado incorrecto y timeouts. Se forzo la reconstruccion del
servidor y las pruebas C++ correspondientes; despues paso la suite sin cambiar
fuentes del robot. Ante artefactos viejos, recompilar antes de interpretar esos fallos.
Preparacion local: server/build/usuarios_admin.exe inicializar (solo base vacia). Servidor: server/build/robot_server.exe. Cliente: python client/main.py. Ver README para sintaxis de PowerShell.
La escucha sigue en 127.0.0.1; no se ha verificado aqui comunicacion entre dos computadoras ni con Arduino.

## Al cambiar de chat o herramienta
Indicar historia y objetivo, rama/commit de partida, trabajo paralelo confirmado y resultado esperado. Leer documentos y fuentes; no copiar toda la conversacion ni asumir acceso a adjuntos de otro chat. Registrar al terminar archivos modificados, pruebas, limites y siguiente paso.

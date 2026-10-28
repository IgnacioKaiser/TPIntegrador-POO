# Estado del proyecto

Fecha de revision: 28/10/2026. Rama local: `revision-logs-informes`, HEAD `bbb845f`,
igual a la referencia local `origin/us-18-20-21-logs-informes` al iniciar la revisión.
El árbol de trabajo estaba limpio. `master` local apunta a `e44e6da`, que integra
US-15/US-16/US-24; los commits de logs e informes aún no están integrados allí.
No se hizo fetch, commit, push ni merge en esta revisión documental.

Este documento es una fotografia, no una declaracion permanente. Antes de una nueva tarea, verificar el commit actual y actualizar lo que cambio.

## Producto
Aplicacion OO para manipulacion remota de un robot RRR de tres grados de libertad con efector simple. Cliente Python, servidor C++ mediante XML-RPC y controlador con firmware provisto por la catedra. Consigna principal: consignas/TPI_2026.pdf.

## Antecedentes de implementación y verificaciones registradas

Los resultados siguientes son registros de etapas anteriores, no pruebas repetidas
en esta revisión. Las menciones a Hito B local describen su etapa original;
US-15/US-16/US-24 ya figuran integradas en `master` local mediante `e44e6da`.

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
- Logger_396/File_396/LogRecord_396 se conservan como biblioteca. El registro actual
  lo implementa RegistroActividadCsv, independiente de XML-RPC, SQLite y Robot,
  conectado a ServicioProtegido y al ciclo de inicio/cierre del servidor.
- data/db/usuarios.sqlite es local y no esta en Git. Cada instalacion inicializa su base. No sobrescribirla ni incluir claves reales en documentacion.
- Conservar y respaldar juntos la base de usuarios y las carpetas de tareas por ID.
  Reemplazar o reinicializar la base puede reutilizar IDs y alterar sus propietarios.
- firmware/arduino/robotArm_v0.62sim contiene el codigo provisto. No equivale a tener el puerto serie conectado.

## US-18/US-20/US-21 en la rama de revisión

Implementado por los commits `b155927` y `bbb845f`: CSV acumulativo con fechas UTC,
peticiones aceptadas y rechazadas, eventos de inicio/cierre normal y resultados de
conectar/desconectar. La identidad solo se guarda tras autenticar una cuenta
habilitada; no se guardan credenciales ni contenidos. `log.consultar` filtra y
pagina; `informe.usuarios` resume peticiones por usuario. Ambos exigen administrador
en el servidor; el cliente ofrece comandos y opciones 19/20 del menú. El contrato
exacto y los límites se mantienen en CONTRATO.md y [US-18/US-20/US-21](backlog/US-18-20-21.md).

Verificaciones realizadas el 28/10/2026 en Windows nativo, desde PowerShell,
con Python 3.14.0 y herramientas MSYS2 UCRT64 (g++ disponible: 15.2.0):

- Revisión de código de almacenamiento, acceso protegido, servidor, cliente y
  pruebas existentes, contrastada con el texto extraído del PDF oficial y sus
  anexos de texto, más Lineamientos_TP.pdf. No se revisaron imágenes geométricas,
  ajenas a este cambio documental.
- `mingw32-make -C server test-registro`: terminó correctamente usando el ejecutable
  existente, sin recompilar. Comprueba CSV en directorio temporal, reapertura,
  filtros por usuario/tipo/código, una página, resumen y rechazo de una fecha inválida.
- Con `PYTHONPATH=client`, `python -m unittest discover -s client/tests -p test_informes.py -v`:
  3 pruebas correctas con transporte simulado; comprueban filtros, paginación y
  omisión de filtros vacíos. No prueban el servidor real.

No se ejecutó la suite completa, una compilación limpia, Linux, Arduino ni las
consultas nuevas contra un servidor real en esta revisión. Las pruebas existentes
revisadas no verifican extremo a extremo los permisos ni los resultados de estas
dos consultas. Los datos usados por la prueba CSV son temporales; no se tocaron
usuarios, trayectorias ni CSV reales.

Pendientes antes de declarar completas estas historias: comprobar criterios de
aceptación y recorrido real Python → C++ → CSV para ambas consultas, permisos,
fechas y límites; verificar ausencia de secretos en los registros y eventos de
inicio/cierre normal. La lectura de código por sí sola no certifica esos resultados.
La consigna (página 3) pide además estado, inicio de actividad y detalle de órdenes
con errores desde la última conexión; el resumen actual no devuelve esos datos.
Revisar ese alcance de US-20 junto al reporte de US-19, que sigue pendiente.
El parser no registra solicitudes que fallan antes de construir MensajeRPC;
no hay auditoría completa ante fallo de escritura ni garantía de cierre ante
terminación forzada. No hay rotación, índice ni soporte de varios procesos
escribiendo el mismo CSV. `nodo_cliente` es fijo para la escucha loopback actual.

## Trabajo paralelo y pendientes generales

Esta tarea solo actualiza documentación de US-18/US-20/US-21. No modifica interfaces
compartidas con el robot ni presupone el trabajo local de otros integrantes.
Pendientes generales: enseñanza de trayectorias y ejecución automática; reporte
US-19 y alcance restante de informes; CLI administrativa completa; coordinación
de acceso; entrega y diagramas. La US-23 completa sigue abierta.
La integración de logs e informes a master sigue pendiente de revisión del equipo.

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

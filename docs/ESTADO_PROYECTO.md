# Estado del proyecto

Fecha de revision: 28/10/2026. Rama local: `us-17-ejecucion-tareas`, creada desde
`origin/master` actualizado por fetch, base `d86c176`. El árbol estaba limpio;
`master` local se conservó en `c72611a`. US-19 ya está integrada en esa base.
US-17 se limita al Hito 1 local: lectura, formato ejecutable y una orden automática.
No se hacen commits, push ni merges; no se modifica firmware ni se abre Arduino.

Antecedente de la última revisión de control manual (se conserva su evidencia):
Fecha de revision: 28/10/2026. Rama de trabajo: `ControlManual`.
Integra el control manual de US-10 a US-13 con identidad, autorizacion y menu
por rol de US-23, además de las operaciones de robot de US-06 a US-09.
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
- US-10 a US-13: modos, origen y pinza verificados con el Arduino simulado en COM5 (CH340). El 28/10, `M114` informó X=0, Y=170, Z=120 mm; desde el cliente Python y el servidor C++ se envió un `G1` de 1 mm en X a 5 mm/s, se confirmó el destino con `M114` y se retornó a la posición inicial. Pasaron en Windows/MSYS2 UCRT64 la compilación, dominio C++, 33 pruebas de cliente, pruebas temporales de usuarios y autenticación y 27 pruebas de integración XML-RPC. El firmware tiene `SIMULATION=true`: esto verifica la comunicación y el estado simulado, no movimiento físico.
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
  de motores sin conexion; el menu continua correctamente. Esa etapa inicial no
  verifico el cliente con Arduino; la reverificacion del 28/10 cubrio `estado` y
  `mover` contra el simulador mediante el servicio del cliente.
  Tambien verifico conexion fallida como administrador y rechazo de conectar como
  operador. Desconexion, permisos, identidad, cuenta deshabilitada y clave incorrecta
  estan cubiertos por las pruebas automaticas reales.
  La simulacion no verifica movimiento fisico.
  La US-23 completa sigue abierta. Ver backlog/US-23.md.

## Arquitectura vigente
- Cliente: cli.py presenta; cliente.py transporta y construye solicitudes; mensaje.py representa datos.
- Servidor: ServidorXmlRpc conecta adaptador, acceso protegido y servicios.
- `Robot` conserva la conexion serie y el modo de trabajo y coordenadas; el servidor valida y traduce las solicitudes a comandos soportados por el firmware.
- US-17 Hito 1: Robot delega el recurso serie a CanalSerie/PuertoSerie y comparte
  validaciones entre movimiento manual y una orden automática local. ProgramaTarea
  interpreta el subconjunto ejecutable; RepositorioTareas agrega lectura autorizada.
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

## US-18/US-20/US-21: integración y revisión documental anterior

Integrado en master hasta `1583e31` (código `b155927` y `bbb845f`, documentación
`1583e31`): CSV acumulativo con fechas UTC,
peticiones aceptadas y rechazadas, eventos de inicio/cierre normal y resultados de
conectar/desconectar. La identidad solo se guarda tras autenticar una cuenta
habilitada; no se guardan credenciales ni contenidos. `log.consultar` filtra y
pagina; `informe.usuarios` resume peticiones por usuario. Ambos exigen administrador
en el servidor; el cliente ofrece comandos y opciones 19/20 del menú. El contrato
exacto y los límites se mantienen en CONTRATO.md y [US-18/US-20/US-21](backlog/US-18-20-21.md).

Verificaciones de la revisión documental anterior del 28/10/2026, desde PowerShell,
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

Aquella revisión no ejecutó suite completa ni consultas contra servidor real. La
etapa US-19 descrita abajo agrega regresión completa y comprobaciones reales de
permisos y conservación del informe global; no certifica todos los criterios
de US-18/US-20/US-21. Los datos de pruebas son temporales.

Pendientes antes de declarar completas estas historias: comprobar criterios de
aceptación y recorrido real Python → C++ → CSV para ambas consultas, permisos,
fechas y límites; verificar ausencia de secretos en los registros y eventos de
inicio/cierre normal. La lectura de código por sí sola no certifica esos resultados.
La consigna (página 3) pide además estado, inicio de actividad y detalle de órdenes
con errores desde la última conexión; el resumen actual no devuelve esos datos.
US-19 agrega ese reporte para la identidad propia. Completar la misma información
en el informe global de US-20 sigue pendiente y queda fuera de esta etapa.
El parser no registra solicitudes que fallan antes de construir MensajeRPC;
no hay auditoría completa ante fallo de escritura ni garantía de cierre ante
terminación forzada. No hay rotación, índice ni soporte de varios procesos
escribiendo el mismo CSV. `nodo_cliente` es fijo para la escucha loopback actual.

## US-19: actividad propia implementada y verificada

Implementación, contrato y pruebas guardados en el commit local `f9e7cc6`.

`informe.propio` obtiene la identidad de Autenticador y permite a ambos roles
consultar solo sus peticiones. Rechaza usuario, rol, IDs y filtros adicionales;
acepta únicamente la paginación existente. RegistroActividadCsv calcula el periodo
mediante eventos de conexión/desconexión e inicio, sin otro registro ni sesiones.
El informe reúne estado actual, inicio propio, detalle y cantidades con errores.
La consulta en curso no se incluye a sí misma. El cliente ofrece `actividad` y
opción 21 para ambos perfiles. Ver [US-19](backlog/US-19.md) para criterios,
decisiones, clases, prueba manual y límites; CONTRATO.md contiene los tipos.

Verificaciones ejecutadas en esta etapa: `mingw32-make -C server test`, en
Windows nativo/PowerShell, MSYS2 UCRT64/g++ 15.2.0 y Python 3.14.0. Se recompilaron
los archivos afectados y pasaron dominio, usuarios, autenticación, 59 comprobaciones
de tareas, CSV, 49 pruebas Python de cliente y 35 de integración con C++ real.
Dos casos de enlaces simbólicos de tareas omitidos por permisos/plataforma.
La regresión incluye aislamiento entre dos operadores, consulta propia del admin,
permisos globales conservados, actividad vacía, límites, credenciales, cuenta
deshabilitada, persistencia global tras reinicio y comando/menú reales. El CSV,
las bases y las carpetas de tareas son temporales. El servicio espía cubre estado
parcial y ausencia de consultas del robot en pedidos rechazados.

Los criterios de US-19 están cubiertos con la interpretación documentada de
conexión serie y detalle seguro. No se ejecutaron Linux ni hardware. La conexión
exitosa y sus fronteras se prueban con eventos sintéticos del CSV, no con Arduino.
No se afirma que el informe mida movimiento físico. Se heredan el barrido completo,
la ausencia de instantánea entre páginas y las limitaciones ante fallos de registro.
US-19 ya está integrada en `origin/master`, base de esta etapa; sus resultados
anteriores se conservan como antecedentes.

## US-17 Hito 1: base local, sin ejecución remota

Rama `us-17-ejecucion-tareas`, base `d86c176`. Implementados leer por ID con
propiedad, ProgramaTarea y órdenes tipadas, CanalSerie sustituible y
Robot::ejecutarOrdenAutomatica. No hay hilo, ejecución de archivo completo,
RPC nuevo ni cambios del cliente. No se publican aún tarea.ejecutar/tarea.estado.
Las decisiones y explicación de clases se mantienen en [US-17](backlog/US-17.md)
y D15 de DECISIONES_DISENO.md.

Verificación de esta etapa en Windows nativo/PowerShell, g++ MSYS2 UCRT64 15.2.0
y Python 3.14.0:

- La prueba específica inicial pasó con 125 comprobaciones. Tras ampliar casos,
  la versión final pasó con 132; incluye archivo administrado → parser → Robot →
  canal sustituido. Dos casos nuevos de enlaces simbólicos omitidos por permisos.
- La primera regresión completa detectó violación de acceso por objetos antiguos
  compilados con otra disposición de Robot. Se corrigió la generación de
  dependencias del Makefile y se reconstruyó con `mingw32-make -B -C server test`.
- Esa reconstrucción y suite completa terminaron correctamente: dominio, usuarios,
  autenticación, 59 comprobaciones de tareas, CSV, 132 del Hito 1, 49 pruebas Python
  del cliente y 35 de integración XML-RPC con el servidor C++ real. Las dos
  omisiones anteriores de enlaces de tareas se suman a las dos de la prueba nueva.
- Un ensayo sin compilar (`make -n -W include/domain/Robot.h build/test_domain.exe`)
  confirmó que las dependencias corregidas detectan el cambio de cabecera.

Los archivos, bases y CSV de prueba son temporales; no se modificaron datos
reales. Las pruebas nuevas sustituyen el canal completo y comprueban secuencias,
respuestas y plazos pedidos, no tiempos ni bytes de un Arduino. PuertoSerie se
compiló para Windows, pero su intercambio con hardware no se ejecutó aquí.
No se ejecutó Linux ni se modificó firmware.

M114 como barrera de fin de interpolación y la tolerancia de comparación de
posición siguen pendientes de verificar con el firmware corregido del equipo.
El código y las respuestas simuladas no certifican finalización física. Los
antecedentes de reverificación de US-12 incorporados desde master se conservan;
no se presentan como prueba física de US-17 ni de este entorno.
US-17 no está completa ni integrada a master. Se detiene el trabajo al cerrar
este Hito 1, sin commits, push ni merges.

## Trabajo paralelo y pendientes generales

No se presupone el trabajo local de otros integrantes. El Hito 1 de US-17 extiende Robot y extrae el canal serie; no modifica firmware. Pendientes: enseñanza de trayectorias y ejecución
automática; alcance restante del informe global US-20; CLI administrativa completa;
coordinación de acceso; entrega y diagramas. La US-23 completa sigue abierta.
US-17 sigue pendiente de ejecución completa, atención concurrente, RPC y cliente
del Hito 2. La integración a master no se realiza en esta tarea.

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

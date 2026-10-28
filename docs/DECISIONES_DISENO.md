# Decisiones de diseno: usuarios y autenticacion

Fecha: 26 de octubre de 2026.
Base revisada: master, commit f4be8c1.
Estado: US-03 y US-04 implementadas y verificadas por el equipo; los apartados de cada hito conservan su contexto historico. Para el estado actual ver ESTADO_PROYECTO.md.

## Objetivo y orden

Trabajar US-03 y US-04 como una misma etapa, en dos hitos: primero persistencia de usuarios (US-04); despues validacion centralizada de cada solicitud (US-03). La segunda depende de los datos de la primera. Mantener commits separados para facilitar revision y pruebas.

## D01. Persistencia con SQLite

La consigna exige una tabla en una base de datos; un CSV no satisface este requisito. Usar SQLite desde C++ y guardar la base en data/db/usuarios.sqlite. Solo el servidor accede a ella. SQLite evita instalar y administrar otro proceso servidor de base de datos.

La ruta debe ser configurable mediante argumento, manteniendo una ruta por defecto documentada y ejecucion desde la raiz. Las pruebas usaran bases temporales, sin tocar la base real.

## D02. Campos de usuario y roles

Guardar id, nombre de usuario unico, hash de la clave, rol y habilitado. Roles iniciales: ADMINISTRADOR y OPERADOR. No crear una jerarquia de clases para los roles; usar una enumeracion y validaciones simples. El campo habilitado permite cumplir US-03; las operaciones para cambiarlo corresponden a US-05.

## D03. Un unico administrador

Inicializar el primer administrador mediante una accion explicita de consola, con clave elegida por el equipo. No incluir claves reales fijas en el repositorio; las pruebas usan claves ficticias y bases temporales. La inicializacion no debe sobrescribir una base existente ni regenerar credenciales en cada arranque.

Aplicar una restriccion de unicidad parcial para impedir mas de un ADMINISTRADOR. Esa restriccion garantiza como maximo uno; la inicializacion y las comprobaciones de arranque garantizan la existencia de uno. En esta etapa no ofrecer eliminacion ni degradacion del administrador.

## D04. Almacenamiento de claves

Usar las funciones de almacenamiento y verificacion de claves de libsodium. Guardar su cadena de verificacion (incluye hash, salt y parametros), no la clave original. No implementar criptografia propia ni usar std::hash para contrasenas. Esta decision agrega una dependencia, pero evita desarrollar y mantener un algoritmo criptografico.

No registrar ni devolver claves ni hashes en respuestas o logs. No versionar la base real, archivos auxiliares de SQLite ni credenciales. Agregar reglas especificas de ignore para los archivos generados de la base, conservando scripts y documentacion necesarios.

## D05. Credenciales en cada solicitud

Conservar los seis campos actuales de MensajeRPC. Incorporar parametros.credenciales como estructura con usuario y clave, separada de los parametros propios de la operacion. No agregar un septimo campo ni introducir sesiones/tokens en esta etapa.

El cliente puede pedir las credenciales una vez al iniciar el menu y conservarlas en memoria mientras esta abierto, pero debe enviarlas y el servidor debe verificarlas en cada peticion. No persistirlas en archivos del cliente ni pasarlas por argumentos de terminal.

Para cumplir literalmente la validacion de cada peticion y evitar excepciones de acceso prematuras, proteger tambien ping y eco. Actualizar contrato y pruebas al cambiar esa regla.

## D06. Autenticacion centralizada y permisos

Antes de ejecutar una operacion, comprobar formato de credenciales, existencia del usuario, clave y estado habilitado. Despues comprobar los permisos que requiera la operacion. Evitar repetir la autenticacion en cada controlador de operacion y evitar ejecutarla solo desde el menu.

Autenticacion responde quien realiza la solicitud; autorizacion responde si puede realizar la accion. Rechazar antes de invocar la operacion y antes de generar efectos.

Para solicitudes bien formadas: usuario inexistente o clave incorrecta producen NO_AUTENTICADO y el mensaje Usuario o clave incorrectos. Una cuenta con clave correcta pero deshabilitada produce PERMISO_DENEGADO con explicacion legible. La ausencia de credenciales produce NO_AUTENTICADO y los tipos incorrectos VALIDACION. Un fallo de base de datos produce ERROR_INTERNO y no se interpreta como una clave incorrecta. Los errores del sobre XML-RPC conservan el tratamiento actual como Fault.

## D07. Responsabilidades OO

Usuario: datos de identidad, rol y habilitacion, sin imprimir menus ni generar XML.
RepositorioUsuarios: crea y consulta la tabla, incorpora usuarios y encapsula SQLite, sus sentencias preparadas y la liberacion de recursos.
Autenticador: verifica la clave y el estado y devuelve la identidad autenticada.

El servidor construye las dependencias y las entrega a los servicios. El cliente no consulta SQLite. Mantener objetos por valor y referencias cuando resulte apropiado, sin agregar new manual ni jerarquias de repositorios para esta etapa.

Los servicios de consulta no expondran el hash. La administracion para incorporar/listar usuarios requerira permisos de administrador; el alta inicial es una excepcion local y explicita de bootstrap, no un servicio remoto sin autenticacion.

## D08. Limite de alcance

Esta etapa abarca persistencia, alta/consulta minima de usuarios, administrador inicial y autenticacion en cada solicitud. El cambio remoto de habilitacion (US-05), el robot, el log completo y los reportes se implementan en etapas posteriores. Usar usuarios deshabilitados de prueba para verificar US-03, sin dar por implementada US-05.

## Verificacion de aceptacion prevista

US-04: persistencia tras cerrar y reabrir; usuario duplicado rechazado; segundo administrador rechazado; inicializacion repetida no modifica las claves ni los usuarios existentes; consulta sin hashes; restricciones de acceso administrativo.

US-03: credenciales correctas, incorrectas, ausentes o con tipos invalidos; usuario inexistente; cuenta deshabilitada; verificacion en dos peticiones sucesivas, incluyendo cambio de estado entre ellas; una solicitud rechazada no invoca el servicio ni produce efectos. Las pruebas de solo codigo de error no bastan para demostrar ausencia de ejecucion.

Estado: US-03 y US-04 implementadas y verificadas en Windows y Linux.

## Referencias

- Consigna TPI 2026 y criterios US-03/US-04 compartidos por el equipo.
- https://www.sqlite.org/whentouse.html
- https://doc.libsodium.org/password_hashing/default_phf

## Actualizacion del hito US-04

Se incorpora una consola local separada (usuarios_admin) para inicializar, agregar operadores y listar. Agregar/listar requieren un administrador autenticado. Se usa struct Usuario para la vista simple de datos, sin secretos, y enum Rol. RepositorioUsuarios encapsula SQLite y ofrece verificarClave para esta herramienta y la integracion posterior. El Autenticador de peticiones se agrega en US-03, no en este hito.

El nombre de usuario se limita a 1-64 caracteres ASCII alfanumericos, guion y guion bajo; distingue mayusculas. La clave se limita a 8-256 bytes UTF-8. Se usa --db para configurar la ruta; el valor por defecto se resuelve desde el directorio de ejecucion, documentado como raiz del proyecto. Ver docs/backlog/US-04.md.

## Actualizacion US-03: puerta de acceso comun

Autenticador comprueba credenciales y estado consultando el repositorio en cada peticion. ServicioProtegido lo usa antes de invocar las operaciones. Las credenciales se retiran de la copia entregada al servicio para que no sean parametros del robot ni terminen en registros de trabajo. ping y eco requieren autenticacion. Los metodos system.* de la biblioteca conservan metadatos de XML-RPC, sin acceso directo al servicio de aplicacion.

Se conserva ServicioPrueba como objeto separado y se hace virtual su operacion atender para sustituirlo en pruebas por un servicio espia. Esto permite comprobar que no se invoca con credenciales rechazadas, sin inventar movimientos del robot ni agregar contadores al servicio de produccion.

La consola Python usa getpass y conserva las credenciales en memoria; no se agregan argumentos con contrasenas ni archivos de sesion. Cada peticion vuelve a enviarlas. La base existente no cambia de esquema. El servidor exige un administrador habilitado al arrancar. La habilitacion remota sigue fuera de este hito (US-05); las pruebas cambian el estado directamente sobre bases temporales.

## Actualizacion US-10 a US-13: control manual

El estado de modo de trabajo y de coordenadas vive en `Robot` y se representa
con dos enumeraciones distintas. El primero inicia en manual y el segundo en
absoluto. `G90`/`G91` implementan las coordenadas del firmware; se repite el
modo seleccionado antes de cada movimiento para sincronizarlo incluso si fue
seleccionado estando desconectado.

Los comandos manuales son `G28` para origen, `G1` para movimiento, `M3` para
cerrar la pinza y `M5` para abrirla, según el firmware actual. Origen,
movimiento y efector validan modo manual y conexión en el dominio, no solo en
la interfaz.

El movimiento replica los límites geométricos de `config.h` y usa velocidad
por defecto de 5 mm/s y máxima de 10 mm/s. El firmware transforma valores
menores de 5 mm/s en una velocidad automática; por eso no se aceptan como
velocidad explícita. El cambio no agrega bibliotecas ni altera el sobre XML-RPC
de seis campos; se agregan operaciones y parámetros en el contrato de
aplicación. Los tiempos de espera serie se ajustan por operación: las órdenes
con ejecución diferida como `G28`, `G1` y `M3`/`M5` requieren más tiempo que
una consulta de estado, por lo que un límite único de un segundo cortaba la
respuesta antes del `OK`.

## Prueba US-10 a US-13 en Arduino simulado

El 27/10/2026 se detectó en Windows un Arduino por USB-serie CH340 (COM5). El
controlador aceptó `G90`, `G91`, `G28`, `M3` y `M5`. `G28` tardaba tres
segundos en la simulación, por encima del timeout previo; el timeout por
comando permitió recibir `HOMING COMPLETE` y `OK`. El firmware de simulación
define `SIMULATION=true` y compila sin escrituras a los pines de motores y
pinza.

La respuesta `M114` no incluyó la línea de posición esperada. Por eso `G1` no
se envió desde `Robot::mover`: no se debe validar una trayectoria sin conocer
el punto actual, en especial en modo relativo. La prueba de movimiento queda
pendiente hasta que el firmware entregue coordenadas interpretables.

## D09. Primer avance US-23 sin consulta de permisos (historico; actualizado por D10)

Consumir solo las operaciones publicadas en origin/ConState, commit bebb692,
reutilizando mensajes, transporte y credenciales por solicitud. No cambiar el contrato
ni inferir roles: esa referencia no ofrece una consulta de identidad o permisos.
Conectar y desconectar se rotulan como administrativas y se muestra el rechazo
del servidor. Filtrar el menu queda pendiente de una respuesta autenticada acordada;
inventar una consulta o confiar en un rol declarado por el cliente seria incorrecto.

Presentar todos los datos devueltos, incluidos los anidados y los parciales de errores.
Conservar los indicadores disponible para no confundir datos desconocidos con ceros
o estados inactivos. La confirmacion del firmware no prueba una accion fisica;
un fallo de comunicacion no habilita reintentos automaticos. Ver docs/backlog/US-23.md
para cambios, ejemplos, pruebas y limites de este avance.

## D10. Identidad autenticada para el menu US-23 (27/10/2026)

Agregar usuario.actual dentro de ServicioProtegido, despues de autenticar. Reutiliza
el Usuario ya obtenido y devuelve solo nombre y rol, sin otra consulta a la base,
sesiones ni cambios en Robot. Rechaza parametros adicionales. El contrato exacto
se mantiene en CONTRATO.md, no en una segunda definicion.

Elegimos devolver los dos roles existentes en lugar de crear una lista de permisos
o una jerarquia nueva: alcanza para las opciones actuales y es el alcance solicitado.
El cliente usa el rol para presentar el menu; la autorizacion sigue en el servidor
en cada pedido. Si faltan datos o el rol es desconocido no se abre el menu operativo.
Los comandos directos conservan el rechazo del servidor. El menu no se abre tampoco
contra una version vieja que no implemente usuario.actual; se informa su respuesta.

La integracion actual conserva el control manual de US-10 a US-13 junto con la
identidad y las opciones de US-23. Las opciones de conexion y administracion se
muestran solo al administrador; la autorizacion sigue siendo responsabilidad del
servidor.

## D11. Tareas en archivos por propietario: US-15/US-16/US-24, Hito A

Fecha: 27/10/2026. Aceptado por el equipo. Base: 92f0f81.

Problema: conservar archivos y su propietario sin mezclar almacenamiento, robot
y transporte. Solucion: RepositorioTareas guarda en raiz/ID/nombre y Tarea representa
id logico, propietario_id y nombre. El Usuario debe provenir de la autenticacion
del servidor; no se recibe una identidad elegida por el cliente. En este hito la API
es local; autenticar el pedido y conectarla a RPC queda para Hito B.

La consigna y US-15 piden archivos en un directorio administrado. Guardar todo en
SQLite no cumple literalmente ese criterio. Archivos mas una tabla de tareas es
posible, pero obliga a sincronizar dos almacenamientos ante fallos. Elegimos archivos
por ID: la carpeta conserva la propiedad y SQLite sigue conteniendo solo usuarios.
No duplicamos contenido, nombres de usuario ni un indice persistente de tareas.
OPERADOR ve solo lo propio; ADMINISTRADOR ve todo, segun los criterios de US-16.

Los nombres se normalizan a minusculas, admiten base ASCII con letras, numeros,
guion y guion bajo y extension .gcode/.nc, hasta 64 caracteres; se rechazan rutas
y nombres reservados en Windows. Duplicados del mismo propietario se rechazan sin
sobrescribir; propietarios distintos pueden usar el mismo nombre. El ID logico
combina propietario y nombre, por ejemplo 2:pieza.gcode.

Se acepta hasta 1 MiB de texto UTF-8 compatible con XML y se normalizan BOM inicial
y saltos de linea. Se revisaron los comandos del firmware y los ejemplos en codigo
y documentacion; no hay archivos .gcode/.nc de ejemplo independientes. El control
de formato reconoce prefijos G/M numericos, minusculas y formas compactas, comentarios,
N y separadores %. No interpreta argumentos ni movimientos ni garantiza que el
firmware pueda ejecutar lo guardado. Las reglas y limites estan en US-15.md.

Se usan fstream y filesystem de C++17 para escribir y recorrer archivos sin otra
biblioteca ni comandos de un sistema operativo. Los streams cierran recursos al
salir de su alcance; ademas se comprueba el cierre antes de publicar. No se reutiliza
File_396 directamente porque esta acoplada a CSV, agrega lineas y omite lineas vacias.
Se reutilizan los criterios de encapsulacion y manejo de errores de los TPs.

UTF-8 representa caracteres con uno a cuatro bytes. Su control recorre esos bytes
para rechazar secuencias invalidas y caracteres que el contrato XML no puede llevar;
no es un interprete G-Code. El costo es una comprobacion lineal y breve sin instalar
otra biblioteca. Un enlace simbolico apunta a otra ubicacion: se comprueba su tipo
antes de guardar y se evita recorrerlo al listar.

El temporal se escribe en la carpeta destino y se renombra al terminar. Los listados
ignoran temporales; fallos normales no publican contenido parcial. Se admite un
unico proceso escritor con llamadas secuenciales, como el servidor actual. Evitamos
hilos, bloqueos distribuidos y garantias de corte de energia fuera de este alcance.
No se siguen enlaces en carpetas de propietario ni archivos administrados. Se agrega
una excepcion ErrorTarea con tipos distinguibles, sin una jerarquia adicional.

**Usuarios y tareas forman una unidad de respaldo:** conservar la base SQLite y
las carpetas juntas. Reemplazar/reinicializar la base puede reutilizar IDs y cambiar
la asociacion de propietarios. No se implementa reasignacion ni migracion automatica.
Esta es una limitacion de guardar la propiedad por ID, no una validacion que haga
el sistema. Los datos subidos y sus temporales quedan excluidos de Git.

Hito A no cambia autenticacion, esquema de usuarios, Robot, RPC ni cliente. Los
resultados ejecutados y lo pendiente se registran en ESTADO_PROYECTO.md y cada historia.

## D12. Hito B: tareas autenticadas y cliente de consola

Fecha: 27/10/2026. Base: 08b0f4e (Hito A). Se reutiliza RepositorioTareas sin cambios
en sus reglas ni en el esquema de usuarios. ServidorXmlRpc construye el repositorio
con una carpeta configurable por --tareas-dir, data/trayectorias por defecto, y lo
entrega por referencia a ServicioProtegido. Las pruebas pasan siempre una carpeta
temporal explicita, tambien al intentar iniciar un segundo servidor.

ServicioProtegido autentica antes de leer parametros de tareas y entrega al
repositorio el Usuario resultante. Los campos que pretendan elegir propietario o
ruta se rechazan. Resuelve los nombres de propietario con los usuarios existentes,
sin guardarlos otra vez en archivos. No delega subir/listar a ServicioRobot: no hay
conexion serie, movimiento ni ejecucion involucrada. Un servicio espia de prueba
comprueba esa separacion usando el metodo virtual ya existente.

Se agregan solo tarea.subir y tarea.listar; CONTRATO.md es su definicion unica.
ErrorTarea se traduce a los codigos existentes: validacion, estado invalido para
duplicados, permiso denegado o error interno. No se agregan codigos, sesiones ni
base de tareas. Un listado sin resultados es OK con una lista vacia.

Python reutiliza ServiciosPrueba y ClienteXmlRpc. pathlib representa la ruta local;
se consulta el tamano antes de abrir y se lee como maximo 1 MiB + 1 byte por si el
archivo crece. La decodificacion utf-8-sig admite y quita BOM. Son herramientas de
la biblioteca estandar, sin paquetes nuevos. El servidor vuelve a validar todo:
los controles locales solo ahorran lecturas y dan errores mas claros, no conceden
permisos ni certifican G-Code. El nombre enviado no incluye la ruta local.

La consola ofrece subir/tareas y opciones 17/18 para ambos perfiles. El listado se
presenta en filas con ID, nombre y propietario. Un fallo de red informa resultado
no confirmado, sin reintentar ni afirmar ejecucion fisica. Si se perdio la respuesta
de una subida, consultar tareas permite comprobar si quedo guardada.

La prueba de concurrencia usa clientes reales contra el servidor secuencial:
de cuatro subidas al mismo nombre/propietario solo una puede publicar. No se
amplia el alcance a varios procesos escritores ni se agregan bloqueos nuevos.
Se conserva la necesidad de respaldar juntos usuarios y carpetas; no se implementan
edicion, borrado, descarga ni ejecucion automatica.

## D13. Registro CSV acumulativo y consultas administrativas

Fecha: 28/10/2026. Decisiones observadas en `b155927`, revisadas en
`revision-logs-informes` (`bbb845f`); no integradas a master en esta revisión.
El formato, las rutas y el alcance se mantienen en
[US-18/US-20/US-21](backlog/US-18-20-21.md); el detalle RPC está en CONTRATO.md.

CSV es texto organizado en filas y columnas separadas por comas. Se usa para el
registro acumulativo que exige la consigna, página 3: se agregan filas y se conserva
el archivo al reiniciar. RegistroActividadCsv encapsula lectura, escritura y escape
de comas y comillas, con streams y herramientas estándar de C++17, sin nuevas
bibliotecas. El logger del TP anterior sigue compilándose, pero no genera este
registro. Guardarlo solo en SQLite no cumpliría literalmente el CSV pedido;
mantener además una tabla duplicaría datos y exigiría sincronizar ambos formatos.
El costo elegido es recorrer todo el archivo en cada consulta y administrar su
crecimiento, ya que no hay rotación ni límite de tamaño automático.

Las fechas se guardan en UTC (una referencia horaria común, independiente del
huso local), con sufijo `Z`. Los filtros toman días completos en esa misma escala,
para evitar que equipos con distinta hora local interpreten rangos diferentes.
El usuario debe convertir sus fechas locales si necesita un rango local preciso;
la marca depende del reloj del servidor y no garantiza sincronización externa.

Se reutiliza la identidad devuelta por Autenticador, después de validar también
la habilitación. No se atribuyen intentos fallidos al nombre declarado por el
cliente: quedan sin usuario verificado. Esto amplía D06 al registro e impide que
un intento con credenciales falsas incremente el informe de otra cuenta.
Las consultas requieren administrador en ServicioProtegido, antes de leer el CSV;
ocultarlas en el menú solo adapta la presentación. No se agregan sesiones ni
campos al sobre RPC. El nodo se fija a loopback por la escucha actual; admitir
clientes remotos requerirá obtener su dirección real del transporte.

D04 ya prohíbe registrar claves y hashes. Se extiende esa omisión a parámetros,
contenido de tareas y texto de eco: el registro guarda operación, código y un
detalle genérico. Se reduce así la exposición de datos; a cambio, el CSV no permite
reconstruir argumentos ni trayectorias y no demuestra movimientos físicos.
El informe cuenta respuestas de peticiones autenticadas, no sesiones ni órdenes
desde la última conexión. Los requisitos de reporte aún no cubiertos se explican
en ESTADO_PROYECTO.md; esta decisión no los elimina.

Un mutex (bloqueo de la biblioteca estándar) evita que lecturas y escrituras del
mismo objeto se intercalen dentro del proceso. No coordina servidores distintos;
se conserva un único proceso escritor. La paginación limita la respuesta, pero
no el barrido del CSV ni el tamaño total del informe. No hay índice, instantáneas
entre consultas ni garantías frente a cortes de energía. Los límites RPC y la
saturación de contadores a int32 se describen en CONTRATO.md.

Si falla el registro de una petición, se conserva la respuesta de la operación
y se advierte en stderr: devolver un error de registro como si hubiera fallado
el movimiento podría motivar repetir una orden ya ejecutada. El costo es que
la auditoría deja de ser completa hasta resolver el fallo. Los errores de creación
del registro o de escritura de eventos de inicio/cierre pueden impedir el arranque
o el cierre normal; esos casos no tienen la misma recuperación que una petición.

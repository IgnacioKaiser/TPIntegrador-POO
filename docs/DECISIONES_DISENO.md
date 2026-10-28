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

En la verificación inicial del 27/10, `M114` no incluyó la línea de posición
esperada y `G1` no se envió desde `Robot::mover`. En la reverificación del
28/10, `M114` informó una posición interpretable y se confirmó un movimiento
simulado de 1 mm y su retorno. Los resultados están registrados en
`docs/backlog/US-12.md` y `docs/VERIFICACION.md`.

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

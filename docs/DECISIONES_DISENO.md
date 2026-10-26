# Decisiones de diseno: usuarios y autenticacion

Fecha: 26 de octubre de 2026.
Base revisada: master, commit f4be8c1.
Estado: US-04 implementada en el entregable del hito 1, pendiente de verificacion nativa del equipo en Windows; US-03 pendiente.

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

Verificar compilacion y pruebas en Windows y Linux antes de integrar a master. Actualizar este documento ante cualquier decision posterior, indicando razon y estado de verificacion.

## Referencias

- Consigna TPI 2026 y criterios US-03/US-04 compartidos por el equipo.
- https://www.sqlite.org/whentouse.html
- https://doc.libsodium.org/password_hashing/default_phf

## Actualizacion del hito US-04

Se incorpora una consola local separada (usuarios_admin) para inicializar, agregar operadores y listar. Agregar/listar requieren un administrador autenticado. Se usa struct Usuario para la vista simple de datos, sin secretos, y enum Rol. RepositorioUsuarios encapsula SQLite y ofrece verificarClave para esta herramienta y la integracion posterior. El Autenticador de peticiones se agrega en US-03, no en este hito.

El nombre de usuario se limita a 1-64 caracteres ASCII alfanumericos, guion y guion bajo; distingue mayusculas. La clave se limita a 8-256 bytes UTF-8. Se usa --db para configurar la ruta; el valor por defecto se resuelve desde el directorio de ejecucion, documentado como raiz del proyecto. Ver docs/backlog/US-04.md.

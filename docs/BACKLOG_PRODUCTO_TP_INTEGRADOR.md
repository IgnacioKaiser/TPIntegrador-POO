# Product Backlog — TP Integrador de POO

**Versión:** borrador inicial para revisar con el equipo  
**Producto:** sistema de manipulación remota de un robot de 3 grados de libertad  
**Arquitectura prevista:** cliente Python, servidor C++ mediante XML-RPC y firmware Arduino provisto por la cátedra.

## Cómo interpretar este backlog

- **Epic:** agrupación grande de funcionalidades relacionadas.
- **User Story:** necesidad concreta vista desde un tipo de usuario. Debe poder completarse aproximadamente en una semana y tener criterios de aceptación verificables.
- **Subtasks:** trabajos técnicos que permiten completar una historia.
- **Prioridad P0:** base o funcionalidad necesaria para desbloquear otras tareas. **P1:** funcionalidad obligatoria importante. **P2:** mejoras opcionales o posteriores.
- **Estado inicial:** es una propuesta de planificación, no una declaración de que el equipo ya haya completado las tareas. Antes de cargarla en la herramienta, el grupo debe confirmar prioridades, responsables y estado real.

Los requisitos funcionales de control, usuarios, archivos G-Code, informes y registros se derivan de la consigna del trabajo práctico. La prioridad concreta y la división en historias/subtareas son una propuesta para organizar el trabajo del grupo.

---

# EPIC E1 — Comunicación entre cliente y servidor

Objetivo: disponer de una comunicación XML-RPC confiable entre el cliente Python y el servidor C++, usando mensajes de dominio serializables.

## US-01 — Verificar la comunicación básica existente
**Prioridad:** P0 · **Estado inicial:** revisar/verificar implementación existente.

**Historia:** Como integrante del equipo, quiero comprobar que el cliente Python pueda enviar solicitudes al servidor C++ y recibir respuestas, para tener una base funcional antes de agregar operaciones del robot.

**Criterios de aceptación:**
- El servidor se inicia y escucha en la dirección/puerto configurados.
- El cliente puede ejecutar `ping` y recibir una respuesta válida.
- El cliente puede ejecutar `eco` y recibir el texto enviado.
- Las pruebas existentes de cliente-servidor se ejecutan sin errores.

**Subtasks:**
- Revisar las instrucciones actuales de compilación y ejecución.
- Ejecutar las pruebas desde un entorno limpio siguiendo el README.
- Documentar problemas encontrados y corregir únicamente los necesarios.

## US-02 — Normalizar el formato de mensajes RPC
**Prioridad:** P0.

**Historia:** Como desarrollador de cliente o servidor, quiero intercambiar mensajes con una estructura acordada, para que ambos extremos interpreten de la misma manera las solicitudes, respuestas y errores.

**Criterios de aceptación:**
- El mensaje incluye un identificador de solicitud y los campos acordados para operación, parámetros y resultado.
- El cliente Python y el servidor C++ serializan y deserializan el mismo contrato.
- Una solicitud inválida genera una respuesta de error entendible y no interrumpe el servidor.

**Subtasks:**
- Acordar/documentar la estructura del mensaje de dominio.
- Verificar la serialización y deserialización en ambos lenguajes.
- Agregar pruebas para mensajes válidos, incompletos y con tipos incorrectos.

# EPIC E2 — Usuarios, autenticación y permisos

Objetivo: identificar a cada usuario que realiza una petición y limitar sus operaciones según sus permisos.

## US-03 — Validar las credenciales en cada petición
**Prioridad:** P0.

**Historia:** Como usuario autorizado, quiero que el servidor valide mi usuario y contraseña en cada solicitud, para que las operaciones no se ejecuten sin autorización.

**Criterios de aceptación:**
- Cada servicio protegido verifica las credenciales recibidas.
- Una contraseña incorrecta, un usuario inexistente o una cuenta deshabilitada producen un rechazo explícito.
- Una petición rechazada no ejecuta la operación solicitada.

**Subtasks:**
- Definir cómo se transportan las credenciales en el mensaje RPC.
- Implementar la validación centralizada de credenciales.
- Probar accesos correctos, credenciales inválidas y cuentas deshabilitadas.

## US-04 — Mantener los usuarios y sus privilegios
**Prioridad:** P0.

**Historia:** Como administrador, quiero que el sistema mantenga los usuarios autorizados y sus privilegios, para gestionar el acceso al robot.

**Criterios de aceptación:**
- Se almacena la información necesaria de cada usuario: identificador, nombre, credencial y privilegios.
- Existe un único usuario administrador inicial.
- Los datos persisten luego de reiniciar el servidor.
- La implementación utiliza el mecanismo de almacenamiento acordado por el equipo.

**Subtasks:**
- Elegir y documentar el mecanismo de persistencia (por ejemplo, SQLite).
- Definir la estructura de datos de usuarios y permisos.
- Implementar la lectura y escritura de usuarios.
- Probar la persistencia y la existencia de un único administrador.

## US-05 — Habilitar y deshabilitar usuarios
**Prioridad:** P1.

**Historia:** Como administrador, quiero habilitar o deshabilitar el acceso de otro usuario, para poder administrar quién utiliza el sistema.

**Criterios de aceptación:**
- Sólo el administrador puede cambiar el estado de acceso de otros usuarios.
- El servidor confirma la operación o explica por qué fue rechazada.
- Un usuario deshabilitado no puede ejecutar operaciones protegidas.

**Subtasks:**
- Implementar la operación de habilitación.
- Implementar la operación de deshabilitación.
- Probar permisos y rechazo de peticiones de usuarios deshabilitados.

# EPIC E3 — Conexión y estado del robot

Objetivo: administrar la conexión serie con Arduino y conocer el estado actual del robot.

## US-06 — Conectar y desconectar el robot
**Prioridad:** P0.

**Historia:** Como administrador, quiero conectar o desconectar el robot del servidor, para controlar cuándo el sistema puede comunicarse con el dispositivo.

**Criterios de aceptación:**
- Sólo el administrador puede conectar o desconectar el robot.
- El servidor informa si la conexión se estableció o si ocurrió un error.
- Si el robot no está conectado, las operaciones que requieren comunicación serie devuelven un error controlado.

**Subtasks:**
- Definir la configuración del puerto serie.
- Implementar la conexión y desconexión en el servidor.
- Probar puerto disponible, puerto incorrecto y pérdida de conexión.

## US-07 — Consultar el estado del robot
**Prioridad:** P1.

**Historia:** Como usuario autorizado, quiero consultar el estado y la posición conocida del robot, para saber en qué condición se encuentra antes de operar.

**Criterios de aceptación:**
- La respuesta indica si el robot está conectado.
- Se informa la posición conocida y el estado de actividad cuando esa información esté disponible.
- Si no puede obtenerse un dato, la respuesta lo indica expresamente en vez de inventarlo.

**Subtasks:**
- Definir el objeto de dominio que representa el estado.
- Obtener y traducir la respuesta del controlador/firmware.
- Agregar pruebas para robot conectado y desconectado.

## US-08 — Activar y desactivar los motores
**Prioridad:** P1.

**Historia:** Como usuario con permisos suficientes, quiero activar o desactivar los motores del robot, para preparar o detener la operación según el procedimiento establecido.

**Criterios de aceptación:**
- El servidor envía los comandos admitidos por el firmware provisto.
- El usuario recibe confirmación o un error comprensible.
- La operación se rechaza si el estado de conexión no permite ejecutarla.

**Subtasks:**
- Verificar el comando exacto admitido por el firmware.
- Implementar los servicios para activar y desactivar motores.
- Probar respuestas exitosas y errores de conexión.

## US-09 — Consultar los comandos disponibles
**Prioridad:** P1.

**Historia:** Como operador, quiero consultar los comandos de control disponibles y su sintaxis, para usar correctamente el robot.

**Criterios de aceptación:**
- Se presenta una lista de comandos implementados en el sistema.
- La ayuda indica parámetros relevantes, límites conocidos y uso básico.
- No se presentan como disponibles comandos que la implementación no soporte.

**Subtasks:**
- Revisar el conjunto de comandos compatible con el firmware suministrado.
- Definir el formato de la ayuda.
- Exponer el servicio XML-RPC y mostrar su resultado en las interfaces.

# EPIC E4 — Control manual del robot

Objetivo: permitir los movimientos manuales requeridos por la consigna respetando límites de posición y velocidad.

## US-10 — Seleccionar el modo de trabajo y de coordenadas
**Prioridad:** P1.

**Historia:** Como operador, quiero seleccionar el modo manual o automático y el modo de coordenadas absolutas o relativas, para definir cómo se interpretan las órdenes de movimiento.

**Criterios de aceptación:**
- El modo de trabajo (manual/automático) se gestiona separadamente del modo de coordenadas (absoluto/relativo).
- El sistema informa el modo seleccionado.
- No se ejecuta una operación incompatible con el modo actual.

**Subtasks:**
- Definir las representaciones de los dos modos.
- Implementar las operaciones para seleccionarlos.
- Probar cambios de modo y operaciones incompatibles.

## US-11 — Mover el efector a la posición de origen/descanso
**Prioridad:** P1.

**Historia:** Como operador en modo manual, quiero enviar el efector a la posición de origen o descanso, para llevar el robot a una posición conocida de referencia.

**Criterios de aceptación:**
- La operación sólo se ejecuta en el modo permitido y con el robot conectado.
- El comando enviado corresponde al que admite el firmware.
- La interfaz muestra la respuesta obtenida del controlador.

**Subtasks:**
- Confirmar la semántica del comando de origen admitido por el firmware.
- Implementar el servicio del servidor.
- Probar el caso correcto y los casos de error.

## US-12 — Mover el efector a una posición cartesiana
**Prioridad:** P0.

**Historia:** Como operador en modo manual, quiero especificar la posición final X, Y, Z del efector, opcionalmente con una velocidad, para mover el robot hasta el punto deseado.

**Criterios de aceptación:**
- La operación acepta las coordenadas X, Y y Z.
- El operador puede indicar una velocidad o utilizar la velocidad predeterminada definida por el sistema.
- Se validan los límites geométricos y la velocidad máxima definidos para el robot.
- Si el comando es rechazado por el controlador, se muestra el error devuelto.

**Subtasks:**
- Identificar e incorporar los límites dimensionales y de velocidad de la documentación del robot.
- Implementar la validación de coordenadas y velocidad.
- Convertir la solicitud en el comando G-Code compatible.
- Probar posiciones válidas, fuera de espacio de trabajo y velocidades no permitidas.

## US-13 — Activar y desactivar el efector final
**Prioridad:** P1.

**Historia:** Como operador en modo manual, quiero activar o desactivar el efector final, para abrir o cerrar la pinza o controlar el efector simple del robot.

**Criterios de aceptación:**
- Existen operaciones diferenciadas para activar y desactivar el efector.
- Se envían comandos soportados por el firmware.
- La interfaz informa si la operación se ejecutó o fue rechazada.

**Subtasks:**
- Confirmar los comandos exactos del firmware.
- Implementar las operaciones en el servidor.
- Probar ambos estados y los errores posibles.

# EPIC E5 — Archivos G-Code y trayectorias

Objetivo: crear, almacenar, consultar y ejecutar secuencias de tareas del robot respetando la propiedad de los archivos.

## US-14 — Registrar una trayectoria paso a paso
**Prioridad:** P1.

**Historia:** Como operador en modo manual, quiero registrar las órdenes que voy ejecutando y guardarlas con un nombre, para reutilizar la trayectoria como archivo G-Code.

**Criterios de aceptación:**
- El operador puede iniciar y finalizar una grabación.
- Las órdenes correspondientes se guardan en un archivo de texto con formato G-Code.
- El archivo utiliza el nombre suministrado por el operador y queda asociado a su usuario.

**Subtasks:**
- Definir el ciclo de vida de una grabación.
- Implementar el registro de comandos y la escritura del archivo.
- Probar grabación vacía, grabación normal y finalización.

## US-15 — Subir y almacenar archivos G-Code
**Prioridad:** P1.

**Historia:** Como usuario autorizado, quiero subir un archivo G-Code al servidor, para conservar una secuencia de tareas que luego pueda ejecutar el robot.

**Criterios de aceptación:**
- El cliente permite seleccionar un archivo y enviarlo al servidor.
- El servidor valida la solicitud y guarda el archivo dentro del directorio de trayectorias administrado por el sistema.
- El archivo queda asociado a su propietario.
- Un nombre o contenido inválido se rechaza de forma controlada.

**Subtasks:**
- Definir cómo transferir el contenido del archivo mediante XML-RPC.
- Implementar el almacenamiento seguro y los metadatos de propiedad.
- Probar carga correcta, nombre inválido y archivo no permitido.

## US-16 — Listar trayectorias respetando la propiedad
**Prioridad:** P1.

**Historia:** Como operador, quiero ver los archivos G-Code de mi propiedad, para seleccionar una trayectoria que pueda ejecutar.

**Criterios de aceptación:**
- El operador sólo ve sus propios archivos.
- El administrador puede consultar todos los archivos.
- La lista permite identificar los archivos de manera inequívoca.

**Subtasks:**
- Implementar la consulta de archivos y sus propietarios.
- Aplicar las restricciones por rol.
- Probar las vistas de operador y administrador.

## US-17 — Ejecutar una trayectoria guardada
**Prioridad:** P1.

**Historia:** Como operador en modo automático, quiero seleccionar y ejecutar una trayectoria a la que tenga acceso, para realizar una tarea completa sin enviar cada orden manualmente.

**Criterios de aceptación:**
- La ejecución sólo se habilita en modo automático.
- El operador sólo puede ejecutar archivos propios; el administrador puede elegir cualquiera.
- El sistema comprueba conexión y estado antes de comenzar e informa errores durante la ejecución.

**Subtasks:**
- Implementar la selección del archivo y las validaciones de propiedad.
- Procesar la secuencia de comandos G-Code admitida por el firmware.
- Probar ejecución válida, archivo inaccesible y errores del controlador.

# EPIC E6 — Registro de actividad e informes

Objetivo: mantener trazabilidad de las operaciones y ofrecer informes distintos para operadores y administrador.

## US-18 — Registrar las peticiones y eventos del servidor en CSV
**Prioridad:** P1.

**Historia:** Como administrador, quiero que el servidor mantenga un registro acumulativo de peticiones y eventos, para poder auditar la actividad y diagnosticar problemas.

**Criterios de aceptación:**
- Para cada petición se registra fecha/hora, detalle, usuario, nodo cliente y código de respuesta.
- Para los eventos generales se registra fecha/hora, módulo y detalle del evento.
- El registro utiliza formato CSV y se conserva entre reinicios.

**Subtasks:**
- Definir y documentar las columnas CSV.
- Implementar el registro centralizado de peticiones y eventos.
- Probar operaciones exitosas, rechazos y eventos de error.

## US-19 — Consultar el informe de actividad propio
**Prioridad:** P1.

**Historia:** Como operador, quiero consultar mi actividad desde la última conexión, para conocer el estado del robot y las órdenes solicitadas.

**Criterios de aceptación:**
- El informe muestra, cuando esté disponible, estado de conexión, posición, actividad actual y momento de inicio.
- Muestra el detalle y la cantidad de órdenes solicitadas desde la última conexión, incluyendo las que produjeron errores.
- Un operador no puede consultar el detalle de otros usuarios.

**Subtasks:**
- Definir el modelo de datos del informe de actividad.
- Reunir los datos de estado, comandos y errores.
- Implementar y probar la restricción para que cada usuario vea sólo su detalle.

## US-20 — Consultar el informe global de usuarios
**Prioridad:** P1.

**Historia:** Como administrador, quiero consultar la actividad de todos los usuarios y filtrar los resultados, para supervisar el uso del sistema.

**Criterios de aceptación:**
- El administrador puede consultar la información sin la restricción de propiedad aplicada al operador.
- Se admiten al menos dos criterios de filtrado acordados por el equipo.
- Un usuario sin privilegios de administrador no puede acceder a este informe global.

**Subtasks:**
- Acordar al menos dos filtros (por ejemplo, usuario y rango de fechas).
- Implementar filtros y presentación en consola.
- Probar distintos filtros y permisos.

## US-21 — Consultar y filtrar el log del servidor
**Prioridad:** P1.

**Historia:** Como administrador, quiero consultar el log acumulativo del servidor y filtrar sus entradas, para investigar errores y eventos del sistema.

**Criterios de aceptación:**
- El informe muestra el detalle de las entradas relevantes del log.
- Permite filtrar por al menos dos criterios acordados por el equipo.
- Sólo el administrador accede a esta consulta.

**Subtasks:**
- Acordar los criterios de filtrado.
- Implementar lectura, filtrado y presentación del CSV.
- Probar filtros y rechazo de acceso no autorizado.

# EPIC E7 — Interfaces de usuario

Objetivo: ofrecer las funciones requeridas mediante la consola del servidor y mediante el cliente remoto.

## US-22 — Administrar el sistema desde la CLI del servidor
**Prioridad:** P1.

**Historia:** Como administrador, quiero acceder a un panel de consola del lado servidor después de autenticarme, para administrar el robot y consultar los informes requeridos.

**Criterios de aceptación:**
- El acceso al panel exige validar al administrador.
- El menú permite acceder a las funciones administrativas acordadas.
- Una operación inválida muestra un mensaje comprensible y el programa puede continuar.
- La interacción por consola no impide que el servidor atienda peticiones XML-RPC.

**Subtasks:**
- Definir el menú de administración.
- Conectar las opciones con los servicios internos.
- Resolver la convivencia entre lectura de consola y atención de solicitudes RPC.
- Probar las opciones y el manejo de entradas inválidas.

## US-23 — Operar el robot desde el cliente Python
**Prioridad:** P1.

**Historia:** Como usuario remoto autorizado, quiero utilizar una interfaz de consola en Python para acceder a las funciones permitidas, para controlar el robot sin operar directamente el servidor.

**Criterios de aceptación:**
- El cliente permite ingresar credenciales y muestra las opciones disponibles según los permisos.
- Las opciones invocan los servicios del servidor mediante XML-RPC.
- Los resultados y errores del servidor se presentan al usuario de forma clara.

**Subtasks:**
- Definir el menú del cliente según los perfiles.
- Integrar cada opción con el módulo de comunicación RPC.
- Probar el flujo con operador, administrador y usuario rechazado.

## US-24 — Subir archivos desde el cliente
**Prioridad:** P1.

**Historia:** Como usuario autorizado, quiero elegir desde el cliente un archivo G-Code y subirlo al servidor, para almacenarlo sin acceder manualmente al sistema de archivos del servidor.

**Criterios de aceptación:**
- La interfaz ofrece una opción específica para subir trayectorias.
- El usuario recibe confirmación o un error claro.
- El archivo queda sujeto a las mismas reglas de propiedad y almacenamiento que cualquier trayectoria cargada.

**Subtasks:**
- Implementar la selección/lectura local del archivo.
- Invocar el servicio XML-RPC de carga.
- Probar subida y manejo de errores.

# EPIC E8 — Verificación, documentación y entrega

Objetivo: comprobar el funcionamiento e incluir los entregables técnicos solicitados por la cátedra.

## US-25 — Probar un módulo mediante pruebas unitarias
**Prioridad:** P1.

**Historia:** Como equipo de desarrollo, queremos contar con pruebas unitarias para uno de los módulos aceptados por la consigna, para verificar su comportamiento de forma repetible.

**Criterios de aceptación:**
- El equipo elige y documenta el módulo a probar entre los indicados en la consigna.
- Las pruebas se pueden repetir siguiendo instrucciones documentadas.
- Se registra el resultado y se incluyen casos normales y de error.

**Subtasks:**
- Elegir el módulo (CLI de servidor, clase Robot/modelo, gestor de archivos o servidor RPC).
- Diseñar casos de prueba.
- Implementar y ejecutar las pruebas.
- Documentar resultados.

## US-26 — Documentar la arquitectura OO de cada programa
**Prioridad:** P1.

**Historia:** Como integrante del equipo y evaluador, quiero disponer de diagramas de clases separados para cliente y servidor, para comprender sus responsabilidades y comprobar que los diagramas reflejen el código implementado.

**Criterios de aceptación:**
- Existe un diagrama de clases del cliente y otro del servidor.
- Se muestran clases, atributos, métodos y relaciones importantes.
- Los diagramas coinciden con la implementación final.

**Subtasks:**
- Revisar las clases reales del cliente y del servidor.
- Crear ambos diagramas UML.
- Contrastar los diagramas con el código luego de la integración.

## US-27 — Documentar un escenario y el flujo general
**Prioridad:** P1.

**Historia:** Como evaluador, quiero ver un diagrama de secuencia de un escenario completo y un diagrama de actividad del servidor, para entender cómo se desarrolla una operación real del sistema.

**Criterios de aceptación:**
- El diagrama de secuencia describe un caso de complejidad intermedia de principio a fin.
- El diagrama de actividad ofrece una vista global del comportamiento del servidor.
- Ambos diagramas son coherentes con el código.

**Subtasks:**
- Seleccionar el escenario de trabajo.
- Elaborar y validar el diagrama de secuencia.
- Elaborar y validar el diagrama de actividad.

## US-28 — Preparar las instrucciones de instalación y uso
**Prioridad:** P1.

**Historia:** Como integrante nuevo del equipo o evaluador, quiero seguir instrucciones claras para configurar y ejecutar el sistema, para poder probarlo sin depender de explicaciones verbales.

**Criterios de aceptación:**
- Se documentan requisitos, compilación, configuración del puerto y ejecución de cliente/servidor.
- Se describe cómo usar el simulador o hardware disponible.
- Se incluyen capturas de las interfaces relevantes.

**Subtasks:**
- Redactar instrucciones de preparación y ejecución.
- Verificar las instrucciones en un entorno limpio.
- Capturar y añadir las pantallas requeridas.

## US-29 — Preparar el informe y el video final
**Prioridad:** P1.

**Historia:** Como equipo, queremos preparar el informe y el video de entrega, para presentar la solución, justificar decisiones y demostrar el funcionamiento.

**Criterios de aceptación:**
- El informe incluye los apartados requeridos por la consigna.
- El video está dentro de la duración solicitada y muestra explicación y demostración de uso.
- Los artefactos finales se preparan para la entrega en el aula virtual.

**Subtasks:**
- Distribuir y redactar las secciones del informe.
- Preparar una demostración reproducible.
- Grabar y revisar el video final.
- Verificar que el paquete de entrega contenga los archivos exigidos.

# EPIC E9 — Mejoras opcionales de interfaz

Estas historias se consideran de baja prioridad y sólo deberían iniciarse si las funcionalidades obligatorias están terminadas y el equipo dispone de tiempo. La consigna presenta estas mejoras como opcionales.

## US-30 — Incorporar una interfaz gráfica
**Prioridad:** P2 · **Opcional.**

**Historia:** Como usuario, quiero disponer de una interfaz gráfica, para operar el sistema sin depender exclusivamente de una terminal.

**Criterio de aceptación:** La interfaz gráfica utiliza los mismos servicios del servidor y no duplica la lógica del dominio.

**Subtasks:**
- Elegir tecnología de interfaz.
- Diseñar una pantalla mínima.
- Integrar y probar las funciones prioritarias.

## US-31 — Mostrar una representación visual del robot
**Prioridad:** P2 · **Opcional.**

**Historia:** Como operador, quiero ver una representación simplificada del robot o su posición, para comprender visualmente el estado del movimiento.

**Criterio de aceptación:** La visualización identifica claramente la posición/estado que representa y no reemplaza las validaciones del servidor.

**Subtasks:**
- Elegir el tipo de representación.
- Conectar la visualización con los datos disponibles.
- Verificar que la información mostrada sea coherente.

## US-32 — Evaluar video remoto o señales sonoras
**Prioridad:** P2 · **Opcional.**

**Historia:** Como operador, quiero recibir una señal audiovisual de ciertos eventos del robot, para tener una indicación adicional de su actividad.

**Criterio de aceptación:** Si se implementa, la función es opcional, no bloquea el control principal y su estado queda claramente indicado.

**Subtasks:**
- Acordar si se implementará video, sonido o ninguno.
- Evaluar una solución compatible con los recursos disponibles.
- Implementar y probar sólo si el equipo confirma la prioridad.

---

# Orden sugerido de implementación

1. Verificar y estabilizar la comunicación XML-RPC y el formato de mensajes (US-01 y US-02).
2. Implementar persistencia, autenticación y permisos (US-03 a US-05).
3. Implementar la conexión serie y las operaciones básicas de estado/control (US-06 a US-13).
4. Agregar gestión y ejecución de trayectorias (US-14 a US-17).
5. Implementar logs e informes (US-18 a US-21).
6. Integrar ambas interfaces (US-22 a US-24).
7. Completar pruebas, UML, documentación y entregables (US-25 a US-29).
8. Evaluar las mejoras opcionales sólo al final (US-30 a US-32).

# Acuerdos que debe cerrar el equipo

- Confirmar qué historias ya están terminadas y cuáles están en progreso. El repositorio actual documenta una base de comunicación `ping`/`eco`, pero no las funciones completas de control del robot.
- Acordar los criterios de filtrado de los dos informes administrativos; la consigna exige al menos dos criterios para cada informe.
- Incorporar los parámetros geométricos y de velocidad oficiales del robot, consultando la documentación provista por la cátedra. No inventar valores para los límites.
- Acordar la prioridad y los responsables de cada historia, sin asignar subtareas a personas hasta que el equipo lo decida.
- No planificar las mejoras opcionales antes de cubrir los requisitos obligatorios.

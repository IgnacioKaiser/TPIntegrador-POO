# Contrato de comunicación cliente-servidor

## 1. Objetivo

Este documento establece cómo intercambian mensajes el cliente y el servidor del proyecto de control del robot.

El cliente solicita operaciones; el servidor valida las solicitudes, ejecuta las acciones permitidas y devuelve respuestas. Cada parámetro se identifica mediante **un nombre y un tipo de dato**.

## 2. Transporte

- Protocolo: **XML-RPC sobre HTTP**.
- Método remoto: **`robot.intercambiar`**.
- Cada llamada recibe **un único argumento XML-RPC**, de tipo `struct`, que representa la solicitud completa.
- La respuesta del método es también **un único `struct` XML-RPC**.
- La biblioteca XML-RPC se encarga de codificar y decodificar los valores. El cliente no debe construir XML manualmente.

## 3. Estructura general del mensaje

Todos los mensajes tienen exactamente los siguientes seis campos:

| Campo | Tipo XML-RPC | Descripción |
| --- | --- | --- |
| `id` | `string` | Identificador de la solicitud, utilizado para relacionarla con su respuesta. |
| `tipo` | `string` | `solicitud` o `respuesta`. |
| `operacion` | `string` | Nombre de la acción solicitada. |
| `parametros` | `struct` | Parámetros de entrada, identificados por nombre. |
| `resultado` | `struct` | Datos devueltos por el servidor, identificados por nombre. |
| `codigo` | `string` | Resultado de la operación: vacío en solicitudes y código de estado en respuestas. |

Estos seis campos siempre están presentes. Cuando `parametros` o `resultado` no tienen datos, se envían como un `struct` vacío, no como texto vacío ni como `nil`.

## 4. Tipos de datos admitidos

Dentro de `parametros` y `resultado` se admiten:

| Tipo XML-RPC | Tipo habitual en C++ | Tipo habitual en Python |
| --- | --- | --- |
| `string` | `std::string` | `str` |
| `int` / `i4` | `int` de 32 bits | `int` dentro del rango admitido |
| `double` | `double` | `float` |
| `boolean` | `bool` | `bool` |
| `array` | `XmlRpcValue` de tipo arreglo | `list` |
| `struct` | `XmlRpcValue` de tipo estructura | `dict` |

Los arreglos y estructuras pueden contener otros valores de los tipos permitidos.

No se utilizarán `nil`, `dateTime.iso8601` ni `base64` en los mensajes de aplicación. Los valores `double` deben ser finitos (no `NaN` ni infinito).

**En C++**, `ValorRPC` y `EstructuraRPC` representan los valores y estructuras sin depender del transporte. El adaptador los convierte a `XmlRpc::XmlRpcValue` para XML-RPC. **En Python**, se utilizan los tipos nativos compatibles con XML-RPC.

## 5. Reglas de identificación y nombres

- `id`: entre 1 y 64 caracteres; letras ASCII, dígitos, guion (`-`) y guion bajo (`_`). Cada solicitud debe contar con su propio identificador.
- `operacion`: entre 1 y 64 caracteres; comienza con una letra ASCII y luego admite letras, dígitos, guion bajo (`_`), guion (`-`) o punto (`.`).
- Nombres de parámetros y resultados: comienzan con una letra ASCII y admiten letras, dígitos y guion bajo (`_`); máximo 64 caracteres. Se recomienda `snake_case`.
- Los nombres distinguen mayúsculas y minúsculas.

## 6. Solicitudes

Una solicitud debe cumplir estas condiciones:

- `tipo` vale `solicitud`.
- `parametros` contiene los valores de entrada definidos para la operación.
- `resultado` es un `struct` vacío.
- `codigo` es una cadena vacía (`""`).
- Los campos obligatorios, los tipos y los rangos de cada parámetro se comprueban antes de ejecutar la operación.
- Un parámetro desconocido o un tipo incorrecto se considera un error de validación, salvo que la definición de la operación indique expresamente lo contrario.

### Ejemplo: solicitud de movimiento

Representación conceptual, equivalente al `struct` XML-RPC que se envía por la red:

```python
{
    "id": "sol-001",
    "tipo": "solicitud",
    "operacion": "mover",
    "parametros": {
        "x": 0.0,
        "y": 170.0,
        "z": 120.0,
        "velocidad": 7.5
    },
    "resultado": {},
    "codigo": ""
}
```

Las unidades y límites de `mover` se especifican en la sección 10.

## 7. Respuestas

Una respuesta debe cumplir estas condiciones:

- `tipo` vale `respuesta`.
- `id` y `operacion` coinciden con los de la solicitud recibida.
- `parametros` es un `struct` vacío.
- `resultado` contiene los datos de salida de la operación. Puede estar vacío cuando no hay datos que devolver.
- `codigo` informa si la operación terminó correctamente o por qué no pudo completarse.

### Ejemplo: respuesta correcta

```python
{
    "id": "sol-001",
    "tipo": "respuesta",
    "operacion": "mover",
    "parametros": {},
    "resultado": {
        "mensaje": "Solicitud procesada"
    },
    "codigo": "OK"
}
```

Este ejemplo **no implica** que el robot haya alcanzado físicamente el destino: cada operación deberá especificar si su respuesta confirma recepción, ejecución o finalización.

## 8. Códigos de respuesta

| Código | Significado |
| --- | --- |
| `OK` | Operación completada según la definición de esa operación. |
| `VALIDACION` | Faltan parámetros, hay tipos incorrectos o valores fuera de rango. |
| `NO_AUTENTICADO` | Faltan credenciales o no permiten autenticar al usuario. No se usan sesiones. |
| `PERMISO_DENEGADO` | El usuario no tiene autorización para la operación. |
| `OPERACION_NO_SOPORTADA` | El servidor no reconoce la operación solicitada. |
| `ESTADO_INVALIDO` | La operación no puede ejecutarse en el estado actual del sistema. |
| `RECURSO_NO_ENCONTRADO` | No existe el recurso solicitado. |
| `ERROR_CONEXION` | No se pudo abrir o configurar el puerto serie del Arduino. |
| `ERROR_INTERNO` | Fallo interno al procesar la operación. |

En caso de error de aplicación, `resultado` puede incluir un campo `mensaje` de tipo `string` con una explicación legible.

### Ejemplo: error de validación

```python
{
    "id": "sol-002",
    "tipo": "respuesta",
    "operacion": "mover",
    "parametros": {},
    "resultado": {
        "mensaje": "El parametro x debe ser numerico"
    },
    "codigo": "VALIDACION"
}
```

## 9. Diferencia entre errores de protocolo y errores de aplicación

**Errores de protocolo:** si no se puede interpretar la llamada XML-RPC, falta un campo obligatorio de la estructura general, un campo general tiene un tipo inválido o la solicitud no respeta el formato externo, el servidor devuelve un **XML-RPC Fault**. En estos casos, puede no ser posible construir una respuesta de aplicación confiable.

**Errores de aplicación:** si el mensaje general está bien formado, pero la operación es desconocida, no tiene permiso o sus parámetros son inválidos, el servidor devuelve una **respuesta normal** con el código correspondiente.

**Errores de comunicación:** si la conexión falla o no llega una respuesta válida, el cliente informa un error de comunicación. No debe presentar ese caso como una confirmación de ejecución.

## 10. Operaciones implementadas

Todas las operaciones requieren `parametros.credenciales` y rechazan parámetros
desconocidos. En la tabla se indican solo los parámetros adicionales a las
credenciales. `ping`, `usuario.actual`, `desconectar`, `estado`, los comandos
de motores, `tarea.listar` y `ayuda` no aceptan parámetros adicionales.

| Operación | Parámetros | Resultado en `OK` | Comportamiento |
| --- | --- | --- | --- |
| `ping` | Ninguno | `mensaje: string` | Prueba del servidor, no requiere conexión al robot. |
| `eco` | `texto: string` | `texto: string` | Devuelve el texto recibido. |
| `usuario.actual` | Ninguno | `usuario: string`, `rol: string` | Devuelve únicamente la identidad autenticada de quien realiza el pedido. |
| `usuario.habilitar` | `usuario: string` | `usuario: string`, `habilitado: boolean`, `mensaje: string` | Habilita un operador; solo administrador. |
| `usuario.deshabilitar` | `usuario: string` | `usuario: string`, `habilitado: boolean`, `mensaje: string` | Deshabilita un operador; solo administrador. |
| `tarea.subir` | `nombre: string`, `contenido: string` | `id: string`, `nombre: string`, `propietario: string`, `mensaje: string` | Almacena una tarea del usuario autenticado; no ejecuta órdenes. |
| `tarea.listar` | Ninguno | `tareas: array` de `struct` | Operador: tareas propias. Administrador: todas. |
| `log.consultar` | Filtros y paginación opcionales (ver abajo) | `registros: array`, `total: int`, `offset: int`, `mostrados: int` | Consulta del CSV; solo administrador. |
| `informe.usuarios` | Filtros opcionales (ver abajo) | `usuarios: array`, `total_peticiones: int`, `total_ok: int`, `total_errores: int` | Totales de peticiones por identidad autenticada; solo administrador. |
| `informe.propio` | `offset: int`, `limite: int`, opcionales | Estado, referencia del periodo, inicio, detalle y totales (ver abajo) | Solo la actividad de la identidad autenticada; ambos roles. |
| `conectar` | `puerto: string` opcional | `mensaje: string` | Abre el puerto a 115200 baudios, 8N1. Por defecto usa `/dev/ttyACM0` en Linux y `COM3` en Windows. |
| `desconectar` | Ninguno | `mensaje: string` | Cierra la conexión si está abierta; también es válida si ya estaba desconectado. |
| `estado` | Ninguno | `conectado: boolean`, `posicion: struct`, `actividad: struct`, `modo_trabajo: string`, `modo_coordenadas: string` | Devuelve el estado de conexión y los modos seleccionados. Si está conectado, consulta `M114`; cada dato no informado se marca `disponible: false` con un `mensaje` explicativo. |
| `activar_motores` | Ninguno | `respuesta: string` | Envía `M17`. |
| `desactivar_motores` | Ninguno | `respuesta: string` | Envía `M18`. |
| `seleccionar_modo_trabajo` | `modo: string` (`manual` o `automatico`) | `modo_trabajo: string`, `mensaje: string` | Guarda el modo de trabajo, separado del modo de coordenadas. Inicia en `manual`. |
| `seleccionar_modo_coordenadas` | `modo: string` (`absoluto` o `relativo`) | `modo_coordenadas: string`, `respuesta: string`, `mensaje: string` | Guarda el modo y, si está conectado, envía `G90` o `G91`. Si está desconectado, se aplica antes del próximo movimiento. Inicia en `absoluto`. |
| `ir_a_origen` | Ninguno | `respuesta: string` | En modo manual y conectado, envía `G28`. |
| `mover` | `x: number`, `y: number`, `z: number`, `velocidad: number` opcional | `respuesta: string`, coordenadas y velocidad solicitadas | En modo manual y conectado, envía `G1`; las coordenadas se interpretan con el modo seleccionado. |
| `activar_efector` | Ninguno | `respuesta: string`, `mensaje: string` | En modo manual y conectado, envía `M3` para cerrar la pinza. |
| `desactivar_efector` | Ninguno | `respuesta: string`, `mensaje: string` | En modo manual y conectado, envía `M5` para abrir la pinza. |
| `ayuda` | Ninguno | `comandos: string` | Informa las operaciones disponibles. |

`posicion` informa `disponible`, `mensaje` cuando no está disponible, y
coordenadas `x`, `y`, `z`, `e` en milímetros cuando el firmware devuelve el
formato `CURRENT POSITION: [X:... Y:... Z:... E:...]`. `actividad` contiene
`motores`, `ventilador` y `modo_coordenadas`. El modo seleccionado se conoce
también cuando el robot está desconectado; los demás datos del controlador
declaran si están disponibles.

`estado` se puede consultar desconectado: responde `conectado: false` y marca
posición, motores y ventilador como no disponibles, en lugar de simular
lecturas. Los modos de trabajo y coordenadas se informan con los valores
seleccionados. Las operaciones de origen, movimiento y efector requieren modo
manual y conexión serie; en otro modo o sin conexión responden
`ESTADO_INVALIDO`. `activar_motores` y `desactivar_motores` requieren
conexión serie; si no existe, responden `ESTADO_INVALIDO`. `conectar` responde
`ERROR_CONEXION` si no puede abrir o configurar el puerto. `OK` en comandos al
Arduino se devuelve al recibir la línea de confirmación `OK` del firmware; un
timeout o una respuesta incompleta se informa como `ERROR_INTERNO`. Si el
firmware informa una línea de error, se devuelve `ESTADO_INVALIDO` junto con
el texto recibido.

`mover` recibe X, Y y Z en milímetros. En modo absoluto representan el punto
final; en modo relativo representan desplazamientos desde la posición
consultada al firmware. La velocidad se expresa en mm/s y debe estar entre
5 y 10; si se omite, se usa 5 mm/s. El mínimo coincide con el menor valor
explícito que el firmware ejecuta sin reemplazarlo por su propia velocidad
automática.

La validación de posición replica los límites de `config.h` del firmware:
altura Z entre -115 y 150 mm, brazos de 120 mm, desplazamiento del efector de
50 mm y radios articulares derivados de los límites angulares configurados.
También se comprueba la trayectoria lineal antes de enviarla, para evitar que
el firmware la interrumpa al pasar por una zona no alcanzable. Una posición o
velocidad inválida responde `VALIDACION`.

Todas las operaciones requieren credenciales válidas (`parametros.credenciales`
con `usuario` y `clave` de tipo `string`). El servidor verifica la cuenta y
su estado en cada solicitud y elimina las credenciales antes de delegar el
comando. Solo el rol `ADMINISTRADOR` puede conectar o desconectar; los usuarios
autenticados y habilitados pueden consultar el estado, seleccionar modos y
solicitar operaciones de motores y control manual.

Las operaciones manuales no se ejecutan mientras el modo de trabajo sea
`automatico`. Seleccionar el modo automático no inicia una trayectoria; su
ejecución pertenece a una historia posterior. La ayuda no anuncia comandos que
la implementación no soporte.

`usuario.actual` reutiliza el `Usuario` obtenido al autenticar la solicitud. `rol`
es exactamente `ADMINISTRADOR` u `OPERADOR`; `usuario` es el nombre de esa cuenta.
`OK` confirma la consulta de identidad, no crea una sesión ni garantiza permisos
para pedidos posteriores. No devuelve contraseñas, hashes ni datos de otras cuentas.
Un parámetro adicional (incluso `usuario` o `rol`) produce `VALIDACION` después de
autenticar. Se mantienen `NO_AUTENTICADO` para credenciales ausentes/incorrectas,
`VALIDACION` para credenciales mal formadas, `PERMISO_DENEGADO` para una cuenta con
clave correcta pero deshabilitada y `ERROR_INTERNO` ante un fallo del repositorio.

El menú Python consulta `usuario.actual` antes de abrirse. Con `ADMINISTRADOR`
muestra todas sus opciones actuales; con `OPERADOR` omite conectar, desconectar
y administración de operadores. Si hay un error o falta un rol reconocido no abre
el menú. Los comandos directos siguen sujetos a los controles del servidor.
Las operaciones de usuarios se detallan en `docs/backlog/US-05.md`.

### Tareas: almacenamiento y listado

`tarea.subir` acepta exclusivamente nombre y contenido además de las credenciales.
No acepta propietario, usuario_id, rol ni rutas de escritura. El propietario se
obtiene del Usuario autenticado, incluso cuando sube el administrador. El contenido
viaja como string UTF-8, no base64. El servidor aplica las reglas de almacenamiento
del Hito A: hasta 1 MiB, nombre normalizado a minúsculas de hasta 64 caracteres,
extensión .gcode/.nc, texto compatible con XML y formato básico G/M. Las reglas
completas y sus límites se mantienen en `docs/backlog/US-15.md`.

El campo `resultado.id` es el identificador lógico `ID_propietario:nombre`, por
ejemplo `2:pieza.nc`; no es una ruta y es diferente del id de correlación del sobre.
`propietario` es el nombre de usuario obtenido del servidor. `OK` confirma que se
publicó el archivo completo en el almacenamiento, no que sea ejecutable por el robot.

`tarea.listar` responde, por ejemplo:

```python
{"tareas": [{"id": "2:pieza.nc", "nombre": "pieza.nc", "propietario": "ana"}]}
```

Cada fila contiene exactamente esos tres strings; no incluye contenido, rutas
internas ni credenciales. Sin tareas devuelve `{"tareas": []}` con `OK`. Los
resultados se ordenan por ID numérico de propietario y nombre. Una cuenta
deshabilitada no puede subir ni listar aunque conserve una consola abierta.

Se reutilizan los códigos: `VALIDACION` para parámetros, nombre, tamaño o contenido
inválidos; `ESTADO_INVALIDO` para un duplicado del mismo propietario;
`NO_AUTENTICADO` y `PERMISO_DENEGADO` según las reglas de acceso existentes;
`ERROR_INTERNO` para fallos de almacenamiento o consulta de propietarios. Un ID de
carpeta sin usuario en la base produce `ERROR_INTERNO`; esto no detecta IDs
reutilizados, por lo que base de usuarios y tareas deben respaldarse juntas.
Un error incluye `resultado.mensaje`, sin revelar contenido ni rutas internas.

El directorio es configuración local: `--tareas-dir carpeta`, predeterminado
`data/trayectorias`. Subir y listar se resuelven en ServicioProtegido y no se delegan
a ServicioRobot. No requieren conexión ni un modo del robot. Un proceso escritor
atiende las solicitudes secuencialmente; no se agregan reintentos automáticos.

### Registro e informes: consultas administrativas

`log.consultar` e `informe.usuarios` conservan el sobre de seis campos y requieren
las mismas credenciales por solicitud. El servidor autentica y comprueba el rol
`ADMINISTRADOR` antes de validar filtros o leer el CSV. No requieren conexión al
robot ni un modo de trabajo particular. `usuario` es un filtro, no la identidad
de quien consulta, que se obtiene exclusivamente de las credenciales verificadas.

Todos los parámetros de esta tabla son opcionales y se agregan a `credenciales`:

| Parámetro | Tipo XML-RPC | Predeterminado y reglas | Operaciones |
| --- | --- | --- | --- |
| `usuario` | `string` | `""`: sin filtro. Hasta 64 bytes; coincidencia exacta, distingue mayúsculas. No exige que la cuenta exista actualmente. | Ambas |
| `desde` | `string` | `""`: sin límite inferior. Fecha real `AAAA-MM-DD`, año 0001..9999, día completo en UTC incluido. | Ambas |
| `hasta` | `string` | `""`: sin límite superior. Mismo formato y reglas; si se indican ambas fechas, `desde <= hasta`. | Ambas |
| `codigo` | `string` | `""`: sin filtro. Hasta 64 bytes; coincidencia exacta. No se restringe a una lista de códigos. | Ambas |
| `tipo` | `string` | `""`: ambos tipos. Acepta únicamente `PETICION` o `EVENTO` si no está vacío. | Solo `log.consultar` |
| `offset` | `int` / `i4` | 0; rango 0..100000000. Cantidad de coincidencias que se omiten desde el comienzo. | Solo `log.consultar` |
| `limite` | `int` / `i4` | 50; rango 1..100. Máximo de registros de la página. | Solo `log.consultar` |

Los filtros se combinan por AND. Omitir un filtro de texto equivale a enviar
`""`; no se recortan espacios ni se cambian mayúsculas. Para paginar se requieren
enteros: un `boolean` o `double` no sustituye a un `int`. Un filtro válido sin
coincidencias produce `OK`, no `RECURSO_NO_ENCONTRADO`.

**Resultado de `log.consultar`:** `registros` es un array de structs con exactamente
nueve campos `string`: `fecha_hora`, `tipo`, `id_solicitud`, `usuario`,
`nodo_cliente`, `modulo`, `operacion`, `codigo` y `detalle`. `fecha_hora` usa UTC
con formato `AAAA-MM-DDTHH:MM:SSZ`. Para `PETICION`, `usuario` queda vacío cuando
no se logró autenticar una cuenta habilitada; `nodo_cliente` es el valor fijo
`127.0.0.1` de la escucha local actual. Los eventos no tienen ID de solicitud,
usuario ni nodo: esos campos son strings vacíos. El detalle es genérico y no
contiene parámetros, credenciales, hashes ni contenido de tareas.

Se conserva el orden de las filas del archivo, desde las más antiguas agregadas;
no se ordena por fecha. `total` cuenta todas las coincidencias antes de paginar,
`offset` repite el desplazamiento pedido y `mostrados` es la longitud de
`registros`. Sin coincidencias, o si `offset >= total`, el array está vacío y
`mostrados` vale 0; `total` conserva el total filtrado.

**Resultado de `informe.usuarios`:** `usuarios` es un array de structs con
`usuario: string`, `peticiones: int`, `exitosas: int` y `rechazadas: int`. Solo
cuenta filas `PETICION` con usuario autenticado no vacío, después de aplicar los
filtros; no incluye eventos ni intentos sin identidad verificada. `exitosas`
cuenta `codigo == "OK"`; `rechazadas` cuenta cualquier otro código, incluidos
errores de la operación posteriores a la autenticación. Los totales globales son
`total_peticiones`, `total_ok` y `total_errores`. Las filas se ordenan por nombre
de usuario según `std::string`; solo aparecen usuarios con coincidencias. Sin
coincidencias devuelve `usuarios: []` y los tres totales en 0. No tiene paginación
ni acepta `tipo`, `offset` o `limite`.

Todos los contadores de ambas respuestas son enteros XML-RPC de 32 bits; si
superan 2147483647 se devuelve ese máximo, sin desbordar. Cada consulta lee el
CSV completo. La petición de consulta se registra después de preparar su
respuesta, por lo que no se incluye a sí misma; puede aparecer en una posterior.
No hay una instantánea estable entre páginas sucesivas mientras se agregan filas.

**Errores de ambas consultas:**

| Código | Causa |
| --- | --- |
| `NO_AUTENTICADO` | Credenciales ausentes, usuario inexistente o clave incorrecta. |
| `PERMISO_DENEGADO` | Cuenta deshabilitada con clave correcta, o cuenta autenticada con rol `OPERADOR`. |
| `VALIDACION` | Credenciales mal formadas; parámetro desconocido; tipo, fecha, longitud o rango inválidos. |
| `ERROR_INTERNO` | Fallo al consultar usuarios, registro no configurado o error de lectura/formato del CSV. |

Los errores de aplicación incluyen `resultado.mensaje: string`. Los errores del
sobre conservan el tratamiento XML-RPC Fault de la sección 9. Un fallo al escribir
el registro de la petición se advierte en stderr y no reemplaza la respuesta ya
obtenida ni provoca reintentar la operación.

El informe actual cuenta peticiones acumuladas; no devuelve estado del robot,
inicio de actividad ni detalle de órdenes desde la última conexión. No equivale
al reporte completo exigido por la consigna (página 3). Alcance del CSV, eventos
y limitaciones: [US-18/US-20/US-21](backlog/US-18-20-21.md).

### Informe de actividad propio (US-19)

`informe.propio` conserva los seis campos y la autenticación por solicitud. Está
disponible para `OPERADOR` y `ADMINISTRADOR` habilitados. El usuario del informe
proviene exclusivamente de Autenticador. No acepta `usuario`, `rol`, IDs ni otros
filtros, aunque indiquen la propia cuenta; no modifica los permisos de las
consultas administrativas anteriores.

Solo admite estos parámetros además de `credenciales`:

| Parámetro | Tipo XML-RPC | Predeterminado | Rango |
| --- | --- | --- | --- |
| `offset` | `int` / `i4` | 0 | 0..100000000 |
| `limite` | `int` / `i4` | 50 | 1..100 |

Booleanos, texto y números decimales no sustituyen a esos enteros. El resultado
con `OK` contiene exactamente:

| Campo | Tipo | Significado |
| --- | --- | --- |
| `usuario` | `string` | Nombre de la identidad autenticada. |
| `desde` | `string` | Fecha UTC `AAAA-MM-DDTHH:MM:SSZ` del evento de referencia; vacía si no existe. |
| `referencia` | `string` | `conectar` para la última conexión serie exitosa; `inicio` si se inició el servidor y aún no hubo conexión; vacío si faltan eventos. |
| `inicio_actividad` | `string` | Fecha UTC de la primera petición propia del periodo, o `""` si no hay. |
| `estado` | `struct` | Datos de la operación `estado` existente, con sus indicadores de disponibilidad y modos; sin `reporte_firmware`. |
| `codigo_estado` | `string` | Código de esa consulta interna de estado; permite informar un error parcial sin perder el detalle propio. |
| `registros` | `array` de `struct` | Las mismas nueve columnas string de `log.consultar`, únicamente filas `PETICION` de la identidad autenticada. |
| `total` | `int` / `i4` | Cantidad de peticiones propias del periodo entero, antes de paginar. |
| `errores` | `int` / `i4` | Cantidad de esas peticiones con código distinto de `OK`, antes de paginar. |
| `offset` | `int` / `i4` | Desplazamiento solicitado. |
| `mostrados` | `int` / `i4` | Longitud de `registros`. |

El periodo comienza después del evento de conexión exitosa más reciente que
abre una conexión, o del inicio del servidor si es posterior. Se reconocen los
eventos por orden de archivo, incluso cuando tienen la misma marca de segundo.
Un conectar repetido estando conectado o rechazado no reinicia el periodo;
desconectar tampoco lo borra. Un nuevo inicio invalida la conexión del proceso
anterior. Si no hay referencia, se usa lo disponible del CSV con fecha y
referencia vacías. La petición `conectar` que precede al evento de apertura queda
fuera del periodo nuevo.

Se incluyen consultas y órdenes rechazadas después de autenticar; nunca se
atribuyen intentos sin identidad verificada a un nombre declarado. El inicio y
los totales no dependen de la página. Sin actividad: `registros: []`, `total: 0`,
`errores: 0`, `mostrados: 0`, `inicio_actividad: ""`. Con `offset >= total`, la
página queda vacía, pero conserva los totales. Se mantiene el orden de archivo,
la saturación int32 y la escritura posterior a la respuesta: la consulta actual
no aparece en su propio resultado. No hay instantánea entre páginas.

El estado se obtiene una vez, sin otra petición de aplicación registrada. No se
requiere conexión serie para consultar. `OK` exterior confirma el informe;
si falla la lectura serie se conserva el estado parcial con
`codigo_estado: ERROR_INTERNO`, mensaje genérico y posición no disponible.
No implica ejecución física ni contiene parámetros, credenciales o contenido.

Errores: `NO_AUTENTICADO` para credenciales ausentes/incorrectas;
`PERMISO_DENEGADO` para cuenta deshabilitada con clave correcta; `VALIDACION`
para credenciales mal formadas, campos desconocidos o paginación inválida;
`ERROR_INTERNO` para fallo de usuarios, registro no configurado o fallo al
construir el informe. Incluyen `resultado.mensaje` sin rutas internas.
Los Fault del sobre mantienen su tratamiento. Ver [US-19](backlog/US-19.md)
para el fundamento del periodo, recorrido, pruebas y límites.

### Control serie existente

El firmware espera `\r` como terminador de comando y responde con líneas de
texto terminadas en `\r\n`. El servidor elimina los saltos de línea de la
respuesta agregada. La conexión espera el reinicio del Arduino y descarta el
contenido inicial del puerto antes de enviar comandos.

El servidor sigue escuchando únicamente en `127.0.0.1`. Los roles provienen
del repositorio SQLite; el cliente no puede otorgarse permisos declarando un
rol en los parámetros. La clave viaja por HTTP sin cifrado, por lo que no se
debe exponer el servicio a una red no confiable.

## 11. Responsabilidades de cada componente

**Cliente Python:** prepara los mensajes, solicita una operación mediante XML-RPC, recibe la respuesta y presenta el resultado al usuario. No controla el Arduino directamente.

**Servidor C++:** recibe y valida los mensajes con la biblioteca XML-RPC, identifica la operación, comprueba parámetros, ejecuta la lógica correspondiente y construye la respuesta. La clase `Robot` mantiene la conexión serie y traduce las operaciones de dominio a G-code.

**Arduino:** utiliza el firmware proporcionado para interpretar las órdenes serie que recibe.

## 12. Seguridad y registro

- Se validan los tipos, rangos y permisos en el servidor, aunque el cliente haya realizado comprobaciones previas.
- Las credenciales se incluyen como parametros.credenciales, estructura con usuario y clave de tipo string. Se validan en cada peticion de aplicacion. No deben registrarse en logs ni devolverse en respuestas.
- Los errores no deben exponer contraseñas ni información interna innecesaria.
- El uso inicial de HTTP/XML-RPC en un entorno de pruebas local no debe interpretarse como protección suficiente para una red no confiable.

## 13. Compatibilidad y mantenimiento

El cliente y el servidor deben respetar los mismos nombres, tipos y significados. Si se modifica la estructura general o la definición de una operación, se actualizarán este documento y las implementaciones y pruebas correspondientes de ambos lados.

La biblioteca XML-RPC se reutilizará para transportar y representar los valores; no se implementará manualmente un parser XML adicional para estos mensajes.

## Autenticacion incorporada en US-03

Todas las operaciones de aplicacion requieren `parametros.credenciales` con
exactamente `usuario` y `clave`, ambos `string`. La omision de credenciales
devuelve `NO_AUTENTICADO`, su formato incorrecto `VALIDACION`, una clave
incorrecta o usuario inexistente `NO_AUTENTICADO` y una cuenta deshabilitada
con clave correcta `PERMISO_DENEGADO`. No se agregan sesiones. Los `system.*`
de XmlRpc++ son metadatos y no ofrecen acceso directo a los servicios de
aplicacion.

Ver docs/backlog/US-03.md para decisiones, recorrido y pruebas.

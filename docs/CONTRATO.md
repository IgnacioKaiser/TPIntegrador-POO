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
        "x": 120.5,
        "y": 80.0,
        "velocidad": 50
    },
    "resultado": {},
    "codigo": ""
}
```

**Nota:** `mover` es un ejemplo de formato. Sus parámetros definitivos y sus rangos deberán ajustarse a las unidades, capacidades y requisitos del robot antes de implementar esa operación.

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
de motores y `ayuda` no aceptan parámetros adicionales.

| Operación | Parámetros | Resultado en `OK` | Comportamiento |
| --- | --- | --- | --- |
| `ping` | Ninguno | `mensaje: string` | Prueba del servidor, no requiere conexión al robot. |
| `eco` | `texto: string` | `texto: string` | Devuelve el texto recibido. |
| `usuario.actual` | Ninguno | `usuario: string`, `rol: string` | Devuelve únicamente la identidad autenticada de quien realiza el pedido. |
| `usuario.habilitar` | `usuario: string` | `usuario: string`, `habilitado: boolean`, `mensaje: string` | Habilita un operador; solo administrador. |
| `usuario.deshabilitar` | `usuario: string` | `usuario: string`, `habilitado: boolean`, `mensaje: string` | Deshabilita un operador; solo administrador. |
| `conectar` | `puerto: string` opcional | `mensaje: string` | Abre el puerto a 115200 baudios, 8N1. Por defecto usa `/dev/ttyACM0` en Linux y `COM3` en Windows. |
| `desconectar` | Ninguno | `mensaje: string` | Cierra la conexión si está abierta; también es válida si ya estaba desconectado. |
| `estado` | Ninguno | `conectado: boolean`, `posicion: struct`, `actividad: struct` | Devuelve el estado de conexión. Si está conectado, consulta `M114`; cada dato no informado se marca `disponible: false` con un `mensaje` explicativo. |
| `activar_motores` | Ninguno | `respuesta: string` | Envía `M17`. |
| `desactivar_motores` | Ninguno | `respuesta: string` | Envía `M18`. |
| `ayuda` | Ninguno | `comandos: string` | Informa las operaciones disponibles. |

`posición` informa `disponible`, `mensaje` cuando no está disponible, y
coordenadas `x`, `y`, `z`, `e` en milímetros cuando el firmware devuelve el
formato `CURRENT POSITION: [X:... Y:... Z:... E:...]`. `actividad` contiene
`motores`, `ventilador` y `modo_coordenadas`; cada dato declara si está
disponible, y solo incluye `activo` o `valor` cuando se pudo leer.

`estado` se puede consultar desconectado: responde `conectado: false` y marca
posición y actividad como no disponibles, en lugar de simular lecturas.
`activar_motores` y `desactivar_motores` requieren conexión serie; si no
existe, responden `ESTADO_INVALIDO`. `conectar` responde `ERROR_CONEXION` si
no puede abrir o configurar el puerto. `OK` en comandos al Arduino se devuelve
al recibir la línea de confirmación `OK` del firmware; un timeout o una
respuesta incompleta se informa como `ERROR_INTERNO`.

Todas las operaciones requieren credenciales válidas (`parametros.credenciales`
con `usuario` y `clave` de tipo `string`). El servidor verifica la cuenta y
su estado en cada solicitud y elimina las credenciales antes de delegar el
comando. Solo el rol `ADMINISTRADOR` puede conectar o desconectar; los usuarios
autenticados y habilitados pueden consultar el estado y solicitar operaciones
de motores.

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

No se expone movimiento G-code desde este conjunto de operaciones. La ayuda
no anuncia comandos que la interfaz no implemente; tampoco se definen límites
de coordenadas.

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

Todas las operaciones de aplicacion de la seccion 10 requieren parametros.credenciales con exactamente usuario y clave, ambos string. La omision de credenciales devuelve NO_AUTENTICADO, su formato incorrecto VALIDACION, una clave incorrecta o usuario inexistente NO_AUTENTICADO y una cuenta deshabilitada con clave correcta PERMISO_DENEGADO. No se agregan sesiones. Los system.* de XmlRpc++ son metadatos y no ofrecen acceso directo a los servicios de aplicacion.

Ver docs/backlog/US-03.md para decisiones, recorrido y pruebas.

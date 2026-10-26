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

**En C++**, la clase `XmlRpc::XmlRpcValue` de la biblioteca XML-RPC existente representará los valores y las estructuras del mensaje. **En Python**, se utilizarán los tipos nativos compatibles con XML-RPC.

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
| `NO_AUTENTICADO` | La operación requiere una sesión o autenticación válida. |
| `PERMISO_DENEGADO` | El usuario no tiene autorización para la operación. |
| `OPERACION_NO_SOPORTADA` | El servidor no reconoce la operación solicitada. |
| `ESTADO_INVALIDO` | La operación no puede ejecutarse en el estado actual del sistema. |
| `RECURSO_NO_ENCONTRADO` | No existe el recurso solicitado. |
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

## 10. Definición de las operaciones

El esquema general no determina automáticamente las operaciones disponibles. Para cada operación implementada se documentarán:

1. Su nombre exacto.
2. Los parámetros obligatorios y opcionales, con nombre, tipo, unidad y restricciones.
3. Los campos devueltos en `resultado` y sus tipos.
4. Los permisos necesarios.
5. El significado de `OK`: recepción, aceptación, ejecución o finalización.
6. Los errores que puede devolver.

Las operaciones de prueba existentes, como `ping` y `eco`, pueden adaptarse a este esquema. Las operaciones de movimiento, consulta, autenticación y administración se incorporarán cuando se definan sus requisitos específicos.

## 11. Responsabilidades de cada componente

**Cliente Python:** prepara los mensajes, solicita una operación mediante XML-RPC, recibe la respuesta y presenta el resultado al usuario. No genera G-Code ni controla el Arduino directamente.

**Servidor C++:** recibe y valida los mensajes con la biblioteca XML-RPC, identifica la operación, comprueba condiciones y permisos, ejecuta la lógica correspondiente y construye la respuesta. La conversión de operaciones de movimiento a G-Code y el envío al Arduino corresponden al lado servidor, en sus componentes específicos.

**Arduino:** utiliza el firmware proporcionado para interpretar las órdenes que recibe. Este contrato describe la comunicación entre el cliente y el servidor, no el formato de comunicación serial con Arduino.

## 12. Seguridad y registro

- Se validan los tipos, rangos y permisos en el servidor, aunque el cliente haya realizado comprobaciones previas.
- Si se incorporan credenciales o tokens, deberán definirse como parámetros con nombre y tipo; no deberán registrarse sus valores en logs.
- Los errores no deben exponer contraseñas ni información interna innecesaria.
- El uso inicial de HTTP/XML-RPC en un entorno de pruebas local no debe interpretarse como protección suficiente para una red no confiable.

## 13. Compatibilidad y mantenimiento

El cliente y el servidor deben respetar los mismos nombres, tipos y significados. Si se modifica la estructura general o la definición de una operación, se actualizarán este documento y las implementaciones y pruebas correspondientes de ambos lados.

La biblioteca XML-RPC se reutilizará para transportar y representar los valores; no se implementará manualmente un parser XML adicional para estos mensajes.

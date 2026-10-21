# Contrato de comunicación — versión 1, aplicación v0.2

## Transporte

- XML-RPC por HTTP, ruta utilizada `/RPC2`. XmlRpc++ no restringe por sí misma la ruta HTTP.
- Dirección predeterminada: `http://127.0.0.1:8080/RPC2`.
- Método de aplicación: `robot.intercambiar`.
- Entrada: exactamente un parámetro XML-RPC de tipo `struct`.
- Salida normal: un `struct` con el mismo esquema lógico.
- La biblioteca puede exponer además sus métodos de introspección; no constituyen servicios del robot.

## MensajeRPC

Se requieren exactamente seis campos. Todos son `string` XML-RPC en esta primera versión.

| Campo | Solicitud | Respuesta |
| --- | --- | --- |
| `version` | `1` | `1` |
| `id` | Identificador de 1–64 caracteres ASCII: letras, dígitos, guion o guion bajo. | Mismo id. |
| `tipo` | `solicitud` | `respuesta` |
| `operacion` | Identificador de 1–32 caracteres con el mismo alfabeto que id. | Misma operación. |
| `contenido` | Texto, incluso vacío, hasta 8192 bytes UTF-8. | Resultado textual. |
| `codigo` | Cadena vacía. | Identificador no vacío, hasta 64 caracteres. |

El contenido admite caracteres XML 1.0 del plano Unicode básico (BMP). Se conserva esta restricción del contrato anterior para mantener compatibilidad. Se rechazan controles no admitidos por XML y caracteres fuera del BMP; no se hace una sustitución silenciosa. Los finales de línea están sujetos a normalización XML. Los ids generados por Python son UUID sin guiones; el servidor no exige ese formato particular.

No hay JSON contenido dentro de strings XML, punteros, memoria de objetos, pickle ni código ejecutable. Cada extremo tiene su propia clase y comparte el esquema.

## Operaciones

| Operación | Contenido de solicitud | Resultado |
| --- | --- | --- |
| `ping` | Puede estar vacío; se ignora. | Código `OK`, contenido `pong: servidor C++ disponible`. |
| `eco` | Texto. | Código `OK`, contenido recibido. |
| Otra operación sintácticamente válida | Cualquier texto permitido. | Código `OPERACION_NO_SOPORTADA`. |

## Errores y correlación

- Un mensaje que no cumple el contrato produce un XML-RPC Fault. No se inventa una respuesta normal si no se puede reconstruir una solicitud válida.
- Una operación desconocida con mensaje válido produce una respuesta normal con código de aplicación de error y mantiene el id.
- Un fallo de red, HTTP, parseo o timeout se traduce a `ErrorComunicacion` en el cliente.
- El cliente comprueba que versión, id, operación y tipo de respuesta sean válidos antes de mostrarla.
- No se implementan reintentos automáticos ni deduplicación persistente de ids. El id es correlación, no una garantía de ejecución exactamente una vez.

## Evolución

Este contrato pequeño permite empezar. Antes de autenticar o controlar el robot, se acordarán credenciales, parámetros tipados y resultados estructurados. Ese cambio de esquema requiere una nueva versión del contrato y actualización coordinada de ambos extremos; no agregar campos unilateralmente a versión 1.

Las nuevas operaciones se implementan detrás de la capa de servicios. Las CLI no deben construir XML ni abrir el puerto serie.

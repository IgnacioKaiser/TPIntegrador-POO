# Migración v0.1 a v0.2

| Componente | Cambio |
| --- | --- |
| Transporte servidor | De xmlrpc-c/Abyss a XmlRpc++ original del anexo. |
| Método remoto | Conserva robot.intercambiar y el contrato versión 1. |
| Modelo/servicio | Se mantienen separados de la biblioteca de comunicación. |
| Cliente Python | Conserva xmlrpc.client estándar, CLI, validaciones y timeout. |
| Compilación | Fuentes locales y bibliotecas estáticas; elimina xmlrpc-c-config. |
| Material de TP2 | Logger, File, LogRecord y excepciones incluidos; logger compila, uso pendiente. |
| Simulador | Incluido como recurso original; integración serie pendiente. |

La herencia de XmlRpcServerMethod implementa el contrato exigido por la biblioteca. ServidorLocal hereda de XmlRpcServer para enlazar únicamente a loopback sin modificar los archivos del anexo. Se emplea el manejador de conexiones y HTTP de XmlRpc++, no un servidor HTTP propio.

El método se destruye antes que el servidor al que está registrado. El proceso atiende SIGINT/SIGTERM y cierra conexiones; ignora SIGPIPE para soportar clientes desconectados. Los mensajes inválidos generan Fault -32602; los errores de aplicación conservan su código dentro de MensajeRPC.

Pendiente antes de usar el logger en el integrador: adaptar campos a los eventos requeridos, escape CSV y sincronización de escrituras. Sus clases se incluyen tal como fueron entregadas, sin declarar cumplido todavía el requisito de logging.

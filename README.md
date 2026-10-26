# TP integrador — comunicación v0.2 reorganizada

Mismo funcionamiento de robot_rpc_v0_2, organizado según el acuerdo del grupo. Servidor C++ con XmlRpc++ del profesor, cliente Python por consola y firmware original completo. No se incorporan nuevas funciones de robot.

## Organización

- `server/xmlrpc/`: biblioteca XmlRpc++ original, licencia y ejemplos, separados del código propio.
- `server/include/robotrpc/` y `server/src/`: clases de mensajes, servicio y servidor.
- `server/include/tp2_logger/` y `server/src/tp2_logger/`: clases del TP anterior, con adaptacion de fecha/hora para Windows. Se compilan pero todavía no se utilizan para registrar eventos.
- `server/examples/tp2_logger/`: main y datos de ejemplo originales del logger.
- `server/tests/`: pruebas C++ y de integración cliente-servidor.
- `server/Makefile`: compilación del servidor, bibliotecas y pruebas.
- `client/main.py`: entrada alternativa al cliente.
- `client/robot_client/cli.py`: interfaz de consola.
- `client/robot_client/cliente.py`: comunicación XML-RPC y servicios.
- `client/robot_client/mensaje.py`: objetos de mensaje y validación.
- `client/tests/`: pruebas unitarias Python.
- `firmware/arduino/robotArm_v0.62sim/`: carpeta completa original del firmware, con sus nombres y contenidos intactos.
- `data/db/`, `data/logs/`, `data/trayectorias/`: carpetas reservadas, sin datos iniciales; incluyen .gitkeep para conservarlas en Git.
- `docs/`: contrato, procedencia, migración, reorganización y verificación.
- `.vscode/`: tareas de compilación y pruebas e includes para VS Code.

Los nombres de módulos Python se conservan porque la propuesta del grupo permite ajustarlos y ya separan interfaz, comunicación y modelos. No hay copias duplicadas con nombres alternativos.

## Ejecutar en Windows (PowerShell)

Requiere MinGW-w64 de MSYS2 UCRT64 (g++, ar y mingw32-make en PATH), y Python 3.10 o posterior. No requiere CMake ni paquetes pip.

Desde la raiz:

```powershell
mingw32-make -C server
.\server\build\robot_server.exe
```

En otra terminal, desde la raiz:

```powershell
python client/main.py ping
python client/main.py eco "Hola desde Windows"
```

Sin argumentos, `python client/main.py` abre el menu. Cerrar el servidor con Ctrl+C.

Pruebas automaticas (inician y terminan su propio servidor):

```powershell
mingw32-make -C server test
```

Ver `docs/COMPATIBILIDAD_WINDOWS.md` para alcance y cambios de portabilidad.

## Ejecutar en Linux

Descomprimir y abrir `TP_integrador_real` en VS Code. Detener con Ctrl+C cualquier servidor anterior que use el puerto 8080.

Se requieren build-essential, Python 3.10 o posterior, libsqlite3-dev y libsodium-dev. No se utiliza libxmlrpc-c++8-dev ni se requieren paquetes pip.

Terminal 1, desde la raíz del proyecto:

```bash
cd server
make
./build/robot_server
```

Terminal 2, desde la raíz del proyecto:

```bash
cd client
python3 main.py ping
python3 main.py eco "Hola desde Python"
```

También funcionan los comandos anteriores `python3 -m robot_client ping` y `python3 -m robot_client eco "Hola desde Python"`, ejecutados desde client. Sin argumentos se abre el menú interactivo.

Resultado de ping: `[OK] pong: servidor C++ disponible` y el ID de la solicitud.

Otro puerto: en server, `./build/robot_server --port 8081`; en client, `python3 main.py --url http://127.0.0.1:8081/RPC2 ping`.

## Pruebas y compilación

Desde la raíz:

```bash
make -C server test
```

También se puede ejecutar `make test` estando dentro de server. `make -C server examples` compila los tres ejemplos originales de XmlRpc++ como ejecutables separados. `make -C server clean` elimina server/build. Ctrl+Shift+B en VS Code compila desde server.

## Alcance conservado

El cliente crea un MensajeRPC y lo serializa. XmlRpc++ recibe robot.intercambiar; el adaptador reconstruye el objeto C++, el servicio procesa ping/eco y devuelve la respuesta con el mismo ID. El dominio no depende de XmlRpc++. Ver docs/CONTRATO.md.

Se conserva la escucha en 127.0.0.1, el timeout del cliente, la validación de mensajes y la atención secuencial de métodos sobre conexiones multiplexadas. El servidor RPC aun no integra autenticacion, puerto serie ni ejecucion de G-Code. SQLite se utiliza por ahora en la herramienta local de US-04. Las carpetas data son una reserva de organización: no cambian las rutas internas del logger original, que aún no está activo.

El firmware se conserva como recurso y no se compila con el Makefile del servidor. Los warnings de parámetros no utilizados provienen del código original de XmlRpc++.

## US-04: usuarios persistentes

Se agrega el modulo de usuarios SQLite y la herramienta local usuarios_admin. Requieren SQLite y libsodium para compilar. Ver [US-04](docs/backlog/US-04.md) para instalacion, comandos, decisiones y pruebas. La autenticacion de peticiones RPC aun no esta integrada; corresponde al siguiente hito US-03.

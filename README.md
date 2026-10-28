# TP integrador — comunicación y control serie del robot

Servidor C++ con XmlRpc++ del profesor, cliente Python por consola y firmware original completo. El servidor permite conectar con el Arduino por puerto serie y ejecutar las operaciones de control documentadas en `docs/CONTRATO.md`.

## Organización

- `server/xmlrpc/`: biblioteca XmlRpc++ original, licencia y ejemplos, separados del código propio.
- `server/include/robotrpc/` y `server/src/`: clases de mensajes, servicio y servidor.
- `server/include/tp2_logger/` y `server/src/tp2_logger/`: logger del TP anterior, conservado y compilado como biblioteca. El CSV de actividad usa `RegistroActividadCsv`, en `server/src/registro_actividad.cpp`.
- `server/include/domain/Robot.h` y `server/src/domain/Robot.cpp`: conexión serie multiplataforma y comandos enviados al Arduino.
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

Sin argumentos, `python client/main.py` consulta `usuario.actual` y abre el menu
segun el rol autenticado. Conectar, desconectar y administrar operadores solo se
muestran al administrador; los modos, origen, movimiento y efector manual estan
disponibles en el menu para ambos roles. Todos los pedidos siguen verificandose en
el servidor. Un rechazo o un rol desconocido impide abrir el menu. Los comandos
directos conservan los controles del servidor. Ver [US-23](docs/backlog/US-23.md)
para la identidad por rol y las operaciones del cliente.
Cerrar el servidor con Ctrl+C.

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

US-15/US-16/US-24 permiten subir y listar tareas por RPC y desde Python. Las pruebas
solo del repositorio se ejecutan con `mingw32-make -C server test-tareas` en Windows
o `make -C server test-tareas` en Linux. La suite completa incluye el recorrido
Python → servidor C++ → archivos temporales. Ver [US-15](docs/backlog/US-15.md).
La base de usuarios y las carpetas de tareas por ID deben conservarse juntas para
mantener sus propietarios; no reinicializar la base dejando las tareas anteriores.

El servidor acepta `--tareas-dir carpeta`; su valor predeterminado es
`data/trayectorias`. Los comandos del cliente son:

```powershell
python client/main.py subir "C:\ruta local\pieza.gcode"
python client/main.py tareas
```

En Linux usar python3 y una ruta Linux. El menu ofrece las mismas operaciones en
17/18 para ambos perfiles. Operador ve sus tareas; administrador ve todas. Subir
no ejecuta ordenes. Para una prueba aislada con dos operadores y un administrador,
sin usar la base habitual, ver [US-24](docs/backlog/US-24.md).

También se puede ejecutar `make test` estando dentro de server. `make -C server examples` compila los tres ejemplos originales de XmlRpc++ como ejecutables separados. `make -C server clean` elimina server/build. Ctrl+Shift+B en VS Code compila desde server.

## Alcance conservado

El cliente crea un MensajeRPC y lo serializa. XmlRpc++ recibe robot.intercambiar; el adaptador reconstruye el objeto C++, y el servicio conserva el ID de la solicitud en su respuesta. `ServicioRobot` delega las operaciones serie a una instancia de `Robot` compartida por el servidor. Ver docs/CONTRATO.md.

Se conserva la escucha en 127.0.0.1, el timeout del cliente, la validación de mensajes y la atención secuencial de métodos sobre conexiones multiplexadas. Las solicitudes requieren credenciales verificadas contra SQLite. Solo administradores pueden conectar o desconectar; los operadores autenticados pueden consultar el estado, seleccionar modos y controlar motores, origen, movimiento cartesiano y efector. Origen, movimiento y efector requieren modo manual y conexión. `estado` informa los modos seleccionados y marca expresamente como no disponibles los datos que el firmware no reporta. Ver `docs/CONTRATO.md` y `docs/backlog/US-10.md` a `US-13.md` para el contrato y las pruebas. El directorio de tareas ya almacena archivos; el registro CSV y las consultas administrativas están implementados en la rama de revisión. Su alcance y pendientes están en [US-18/US-20/US-21](docs/backlog/US-18-20-21.md).

Comandos de control por consola (requieren usuario y clave al iniciar):

```powershell
python client/main.py modo-trabajo manual
python client/main.py modo-coordenadas absoluto
python client/main.py origen
python client/main.py mover 0 170 120 --velocidad 5
python client/main.py activar-efector
python client/main.py desactivar-efector
python client/main.py estado
```

El firmware se conserva como recurso y no se compila con el Makefile del servidor. Los warnings de parámetros no utilizados provienen del código original de XmlRpc++.

## US-04: usuarios persistentes

Se agrega el modulo de usuarios SQLite y la herramienta local usuarios_admin. Requieren SQLite y libsodium para compilar. Ver [US-04](docs/backlog/US-04.md) para instalacion, comandos, decisiones y pruebas. La autenticacion de peticiones se integra en US-03; ver su documentacion.

## US-03: autenticacion de solicitudes

El servidor utiliza la base preparada en US-04 y exige un administrador inicial. Se conserva esa base: no reinicializarla. Ejecutar desde la raiz `server/build/robot_server` (Windows: `.\server\build\robot_server.exe`). Se puede indicar `--db ruta` para elegir otra base. Cada ejecucion del cliente pide usuario y clave; ping y eco ya requieren autenticacion. Ver [US-03](docs/backlog/US-03.md) para cambios, explicaciones y pruebas.

## Registro CSV e informes (US-18/US-20/US-21)

El servidor agrega peticiones y eventos a `data/logs/actividad.csv`, relativo al
**directorio desde donde se inicia el servidor**. Desde la raíz se guarda en esa
carpeta del repositorio; si se inicia desde `server`, queda bajo `server/data/logs`.
El CSV es acumulativo: reiniciar el servidor conserva las filas anteriores. No hay
rotación ni límite automático de tamaño. Si la ruta de `--db` es distinta de
`data/db/usuarios.sqlite`, el CSV se guarda como `actividad.csv` junto a esa base.
No se incluyen datos reales de actividad en Git.

Ejemplos desde la raíz, con el servidor iniciado:

```powershell
python client/main.py informe-usuarios
python client/main.py informe-usuarios --usuario operador --desde 2026-10-01 --hasta 2026-10-31
python client/main.py log --tipo PETICION --codigo NO_AUTENTICADO --limite 20 --offset 0
python client/main.py log --tipo EVENTO --desde 2026-10-01 --hasta 2026-10-31
```

En Linux usar `python3`. Cada comando pide usuario y clave. Ambas consultas
requieren una cuenta habilitada con rol `ADMINISTRADOR`; el servidor comprueba
ese permiso en cada pedido. El menú ofrece las mismas consultas en 19/20 solo
al administrador. Las fechas son días completos en UTC, con ambos extremos
incluidos. Los filtros se combinan; `log` devuelve por defecto hasta 50 filas
(máximo 100), en orden de archivo. Para la página siguiente se aumenta `--offset`.

El informe resume peticiones por identidad autenticada y cuenta respuestas `OK`
y errores; no incluye intentos sin identidad verificada ni reemplaza el reporte
de actividad desde la última conexión. El log permite consultar también esos
intentos fallidos. No se guardan claves, hashes, parámetros ni contenido G-Code.
Ver [contrato](docs/CONTRATO.md#registro-e-informes-consultas-administrativas)
para tipos, filtros, resultados y errores.

## Mi actividad (US-19)

Ambos perfiles pueden consultar su actividad con el comando `actividad` o la
opción **21** del menú:

```powershell
python client/main.py actividad
python client/main.py actividad --offset 50 --limite 50
```

Muestra el estado actual del robot, la referencia del periodo, el inicio de la
actividad propia y el detalle de peticiones con cantidades y errores. Usa la última
conexión serie exitosa; si aún no hubo una desde el arranque, lo indica y usa el
inicio del servidor. Repetir conectar estando conectado no reinicia el periodo.
No incluye pedidos de otras cuentas ni la consulta que está respondiendo. Sin
actividad devuelve cero; sin robot muestra los datos que no están disponibles.
En Linux usar `python3`. Cada pedido verifica nuevamente usuario, clave y
habilitación. `log` e `informe-usuarios` siguen siendo administrativos.
Ver [US-19](docs/backlog/US-19.md) para probar con dos usuarios y datos temporales.

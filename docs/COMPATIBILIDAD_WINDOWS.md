# Compatibilidad con Windows y Linux

## Alcance

Adaptacion sobre master 8bb7719. Mantiene ping, eco, el contrato de seis campos y la escucha local en 127.0.0.1. No agrega autenticacion, control serie ni funciones del robot.

## Herramientas en Windows

- MinGW-w64 de MSYS2 UCRT64: g++, ar y mingw32-make en PATH.
- Python 3.10 o posterior; sin dependencias pip.
- PowerShell para los comandos del usuario. El Makefile usa cmd.exe para sus recetas en Windows.
- No requiere CMake.

Desde la raiz:

```powershell
mingw32-make -C server clean
mingw32-make -C server
mingw32-make -C server test
```

Prueba manual, terminal 1:

```powershell
.\server\build\robot_server.exe
```

Terminal 2:

```powershell
python client/main.py ping
python client/main.py eco "Hola desde Windows"
```

Cerrar con Ctrl+C en la terminal del servidor. Las pruebas automaticas levantan su propio proceso y lo terminan al finalizar; en Windows terminate() finaliza el proceso directamente. Por eso el cierre ordenado con Ctrl+C se verifica manualmente.

Para compilar los ejemplos de la biblioteca: `mingw32-make -C server examples`.

En VS Code, la tarea Compilar selecciona mingw32-make en Windows. Si IntelliSense muestra cabeceras incorrectas, elegir la configuracion Windows con C/C++: Select a Configuration. La ruta de compilador de esa configuracion corresponde a C:/msys64/ucrt64/bin/g++.exe; ajustarla si se usa otra instalacion.

## Cambios de codigo

- Makefile: elige comandos de creacion/borrado de carpetas, Python, extension .exe, entorno de pruebas y enlace con ws2_32 segun el sistema. Usa _WINDOWS porque la biblioteca original consulta esa macro; _WIN32 la define el compilador.
- servidor.cpp: incluye Winsock en Windows; mantiene arpa/inet.h en Linux. Configura un puerto exclusivo en Windows y usa signal para SIGINT/SIGTERM. Solo Linux ignora SIGPIPE.
- Logger_396.cpp: usa localtime_s en Windows y localtime_r en Linux, comprobando el resultado.
- XmlRpc++: usa SocketHandle (uintptr_t en Windows e int en Linux) e InvalidSocket. Propaga el tipo por las conexiones y el dispatcher para evitar truncar handles en 64 bits. Adapta los formatos de diagnostico. Winsock ignora el primer parametro de select.
- XmlRpcSocket.cpp: distingue errores transitorios de Winsock de los de POSIX. Elimina un using namespace std anterior a las cabeceras de Windows que causaba ambiguedad con std::byte en C++17.
- test_integration.py: selecciona explicitamente robot_server.exe en Windows.
- Configuracion VS Code y documentos: reflejan los comandos y las adaptaciones de las fuentes importadas.

La logica de mensajes y servicios permanece igual. La biblioteca sigue conservando su licencia; ya no es una copia byte a byte de la importacion inicial.

## Verificacion realizada el 26 de octubre de 2026

- Linux: compilacion del servidor, logger y ejemplos; 15 comprobaciones C++, 10 pruebas Python y 16 de integracion correctas.
- Compilacion cruzada con MinGW-w64 GCC 13 desde Linux: genera PE x86-64 para el servidor, test_domain y tres ejemplos; compila tambien el logger.
- No se ejecutaron binarios Windows en ese entorno. Sigue pendiente verificar compilacion nativa con MSYS2 UCRT64 GCC 15.2, recetas de cmd.exe, pruebas y Ctrl+C en la PC del equipo.

No se verifico todavia comunicacion con Arduino ni entre dos computadoras.

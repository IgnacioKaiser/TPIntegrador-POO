# Servidor inicial del Trabajo Practico Integrador

Esta base sigue la separacion usada en C4:

- `src/server/main.cpp`: punto de entrada y reporte de errores.
- `ServerController`: valida el puerto, configura el servidor e inicia el ciclo RPC.
- `ServerModel`: registra las operaciones de ejemplo `ServerTest` y `Eco`.
- `lib/xmlrpc`: implementacion XML-RPC sobre TCP usada por el servidor.

Las operaciones son un punto de partida para probar la comunicacion; deben
reemplazarse o ampliarse con las operaciones que defina la consigna especifica.
La introspeccion XML-RPC esta activada para consultar los metodos publicados.

## Compilar

Desde esta carpeta:

```text
cmake -S . -B build
cmake --build build
```

## Ejecutar

```text
build\Debug\servidor.exe 8080
```

En generadores de configuracion unica el ejecutable suele quedar en
`build\servidor.exe`. El puerto debe ser un entero entre 1 y 65535.

El servidor usa el comportamiento de `XmlRpcServer::bindAndListen` de C4 y
escucha en las interfaces disponibles. No publicar este servidor en una red no
confiable sin agregar autenticacion y controles de acceso.

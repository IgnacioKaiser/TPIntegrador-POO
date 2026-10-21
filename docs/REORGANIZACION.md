# Reorganización de v0.2

Se conserva la lógica de aplicación y la versión 1 del contrato. Solo se ajustan ubicación, rutas de compilación, rutas de pruebas, configuración de VS Code y documentación. Se agrega una entrada client/main.py que delega en la misma CLI.

| Ubicación anterior | Ubicación actual |
| --- | --- |
| lib/xmlrpcpp/ | server/xmlrpc/ |
| lib/tp2_logger/include/ | server/include/tp2_logger/ |
| lib/tp2_logger/src/ | server/src/tp2_logger/ |
| lib/tp2_logger/examples/ | server/examples/tp2_logger/ |
| Makefile | server/Makefile |
| tests/ | server/tests/ |
| resources/arduino/ | firmware/arduino/ |
| shared/CONTRATO.md | docs/CONTRATO.md |
| build/ generado | server/build/ generado |

Se crean data/db, data/logs y data/trayectorias con .gitkeep. Las pruebas Python unitarias se mantienen en client/tests. El cliente mantiene su paquete robot_client; no se renombraron sus módulos para evitar cambios innecesarios en imports y comandos.

Para compilar desde la raíz ahora se usa make -C server. El ejecutable está en server/build/robot_server. Los ejemplos del logger conservan sus rutas relativas originales y no escriben automáticamente en data/logs; integrarlos no forma parte de esta reorganización.

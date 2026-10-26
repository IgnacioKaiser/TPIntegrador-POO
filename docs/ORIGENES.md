# Procedencia de los materiales

- `poo_2025-tp2-req4-anexo(1).zip` → `server/xmlrpc`: 24 archivos de código/datos comparados byte a byte, sin cambios.
- `TP2_req3.zip` → `server/include/tp2_logger y server/src/tp2_logger`: 10 archivos de código/datos comparados byte a byte, sin cambios.
- `robotArm_v0.62sim(2).zip` → `firmware/arduino`: 26 archivos de código/datos comparados byte a byte, sin cambios.

Los ejemplos se separaron para evitar múltiples main. Se excluyen binarios precompilados del TP original: se compila todo desde fuentes. Los documentos de las consignas siguen siendo referencias externas, no dependencias de compilación. Se conserva la cabecera de licencia original de XmlRpc++; COPYING.LESSER contiene el texto LGPL 2.1 distribuido por el sistema.

## Adaptaciones posteriores para Windows

La comparacion byte a byte anterior describe la importacion original. La adaptacion de Windows modifica de forma localizada los tipos de socket en XmlRpc++, sus comprobaciones de errores y formatos de diagnostico. Tambien adapta la fecha/hora en Logger_396. Se conservan las licencias originales. Ver COMPATIBILIDAD_WINDOWS.md.

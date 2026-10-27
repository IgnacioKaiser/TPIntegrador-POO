# Instrucciones del proyecto POO

## Antes de trabajar
- Leer docs/ESTADO_PROYECTO.md, docs/DECISIONES_DISENO.md y docs/CONTRATO.md.
- Leer el enunciado oficial docs/consignas/TPI_2026.pdf y los anexos relevantes. Un resumen no sustituye al PDF.
- Si faltan fuentes necesarias, informar que faltan. No afirmar que se leyeron.
- Revisar codigo, rama y cambios locales antes de editar. El estado documentado tiene fecha y puede quedar desactualizado.
- Tomar el reparto de trabajo de la ultima indicacion explicita del equipo. Jira sirve como descripcion de requisitos, no como evidencia de responsables o finalizacion.
- Acordar interfaces con el trabajo paralelo del robot antes de modificar puntos compartidos. No suponer que una rama publicada representa todo el trabajo local de otros integrantes.

## Fuentes y alcance
- Priorizar consigna oficial y aclaraciones explicitas de catedra; despues anexos, materiales de clase y TPs. Los informes antiguos y apuntes son referencias secundarias.
- Respetar las decisiones vigentes; si una contradice la consigna, explicar la diferencia y corregir su registro al resolverla.
- Implementar solo la historia solicitada y lo necesario para cumplirla. No convertir mejoras opcionales en obligatorias.

## Codigo y explicacion
- Trabajar progresivamente y explicar en espanol sencillo el problema, el cambio y su motivo.
- Conservar orientacion a objetos y separar presentacion, logica y transporte.
- Mantener C++17 en servidor y Python en cliente; compatibilidad Windows MSYS2 UCRT64 y Linux.
- Reutilizar clases existentes sin reconstruir el proyecto ni agregar dependencias o jerarquias innecesarias.
- Evitar new manual cuando los objetos por valor y referencias resuelvan el problema. Encapsular y liberar los recursos de APIs externas.
- No cambiar el contrato RPC sin justificarlo y actualizar ambos extremos, documentacion y pruebas afectadas.
- Mantener la autenticacion en el servidor antes de ejecutar operaciones. No confiar en permisos comprobados solo por el cliente.
- No registrar contrasenas ni hashes. No versionar bases reales ni ejecutables.

## Verificacion y registro
- Ejecutar las pruebas apropiadas al cambio y reportar el entorno real: no confundir compilacion cruzada con ejecucion en Windows.
- Usar bases y archivos temporales en pruebas, sin modificar usuarios ni trayectorias reales.
- Documentar cada historia en docs/backlog/US-XX.md. Usar PLANTILLA.md como guia y omitir secciones que no aporten.
- Explicar que es cada biblioteca o concepto nuevo, para que sirve, su relacion con el TPI, alternativas y costo de la eleccion.
- Actualizar docs/DECISIONES_DISENO.md para decisiones compartidas y docs/ESTADO_PROYECTO.md al cerrar una etapa.
- Distinguir implementado, probado e integrado. No declarar terminada una historia solo por compilar.
- Conservar cambios ajenos. No hacer force-push, descartar archivos ni cambiar la base real para resolver una integracion rutinaria.

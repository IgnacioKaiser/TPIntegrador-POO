"""Comprueba repositorio y herramienta con una base temporal; no usa datos reales."""
from pathlib import Path
from contextlib import closing
import os
import sqlite3
import subprocess
import tempfile

build = Path(__file__).resolve().parents[1] / "build"
extension = ".exe" if os.name == "nt" else ""
with tempfile.TemporaryDirectory(prefix="poo_usuarios_") as carpeta:
    base = Path(carpeta) / "usuarios.sqlite"
    subprocess.run([str(build / ("test_usuarios" + extension)), str(base)], check=True)

    def ejecutar(accion, entrada, correcto):
        resultado = subprocess.run(
            [str(build / ("usuarios_admin" + extension)), accion, "--db", str(base)],
            input=entrada, capture_output=True, text=True, timeout=15)
        if (resultado.returncode == 0) != correcto:
            raise RuntimeError(resultado.stdout + resultado.stderr)
        return resultado.stdout + resultado.stderr

    ejecutar("agregar", "pedro\nClavePedro1\n", False)
    ejecutar("listar", "admin\nIncorrecta\n", False)
    ejecutar("agregar", "admin\nClaveAdmin1\nnuevo\nClaveNueva1\nClaveNueva1\n", True)
    salida = ejecutar("listar", "admin\nClaveAdmin1\n", True)
    if "nuevo" not in salida or "ClaveNueva1" in salida or "$argon2" in salida:
        raise RuntimeError("El listado es incorrecto o expone credenciales.")
    with closing(sqlite3.connect(base)) as db:
        hashes = db.execute("SELECT clave_hash FROM usuarios").fetchall()
        if not all(h[0].startswith("$argon2") for h in hashes):
            raise RuntimeError("Se esperaba una credencial Argon2, no texto plano.")
    print("CLI: acceso administrativo y listado sin claves OK.")

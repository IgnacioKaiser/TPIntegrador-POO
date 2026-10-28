"""Ejecuta la prueba C++ con una base temporal exclusiva."""
from pathlib import Path
import os
import subprocess
import tempfile

build = Path(__file__).resolve().parents[1] / "build"
extension = ".exe" if os.name == "nt" else ""
with tempfile.TemporaryDirectory(prefix="poo_auth_") as carpeta:
    subprocess.run([str(build / ("test_autenticacion" + extension)),
                    str(Path(carpeta) / "usuarios.sqlite"),
                    str(Path(carpeta) / "tareas")], check=True)

"""Ejecutor C++ con archivos temporales y controlador sustituido, sin puertos reales."""
from pathlib import Path
import os
import subprocess
import tempfile

build = Path(__file__).resolve().parents[1] / "build"
with tempfile.TemporaryDirectory(prefix="poo_us17_h2_") as carpeta:
    subprocess.run([str(build / ("test_ejecutor_tareas.exe" if os.name == "nt" else "test_ejecutor_tareas")),
                    carpeta], check=True, timeout=30)

"""Hito A: prueba el repositorio C++ con archivos temporales, sin RPC ni usuarios reales."""
from pathlib import Path
import os
import subprocess
import tempfile

build = Path(__file__).resolve().parents[1] / "build"
ejecutable = build / ("test_tareas.exe" if os.name == "nt" else "test_tareas")
with tempfile.TemporaryDirectory(prefix="poo_tareas_") as carpeta:
    subprocess.run([str(ejecutable), carpeta], check=True, timeout=30)

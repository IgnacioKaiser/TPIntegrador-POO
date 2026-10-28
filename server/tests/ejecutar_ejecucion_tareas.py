"""US-17 Hito 1: archivos temporales y canal sustituido; nunca abre un puerto real."""
from pathlib import Path
import os
import subprocess
import tempfile

build = Path(__file__).resolve().parents[1] / "build"
ejecutable = build / ("test_ejecucion_tareas.exe" if os.name == "nt" else "test_ejecucion_tareas")
with tempfile.TemporaryDirectory(prefix="poo_us17_") as carpeta:
    subprocess.run([str(ejecutable), carpeta], check=True, timeout=30)

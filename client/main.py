"""Punto de entrada alternativo; reutiliza la CLI existente."""
from robot_client.cli import main

if __name__ == "__main__":
    raise SystemExit(main())

"""Interfaz de consola del cliente, separada del transporte y del modelo."""

import argparse
import json
import sys

from .cliente import ClienteXmlRpc, ErrorComunicacion, ServiciosPrueba
from .mensaje import MensajeInvalido


class CliCliente:
    def __init__(self, servicios: ServiciosPrueba):
        self.servicios = servicios

    @staticmethod
    def mostrar(respuesta):
        # ping devuelve resultado.mensaje; eco devuelve resultado.texto.
        datos = respuesta.resultado
        if "texto" in datos and isinstance(datos["texto"], str):
            detalle = datos["texto"]
        elif "mensaje" in datos and isinstance(datos["mensaje"], str):
            detalle = datos["mensaje"]
        else:
            # Tambien sirve para futuros resultados estructurados.
            detalle = json.dumps(datos, ensure_ascii=False)

        print(f"[{respuesta.codigo}] {detalle}")
        print(f"ID: {respuesta.id}")

    def ejecutar(self):
        while True:
            print("\n1. Comprobar conexion (ping)\n2. Enviar texto (eco)\n0. Salir")
            opcion = input("Opcion: ").strip()
            if opcion == "0":
                return
            try:
                if opcion == "1":
                    self.mostrar(self.servicios.ping())
                elif opcion == "2":
                    self.mostrar(self.servicios.eco(input("Texto: ")))
                else:
                    print("Opcion invalida.")
            except (ErrorComunicacion, MensajeInvalido) as error:
                print(f"Error: {error}", file=sys.stderr)


def main(argv=None):
    parser = argparse.ArgumentParser(
        description="Cliente Python del TP: comunicacion XML-RPC"
    )
    parser.add_argument("--url", default="http://127.0.0.1:8080/RPC2")
    parser.add_argument("--timeout", type=float, default=3.0)
    comandos = parser.add_subparsers(dest="comando")
    comandos.add_parser("ping", help="Comprobar que el servidor responde")
    eco = comandos.add_parser("eco", help="Enviar y recuperar texto")
    eco.add_argument("texto")
    args = parser.parse_args(argv)

    try:
        servicios = ServiciosPrueba(ClienteXmlRpc(args.url, args.timeout))
        cli = CliCliente(servicios)
        if args.comando == "ping":
            cli.mostrar(servicios.ping())
        elif args.comando == "eco":
            cli.mostrar(servicios.eco(args.texto))
        else:
            cli.ejecutar()
        return 0
    except (ErrorComunicacion, ValueError) as error:
        print(f"Error: {error}", file=sys.stderr)
        return 1
    except (KeyboardInterrupt, EOFError):
        print("\nCliente cerrado.")
        return 0

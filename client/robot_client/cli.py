"""Interfaz de consola del cliente, separada del transporte y del modelo."""

import argparse
from getpass import getpass
import json
import sys

from .cliente import ClienteXmlRpc, ErrorComunicacion, ServiciosPrueba
from .mensaje import MensajeInvalido


class CliCliente:
    def __init__(self, servicios: ServiciosPrueba):
        self.servicios = servicios

    @staticmethod
    def mostrar(respuesta):
        datos = respuesta.resultado
        if "mensaje" in datos and isinstance(datos["mensaje"], str):
            detalle = datos["mensaje"]
        elif "texto" in datos and isinstance(datos["texto"], str):
            detalle = datos["texto"]
        else:
            detalle = json.dumps(datos, ensure_ascii=False)
        print(f"[{respuesta.codigo}] {detalle}")
        print(f"ID: {respuesta.id}")

    def ejecutar(self):
        while True:
            print("\n1. Comprobar conexion (ping)"
                  "\n2. Enviar texto (eco)"
                  "\n3. Habilitar operador (solo administrador)"
                  "\n4. Deshabilitar operador (solo administrador)"
                  "\n0. Salir")
            opcion = input("Opcion: ").strip()
            if opcion == "0":
                return
            try:
                if opcion == "1":
                    self.mostrar(self.servicios.ping())
                elif opcion == "2":
                    self.mostrar(self.servicios.eco(input("Texto: ")))
                elif opcion == "3":
                    self.mostrar(self.servicios.habilitar_operador(
                        input("Nombre del operador: ").strip()))
                elif opcion == "4":
                    self.mostrar(self.servicios.deshabilitar_operador(
                        input("Nombre del operador: ").strip()))
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
    habilitar = comandos.add_parser("habilitar", help="Habilitar operador (administrador)")
    habilitar.add_argument("usuario")
    deshabilitar = comandos.add_parser("deshabilitar", help="Deshabilitar operador (administrador)")
    deshabilitar.add_argument("usuario")
    args = parser.parse_args(argv)
    try:
        usuario = input("Usuario: ").strip()
        clave = getpass("Clave: ")
        servicios = ServiciosPrueba(ClienteXmlRpc(args.url, args.timeout), usuario, clave)
        cli = CliCliente(servicios)
        if args.comando == "ping":
            respuesta = servicios.ping()
        elif args.comando == "eco":
            respuesta = servicios.eco(args.texto)
        elif args.comando == "habilitar":
            respuesta = servicios.habilitar_operador(args.usuario)
        elif args.comando == "deshabilitar":
            respuesta = servicios.deshabilitar_operador(args.usuario)
        else:
            cli.ejecutar()
            return 0
        cli.mostrar(respuesta)
        return 0 if respuesta.codigo == "OK" else 1
    except (ErrorComunicacion, MensajeInvalido, ValueError) as error:
        print(f"Error: {error}", file=sys.stderr)
        return 1
    except (KeyboardInterrupt, EOFError):
        print("\nCliente cerrado.")
        return 0

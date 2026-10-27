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
                  "\n5. Consultar estado y modos del robot"
                  "\n6. Seleccionar modo de trabajo"
                  "\n7. Seleccionar modo de coordenadas"
                  "\n8. Enviar efector a origen/descanso"
                  "\n9. Mover efector a X, Y, Z"
                  "\n10. Activar efector (cerrar pinza)"
                  "\n11. Desactivar efector (abrir pinza)"
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
                elif opcion == "5":
                    self.mostrar(self.servicios.estado_robot())
                elif opcion == "6":
                    modo = input("Modo (manual/automatico): ").strip()
                    self.mostrar(
                        self.servicios.seleccionar_modo_trabajo(modo))
                elif opcion == "7":
                    modo = input("Modo (absoluto/relativo): ").strip()
                    self.mostrar(
                        self.servicios.seleccionar_modo_coordenadas(modo))
                elif opcion == "8":
                    self.mostrar(self.servicios.ir_a_origen())
                elif opcion == "9":
                    x = float(input("X (mm): "))
                    y = float(input("Y (mm): "))
                    z = float(input("Z (mm): "))
                    texto_velocidad = input(
                        "Velocidad (5-10 mm/s, vacio para 5): ").strip()
                    velocidad = (
                        None if not texto_velocidad
                        else float(texto_velocidad)
                    )
                    self.mostrar(
                        self.servicios.mover(x, y, z, velocidad))
                elif opcion == "10":
                    self.mostrar(self.servicios.activar_efector())
                elif opcion == "11":
                    self.mostrar(self.servicios.desactivar_efector())
                else:
                    print("Opcion invalida.")
            except (ErrorComunicacion, MensajeInvalido, ValueError) as error:
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
    modo_trabajo = comandos.add_parser(
        "modo-trabajo", help="Seleccionar modo manual o automatico")
    modo_trabajo.add_argument(
        "modo", choices=("manual", "automatico"))
    modo_coordenadas = comandos.add_parser(
        "modo-coordenadas", help="Seleccionar coordenadas absolutas o relativas")
    modo_coordenadas.add_argument(
        "modo", choices=("absoluto", "relativo"))
    comandos.add_parser("estado", help="Consultar estado y modos del robot")
    comandos.add_parser("origen", help="Enviar el efector a origen/descanso")
    mover = comandos.add_parser(
        "mover", help="Mover el efector a coordenadas X, Y, Z")
    mover.add_argument("x", type=float)
    mover.add_argument("y", type=float)
    mover.add_argument("z", type=float)
    mover.add_argument("--velocidad", type=float)
    comandos.add_parser(
        "activar-efector", help="Activar/cerrar el efector")
    comandos.add_parser(
        "desactivar-efector", help="Desactivar/abrir el efector")
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
        elif args.comando == "modo-trabajo":
            respuesta = servicios.seleccionar_modo_trabajo(args.modo)
        elif args.comando == "modo-coordenadas":
            respuesta = servicios.seleccionar_modo_coordenadas(args.modo)
        elif args.comando == "estado":
            respuesta = servicios.estado_robot()
        elif args.comando == "origen":
            respuesta = servicios.ir_a_origen()
        elif args.comando == "mover":
            respuesta = servicios.mover(
                args.x, args.y, args.z, args.velocidad)
        elif args.comando == "activar-efector":
            respuesta = servicios.activar_efector()
        elif args.comando == "desactivar-efector":
            respuesta = servicios.desactivar_efector()
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

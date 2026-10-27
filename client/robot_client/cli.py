"""Interfaz de consola del cliente, separada del transporte y del modelo."""

import argparse
from getpass import getpass
import sys

from .cliente import ClienteXmlRpc, ErrorComunicacion, ServiciosPrueba
from .mensaje import MensajeInvalido


class CliCliente:
    def __init__(self, servicios: ServiciosPrueba):
        self.servicios = servicios

    @staticmethod
    def mostrar_datos(datos, sangria=0):
        """Recorre estructuras anidadas sin inventar valores faltantes."""
        for nombre, valor in datos.items():
            prefijo = " " * sangria + nombre + ":"
            if isinstance(valor, dict):
                print(prefijo)
                CliCliente.mostrar_datos(valor, sangria + 2)
            else:
                if isinstance(valor, bool):
                    valor = "si" if valor else "no"
                print(f"{prefijo} {valor}")

    @staticmethod
    def mostrar(respuesta):
        datos = respuesta.resultado
        # Mostrar el mensaje no debe ocultar datos parciales de un error de estado.
        principal = next((campo for campo in ("mensaje", "texto")
                          if isinstance(datos.get(campo), str)), None)
        detalle = datos[principal] if principal else "Respuesta del servidor."
        print(f"[{respuesta.codigo}] {detalle}")
        CliCliente.mostrar_datos({k: v for k, v in datos.items() if k != principal})
        if (respuesta.codigo == "OK" and respuesta.operacion in
                ("activar_motores", "desactivar_motores")):
            print("Confirmacion del firmware; no verifica una accion fisica.")
        print(f"ID: {respuesta.id}")

    def ejecutar(self):
        while True:
            print("\n1. Comprobar conexion (ping)"
                  "\n2. Enviar texto (eco)"
                  "\n3. Habilitar operador (solo administrador)"
                  "\n4. Deshabilitar operador (solo administrador)"
                  "\n5. Conectar robot (solo administrador)"
                  "\n6. Desconectar robot (solo administrador)"
                  "\n7. Consultar estado del robot"
                  "\n8. Activar motores"
                  "\n9. Desactivar motores"
                  "\n10. Ayuda del servidor"
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
                    puerto = input("Puerto serie (Enter: predeterminado del servidor): ").strip()
                    self.mostrar(self.servicios.conectar(puerto or None))
                elif opcion in ("6", "7", "8", "9", "10"):
                    acciones = {"6": self.servicios.desconectar,
                                "7": self.servicios.estado,
                                "8": self.servicios.activar_motores,
                                "9": self.servicios.desactivar_motores,
                                "10": self.servicios.ayuda}
                    self.mostrar(acciones[opcion]())
                else:
                    print("Opcion invalida.")
            except (ErrorComunicacion, MensajeInvalido) as error:
                print(f"Error: {error}", file=sys.stderr)
                if isinstance(error, ErrorComunicacion):
                    print("Ejecucion no confirmada. No se reintenta automaticamente.",
                          file=sys.stderr)


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
    conectar = comandos.add_parser("conectar", help="Conectar robot (solo administrador)")
    conectar.add_argument("puerto", nargs="?", help="Puerto serie del servidor")
    comandos.add_parser("desconectar", help="Desconectar robot (solo administrador)")
    comandos.add_parser("estado", help="Consultar conexion, posicion y actividad")
    comandos.add_parser("activar_motores", help="Solicitar activacion de motores")
    comandos.add_parser("desactivar_motores", help="Solicitar desactivacion de motores")
    comandos.add_parser("ayuda", help="Consultar comandos del servidor")
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
        elif args.comando == "conectar":
            respuesta = servicios.conectar(args.puerto)
        elif args.comando in ("desconectar", "estado", "activar_motores",
                              "desactivar_motores", "ayuda"):
            # Los nombres del comando y del metodo coinciden con el contrato.
            respuesta = getattr(servicios, args.comando)()
        else:
            cli.ejecutar()
            return 0
        cli.mostrar(respuesta)
        return 0 if respuesta.codigo == "OK" else 1
    except (ErrorComunicacion, MensajeInvalido, ValueError) as error:
        print(f"Error: {error}", file=sys.stderr)
        if isinstance(error, ErrorComunicacion):
            print("Ejecucion no confirmada. No se reintenta automaticamente.",
                  file=sys.stderr)
        return 1
    except (KeyboardInterrupt, EOFError):
        print("\nCliente cerrado.")
        return 0

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
        if respuesta.codigo == "OK" and respuesta.operacion == "informe.propio":
            print(f"[OK] Actividad propia de {datos['usuario']} (UTC)")
            referencia = {"inicio": "inicio del servidor (sin conexion posterior)",
                          "conectar": "ultima conexion del robot"}.get(
                              datos['referencia'], "sin referencia disponible")
            print(f"Periodo: {referencia} | "
                  f"{datos['desde'] or 'fecha no disponible'}")
            print(f"Inicio de actividad propia: {datos['inicio_actividad'] or 'sin actividad'}")
            print(f"Peticiones: {datos['total']} | Errores: {datos['errores']} | "
                  f"Mostradas: {datos['mostrados']} | Offset: {datos['offset']}")
            print(f"Estado del robot [{datos['codigo_estado']}]:")
            CliCliente.mostrar_datos(datos['estado'], 2)
            if not datos['total']:
                print("Sin actividad propia en este periodo.")
            elif not datos['registros']:
                print("No hay registros en esta pagina; revise el offset.")
            for fila in datos['registros']:
                print(f"{fila['fecha_hora']} | {fila['id_solicitud']} | "
                      f"{fila['operacion']} | {fila['codigo']} | {fila['detalle']}")
            return
        if respuesta.codigo == "OK" and respuesta.operacion == "informe.usuarios":
            print(f"[OK] Peticiones: {datos.get('total_peticiones', 0)}  "
                  f"Correctas: {datos.get('total_ok', 0)}  "
                  f"Rechazadas: {datos.get('total_errores', 0)}")
            print("Usuario | Peticiones | OK | Errores")
            for fila in datos.get("usuarios", []):
                print(f"{fila['usuario']} | {fila['peticiones']} | "
                      f"{fila['exitosas']} | {fila['rechazadas']}")
            return
        if respuesta.codigo == "OK" and respuesta.operacion == "log.consultar":
            print(f"[OK] {datos.get('mostrados', 0)} de {datos.get('total', 0)} "
                  "entradas (UTC)")
            for fila in datos.get("registros", []):
                print(f"{fila['fecha_hora']} | {fila['tipo']} | "
                      f"{fila['usuario'] or '(sin autenticar)'} | {fila['operacion']} | "
                      f"{fila['codigo']} | {fila['detalle']}")
            return
        if respuesta.operacion == "tarea.listar" and respuesta.codigo == "OK":
            tareas = datos.get("tareas")
            if not isinstance(tareas, list) or any(
                not isinstance(tarea, dict) or set(tarea) != {"id", "nombre", "propietario"}
                or any(not isinstance(valor, str) for valor in tarea.values()) for tarea in tareas
            ):
                raise MensajeInvalido("El servidor no devolvio un listado de tareas valido.")
            print(f"[OK] {len(tareas)} tareas disponibles.")
            if not tareas:
                print("No hay tareas disponibles.")
            else:
                print("ID | Nombre | Propietario")
                for tarea in tareas:
                    print(f"{tarea['id']} | {tarea['nombre']} | {tarea['propietario']}")
            print(f"ID de solicitud: {respuesta.id}")
            return
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
        # Consultar identidad antes del menu. No se deduce el rol por el nombre.
        identidad = self.servicios.usuario_actual()
        if identidad.codigo != "OK":
            self.mostrar(identidad)
            return 1
        rol = identidad.resultado.get("rol")
        usuario = identidad.resultado.get("usuario")
        if rol not in ("ADMINISTRADOR", "OPERADOR"):
            raise MensajeInvalido("El servidor no informo un rol reconocido. No se abre el menu.")
        if not isinstance(usuario, str) or not usuario:
            raise MensajeInvalido("El servidor no informo el usuario autenticado. No se abre el menu.")
        administrador = rol == "ADMINISTRADOR"
        print(f"Usuario autenticado: {usuario} ({rol})")

        while True:
            print("\n1. Comprobar conexion (ping)"
                  "\n2. Enviar texto (eco)")
            if administrador:
                print("3. Habilitar operador (solo administrador)"
                      "\n4. Deshabilitar operador (solo administrador)"
                      "\n5. Conectar robot (solo administrador)"
                      "\n6. Desconectar robot (solo administrador)")
                print("19. Informe de usuarios (solo administrador)"
                      "\n20. Consultar log CSV (solo administrador)")
            print("7. Consultar estado del robot"
                  "\n8. Activar motores"
                  "\n9. Desactivar motores"
                  "\n10. Ayuda del servidor"
                  "\n11. Seleccionar modo de trabajo"
                  "\n12. Seleccionar modo de coordenadas"
                  "\n13. Enviar efector a origen/descanso"
                  "\n14. Mover efector a X, Y, Z"
                  "\n15. Activar efector (cerrar pinza)"
                  "\n16. Desactivar efector (abrir pinza)"
                  "\n17. Subir archivo de tarea (sin ejecutar)"
                  "\n18. Listar tareas disponibles"
                  "\n21. Consultar mi actividad"
                  "\n0. Salir")
            opcion = input("Opcion: ").strip()
            if opcion == "0":
                return 0
            # Ocultar opciones no reemplaza los permisos comprobados por el servidor.
            if opcion in ("3", "4", "5", "6", "19", "20") and not administrador:
                print("Opcion no disponible para este perfil.")
                continue
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
                elif opcion == "6":
                    self.mostrar(self.servicios.desconectar())
                elif opcion == "7":
                    self.mostrar(self.servicios.estado())
                elif opcion == "8":
                    self.mostrar(self.servicios.activar_motores())
                elif opcion == "9":
                    self.mostrar(self.servicios.desactivar_motores())
                elif opcion == "10":
                    self.mostrar(self.servicios.ayuda())
                elif opcion == "11":
                    modo = input("Modo (manual/automatico): ").strip()
                    self.mostrar(
                        self.servicios.seleccionar_modo_trabajo(modo))
                elif opcion == "12":
                    modo = input("Modo (absoluto/relativo): ").strip()
                    self.mostrar(
                        self.servicios.seleccionar_modo_coordenadas(modo))
                elif opcion == "13":
                    self.mostrar(self.servicios.ir_a_origen())
                elif opcion == "14":
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
                elif opcion == "15":
                    self.mostrar(self.servicios.activar_efector())
                elif opcion == "16":
                    self.mostrar(self.servicios.desactivar_efector())
                elif opcion == "17":
                    self.mostrar(self.servicios.subir_archivo(input("Ruta del archivo local: ").strip()))
                elif opcion == "18":
                    self.mostrar(self.servicios.listar_tareas())
                elif opcion == "21":
                    offset = int(input("Offset (Enter=0): ").strip() or "0")
                    limite = int(input("Limite (Enter=50, maximo 100): ").strip() or "50")
                    self.mostrar(self.servicios.actividad_propia(offset, limite))
                elif opcion in ("19", "20"):
                    usuario = input("Usuario (Enter=todos): ").strip()
                    desde = input("Desde AAAA-MM-DD (Enter=sin limite): ").strip()
                    hasta = input("Hasta AAAA-MM-DD (Enter=sin limite): ").strip()
                    codigo = input("Codigo (Enter=todos): ").strip().upper()
                    if opcion == "19":
                        self.mostrar(self.servicios.informe_usuarios(usuario, desde, hasta, codigo))
                    else:
                        tipo = input("Tipo PETICION/EVENTO (Enter=todos): ").strip().upper()
                        self.mostrar(self.servicios.consultar_log(usuario, desde, hasta, tipo, codigo))
                else:
                    print("Opcion invalida.")
            except (ErrorComunicacion, MensajeInvalido, ValueError) as error:
                print(f"Error: {error}", file=sys.stderr)
                if isinstance(error, ErrorComunicacion):
                    print("Resultado no confirmado. No se reintenta automaticamente.",
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
    conectar = comandos.add_parser("conectar", help="Conectar robot (solo administrador)")
    conectar.add_argument("puerto", nargs="?", help="Puerto serie del servidor")
    comandos.add_parser("desconectar", help="Desconectar robot (solo administrador)")
    comandos.add_parser("activar_motores", help="Solicitar activacion de motores")
    comandos.add_parser("desactivar_motores", help="Solicitar desactivacion de motores")
    comandos.add_parser("ayuda", help="Consultar comandos del servidor")
    subir = comandos.add_parser("subir", help="Almacenar un archivo G-Code sin ejecutarlo")
    subir.add_argument("ruta", help="Ruta del archivo local UTF-8")
    comandos.add_parser("tareas", help="Listar tareas segun propietario y rol")
    actividad = comandos.add_parser("actividad", help="Consultar mi actividad desde la ultima conexion")
    actividad.add_argument("--offset", type=int, default=0)
    actividad.add_argument("--limite", type=int, default=50)
    informes = comandos.add_parser("informe-usuarios", help="Informe global (administrador)")
    log = comandos.add_parser("log", help="Consultar el CSV (administrador)")
    for p in (informes, log):
        p.add_argument("--usuario", default="")
        p.add_argument("--desde", default="", help="Fecha UTC AAAA-MM-DD")
        p.add_argument("--hasta", default="", help="Fecha UTC AAAA-MM-DD")
        p.add_argument("--codigo", default="")
    log.add_argument("--tipo", default="", choices=("", "PETICION", "EVENTO"))
    log.add_argument("--limite", type=int, default=50)
    log.add_argument("--offset", type=int, default=0)
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
            respuesta = servicios.estado()
        elif args.comando == "origen":
            respuesta = servicios.ir_a_origen()
        elif args.comando == "mover":
            respuesta = servicios.mover(
                args.x, args.y, args.z, args.velocidad)
        elif args.comando == "activar-efector":
            respuesta = servicios.activar_efector()
        elif args.comando == "desactivar-efector":
            respuesta = servicios.desactivar_efector()
        elif args.comando == "conectar":
            respuesta = servicios.conectar(args.puerto)
        elif args.comando == "subir":
            respuesta = servicios.subir_archivo(args.ruta)
        elif args.comando == "tareas":
            respuesta = servicios.listar_tareas()
        elif args.comando == "actividad":
            respuesta = servicios.actividad_propia(args.offset, args.limite)
        elif args.comando == "informe-usuarios":
            respuesta = servicios.informe_usuarios(
                args.usuario, args.desde, args.hasta, args.codigo)
        elif args.comando == "log":
            respuesta = servicios.consultar_log(
                args.usuario, args.desde, args.hasta, args.tipo,
                args.codigo, args.offset, args.limite)
        elif args.comando in ("desconectar", "estado", "activar_motores",
                              "desactivar_motores", "ayuda"):
            # Los nombres del comando y del metodo coinciden con el contrato.
            respuesta = getattr(servicios, args.comando)()
        else:
            return cli.ejecutar()
        cli.mostrar(respuesta)
        return 0 if respuesta.codigo == "OK" else 1
    except (ErrorComunicacion, MensajeInvalido, ValueError) as error:
        print(f"Error: {error}", file=sys.stderr)
        if isinstance(error, ErrorComunicacion):
            print("Resultado no confirmado. No se reintenta automaticamente.",
                  file=sys.stderr)
        return 1
    except (KeyboardInterrupt, EOFError):
        print("\nCliente cerrado.")
        return 0

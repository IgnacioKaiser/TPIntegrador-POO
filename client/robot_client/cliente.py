"""Transporte XML-RPC y servicios del cliente, sin interfaz de consola."""

import math
from pathlib import Path
from http.client import HTTPException
from urllib.parse import urlsplit
from xml.parsers.expat import ExpatError
from xmlrpc.client import Fault, ProtocolError, ResponseError, ServerProxy, Transport

from .mensaje import MensajeInvalido, MensajeRPC

MAX_TAREA_BYTES = 1024 * 1024  # Mismo limite que el almacenamiento del servidor.


class ErrorComunicacion(RuntimeError):
    """Error de transporte o de respuesta remota invalida."""


class TransporteConTimeout(Transport):
    def __init__(self, timeout: float):
        super().__init__()
        self.timeout = timeout

    def make_connection(self, host):
        conexion = super().make_connection(host)
        conexion.timeout = self.timeout
        return conexion

    def request(self, host, handler, request_body, verbose=False):
        # No reintentar automaticamente: en el futuro puede haber movimientos.
        return self.single_request(host, handler, request_body, verbose)


class ClienteXmlRpc:
    def __init__(self, url="http://127.0.0.1:8080/RPC2", timeout=3.0):
        partes = urlsplit(url)
        if (partes.scheme != "http" or not partes.hostname
                or partes.username is not None or partes.password is not None
                or partes.query or partes.fragment):
            raise ValueError(
                "Usa una URL HTTP sin credenciales, por ejemplo "
                "http://127.0.0.1:8080/RPC2"
            )
        try:
            puerto = partes.port
        except ValueError as error:
            raise ValueError("Puerto invalido en la URL.") from error
        if (puerto == 0 or type(timeout) not in (int, float)
                or not math.isfinite(timeout) or timeout <= 0):
            raise ValueError("Puerto y timeout deben ser validos; timeout positivo.")
        self.url = url
        self.timeout = timeout

    def enviar(self, solicitud: MensajeRPC) -> MensajeRPC:
        if not isinstance(solicitud, MensajeRPC) or solicitud.tipo != "solicitud":
            raise MensajeInvalido("Solo se pueden enviar objetos MensajeRPC de solicitud.")
        try:
            with ServerProxy(
                self.url,
                transport=TransporteConTimeout(self.timeout),
                allow_none=False,
                encoding="utf-8",
            ) as remoto:
                campos = remoto.robot.intercambiar(solicitud.serializar())
            respuesta = MensajeRPC.deserializar(campos)
        except Fault as error:
            raise ErrorComunicacion(
                f"El servidor rechazo el mensaje ({error.faultCode}): "
                f"{error.faultString}"
            ) from error
        except (ProtocolError, ResponseError, OSError, HTTPException,
                ExpatError, MensajeInvalido, TypeError, ValueError) as error:
            raise ErrorComunicacion(
                f"No se obtuvo una respuesta valida desde {self.url}: {error}"
            ) from error
        if (respuesta.tipo != "respuesta" or respuesta.id != solicitud.id
                or respuesta.operacion != solicitud.operacion):
            raise ErrorComunicacion("La respuesta no corresponde a la solicitud enviada.")
        return respuesta


class ServiciosPrueba:
    """Solicitudes autenticadas, independientes de la CLI."""

    def __init__(self, cliente: ClienteXmlRpc, usuario: str, clave: str):
        self.cliente = cliente
        self._credenciales = {"usuario": usuario, "clave": clave}

    def _enviar(self, operacion: str, parametros: dict | None = None) -> MensajeRPC:
        datos = {} if parametros is None else dict(parametros)
        datos["credenciales"] = dict(self._credenciales)
        return self.cliente.enviar(MensajeRPC.solicitud(operacion, datos))

    def ping(self) -> MensajeRPC:
        return self._enviar("ping")

    def eco(self, texto: str) -> MensajeRPC:
        return self._enviar("eco", {"texto": texto})

    def usuario_actual(self) -> MensajeRPC:
        return self._enviar("usuario.actual")

    def subir_tarea(self, nombre: str, contenido: str) -> MensajeRPC:
        return self._enviar("tarea.subir", {"nombre": nombre, "contenido": contenido})

    def subir_archivo(self, ruta: str) -> MensajeRPC:
        archivo = Path(ruta)
        try:
            if not archivo.is_file():
                raise ValueError("La ruta local debe corresponder a un archivo regular existente.")
            if archivo.stat().st_size > MAX_TAREA_BYTES:
                raise ValueError("El archivo local supera el limite de 1 MiB.")
            with archivo.open("rb") as entrada:
                datos = entrada.read(MAX_TAREA_BYTES + 1)
        except OSError as error:
            raise ValueError("No se pudo leer el archivo local.") from error
        # Limitar tambien la lectura si el archivo crecio despues de consultar su tamano.
        if len(datos) > MAX_TAREA_BYTES:
            raise ValueError("El archivo local supera el limite de 1 MiB.")
        try:
            contenido = datos.decode("utf-8-sig")  # Admite UTF-8 con o sin BOM.
        except UnicodeDecodeError as error:
            raise ValueError("El archivo local debe estar codificado en UTF-8.") from error
        return self.subir_tarea(archivo.name, contenido)

    def listar_tareas(self) -> MensajeRPC:
        return self._enviar("tarea.listar")

    def informe_usuarios(self, usuario="", desde="", hasta="", codigo="") -> MensajeRPC:
        datos = {k: v for k, v in {"usuario": usuario, "desde": desde,
                                  "hasta": hasta, "codigo": codigo}.items() if v}
        return self._enviar("informe.usuarios", datos)

    def consultar_log(self, usuario="", desde="", hasta="", tipo="", codigo="",
                      offset=0, limite=50) -> MensajeRPC:
        datos = {k: v for k, v in {"usuario": usuario, "desde": desde, "hasta": hasta,
                                  "tipo": tipo, "codigo": codigo}.items() if v}
        datos["offset"] = offset
        datos["limite"] = limite
        return self._enviar("log.consultar", datos)

    def conectar(self, puerto: str | None = None) -> MensajeRPC:
        # Al omitir puerto, el servidor elige el predeterminado de su sistema.
        parametros = {} if puerto is None else {"puerto": puerto}
        return self._enviar("conectar", parametros)

    def desconectar(self) -> MensajeRPC:
        return self._enviar("desconectar")

    def estado(self) -> MensajeRPC:
        return self._enviar("estado")

    def activar_motores(self) -> MensajeRPC:
        return self._enviar("activar_motores")

    def desactivar_motores(self) -> MensajeRPC:
        return self._enviar("desactivar_motores")

    def ayuda(self) -> MensajeRPC:
        return self._enviar("ayuda")

    def habilitar_operador(self, usuario: str) -> MensajeRPC:
        return self._enviar("usuario.habilitar", {"usuario": usuario})

    def deshabilitar_operador(self, usuario: str) -> MensajeRPC:
        return self._enviar("usuario.deshabilitar", {"usuario": usuario})

    def seleccionar_modo_trabajo(self, modo: str) -> MensajeRPC:
        return self._enviar("seleccionar_modo_trabajo", {"modo": modo})

    def seleccionar_modo_coordenadas(self, modo: str) -> MensajeRPC:
        return self._enviar("seleccionar_modo_coordenadas", {"modo": modo})

    def estado_robot(self) -> MensajeRPC:
        return self._enviar("estado")

    def ir_a_origen(self) -> MensajeRPC:
        return self._enviar("ir_a_origen")

    def mover(
        self,
        x: float,
        y: float,
        z: float,
        velocidad: float | None = None,
    ) -> MensajeRPC:
        parametros = {"x": x, "y": y, "z": z}
        if velocidad is not None:
            parametros["velocidad"] = velocidad
        return self._enviar("mover", parametros)

    def activar_efector(self) -> MensajeRPC:
        return self._enviar("activar_efector")

    def desactivar_efector(self) -> MensajeRPC:
        return self._enviar("desactivar_efector")

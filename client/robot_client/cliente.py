"""Transporte XML-RPC y servicios; este módulo no imprime ni pide datos."""
import math
from http.client import HTTPException
from urllib.parse import urlsplit
from xml.parsers.expat import ExpatError
from xmlrpc.client import Fault, ProtocolError, ResponseError, ServerProxy, Transport

from .mensaje import MensajeInvalido, MensajeRPC


class ErrorComunicacion(RuntimeError):
    pass


class TransporteConTimeout(Transport):
    def __init__(self, timeout: float):
        super().__init__()
        self.timeout = timeout

    def make_connection(self, host):
        conexion = super().make_connection(host)
        conexion.timeout = self.timeout
        return conexion

    def request(self, host, handler, request_body, verbose=False):
        # Una sola tentativa: no repetir automáticamente futuras acciones del robot.
        return self.single_request(host, handler, request_body, verbose)


class ClienteXmlRpc:
    def __init__(self, url="http://127.0.0.1:8080/RPC2", timeout=3.0):
        partes = urlsplit(url)
        if (partes.scheme != "http" or not partes.hostname or partes.username is not None
                or partes.password is not None or partes.query or partes.fragment):
            raise ValueError("Usá una URL HTTP sin credenciales, por ejemplo http://127.0.0.1:8080/RPC2")
        try:
            puerto = partes.port
        except ValueError as error:
            raise ValueError("Puerto inválido en la URL.") from error
        if puerto == 0 or not math.isfinite(timeout) or timeout <= 0:
            raise ValueError("Puerto y timeout deben ser válidos; timeout debe ser positivo.")
        self.url = url
        self.timeout = timeout

    def enviar(self, solicitud: MensajeRPC) -> MensajeRPC:
        if solicitud.tipo != "solicitud":
            raise MensajeInvalido("Solo se pueden enviar solicitudes.")
        try:
            with ServerProxy(self.url, transport=TransporteConTimeout(self.timeout),
                             allow_none=False, encoding="utf-8") as remoto:
                campos = remoto.robot.intercambiar(solicitud.serializar())
            respuesta = MensajeRPC.deserializar(campos)
        except Fault as error:
            raise ErrorComunicacion(f"El servidor rechazó el mensaje ({error.faultCode}): {error.faultString}") from error
        except (ProtocolError, ResponseError, OSError, HTTPException, ExpatError, MensajeInvalido) as error:
            raise ErrorComunicacion(f"No se obtuvo una respuesta válida desde {self.url}: {error}") from error
        if (respuesta.tipo != "respuesta" or respuesta.id != solicitud.id
                or respuesta.operacion != solicitud.operacion):
            raise ErrorComunicacion("La respuesta no corresponde a la solicitud enviada.")
        return respuesta


class ServiciosPrueba:
    def __init__(self, cliente: ClienteXmlRpc):
        self.cliente = cliente

    def ping(self) -> MensajeRPC:
        return self.cliente.enviar(MensajeRPC.solicitud("ping"))

    def eco(self, texto: str) -> MensajeRPC:
        return self.cliente.enviar(MensajeRPC.solicitud("eco", texto))

"""Transporte XML-RPC y servicios del cliente, sin interfaz de consola."""

import math
from http.client import HTTPException
from urllib.parse import urlsplit
from xml.parsers.expat import ExpatError
from xmlrpc.client import Fault, ProtocolError, ResponseError, ServerProxy, Transport

from .mensaje import MensajeInvalido, MensajeRPC


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

    def habilitar_operador(self, usuario: str) -> MensajeRPC:
        return self._enviar("usuario.habilitar", {"usuario": usuario})

    def deshabilitar_operador(self, usuario: str) -> MensajeRPC:
        return self._enviar("usuario.deshabilitar", {"usuario": usuario})

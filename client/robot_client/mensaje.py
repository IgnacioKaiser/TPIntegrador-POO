"""Modelo de mensajes independiente del transporte XML-RPC."""

from copy import deepcopy
from dataclasses import dataclass, field
import math
import re
from uuid import uuid4


class MensajeInvalido(ValueError):
    pass


_ID = re.compile(r"[A-Za-z0-9_-]{1,64}\Z")
_OPERACION = re.compile(r"[A-Za-z][A-Za-z0-9_.-]{0,63}\Z")
_NOMBRE = re.compile(r"[A-Za-z][A-Za-z0-9_]{0,63}\Z")


def _texto_xml_valido(texto: str) -> bool:
    return all(
        ord(c) in (9, 10, 13)
        or 0x20 <= ord(c) <= 0xD7FF
        or 0xE000 <= ord(c) <= 0xFFFD
        or 0x10000 <= ord(c) <= 0x10FFFF
        for c in texto
    )


def _validar_valor(valor, nombre: str) -> None:
    """Admite string, int32, double, bool, list y dict anidados."""
    if type(valor) is bool:
        return

    if type(valor) is int:
        if not -(2**31) <= valor <= 2**31 - 1:
            raise MensajeInvalido(f"{nombre}: entero fuera del rango de 32 bits.")
        return

    if type(valor) is float:
        if not math.isfinite(valor):
            raise MensajeInvalido(f"{nombre}: double no finito.")
        return

    if type(valor) is str:
        if not _texto_xml_valido(valor):
            raise MensajeInvalido(f"{nombre}: caracteres no validos para XML 1.0.")
        try:
            valor.encode("utf-8")
        except UnicodeEncodeError as error:
            raise MensajeInvalido(f"{nombre}: texto UTF-8 invalido.") from error
        return

    if type(valor) is list:
        for indice, elemento in enumerate(valor):
            _validar_valor(elemento, f"{nombre}[{indice}]")
        return

    if type(valor) is dict:
        for clave, elemento in valor.items():
            if type(clave) is not str or _NOMBRE.fullmatch(clave) is None:
                raise MensajeInvalido(f"{nombre}: nombre de campo invalido.")
            _validar_valor(elemento, f"{nombre}.{clave}")
        return

    raise MensajeInvalido(f"{nombre}: tipo de dato no admitido.")


@dataclass(frozen=True)
class MensajeRPC:
    id: str
    tipo: str
    operacion: str
    parametros: dict = field(default_factory=dict)
    resultado: dict = field(default_factory=dict)
    codigo: str = ""

    def __post_init__(self):
        if type(self.id) is not str or _ID.fullmatch(self.id) is None:
            raise MensajeInvalido("id invalido.")

        if (type(self.operacion) is not str
                or _OPERACION.fullmatch(self.operacion) is None):
            raise MensajeInvalido("operacion invalida.")

        if type(self.tipo) is not str or self.tipo not in ("solicitud", "respuesta"):
            raise MensajeInvalido("tipo debe ser solicitud o respuesta.")

        if type(self.codigo) is not str:
            raise MensajeInvalido("codigo debe ser string.")

        if type(self.parametros) is not dict or type(self.resultado) is not dict:
            raise MensajeInvalido("parametros y resultado deben ser diccionarios.")

        _validar_valor(self.parametros, "parametros")
        _validar_valor(self.resultado, "resultado")

        if self.tipo == "solicitud":
            if self.codigo != "" or self.resultado:
                raise MensajeInvalido(
                    "Solicitud: codigo y resultado deben estar vacios.")
        else:
            if _ID.fullmatch(self.codigo) is None or self.parametros:
                raise MensajeInvalido(
                    "Respuesta: codigo valido y parametros vacios obligatorios.")

        # Evita conservar referencias a diccionarios externos mutables.
        object.__setattr__(self, "parametros", deepcopy(self.parametros))
        object.__setattr__(self, "resultado", deepcopy(self.resultado))

    @classmethod
    def solicitud(cls, operacion: str, parametros: dict | None = None):
        return cls(
            id=uuid4().hex,
            tipo="solicitud",
            operacion=operacion,
            parametros={} if parametros is None else parametros,
        )

    @classmethod
    def respuesta_a(cls, solicitud: "MensajeRPC",
                    resultado: dict | None = None, codigo: str = "OK"):
        if solicitud.tipo != "solicitud":
            raise MensajeInvalido("Se esperaba una solicitud para responder.")
        return cls(
            id=solicitud.id,
            tipo="respuesta",
            operacion=solicitud.operacion,
            resultado={} if resultado is None else resultado,
            codigo=codigo,
        )

    def serializar(self) -> dict:
        return {
            "id": self.id,
            "tipo": self.tipo,
            "operacion": self.operacion,
            "parametros": deepcopy(self.parametros),
            "resultado": deepcopy(self.resultado),
            "codigo": self.codigo,
        }

    @classmethod
    def deserializar(cls, campos):
        esperados = {
            "id", "tipo", "operacion", "parametros", "resultado", "codigo"
        }
        if type(campos) is not dict or set(campos) != esperados:
            raise MensajeInvalido(
                "El mensaje debe contener exactamente los seis campos del contrato.")
        return cls(**campos)
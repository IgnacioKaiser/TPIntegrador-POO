"""El mismo contrato de MensajeRPC que implementa el servidor C++."""
from dataclasses import dataclass
import re
from uuid import uuid4


class MensajeInvalido(ValueError):
    pass


@dataclass(frozen=True)
class MensajeRPC:
    id: str
    tipo: str
    operacion: str
    contenido: str = ""
    codigo: str = ""
    version: str = "1"

    def __post_init__(self):
        campos = self.serializar()
        if any(type(valor) is not str for valor in campos.values()):
            raise MensajeInvalido("Todos los campos deben ser strings.")
        if self.version != "1":
            raise MensajeInvalido("Versión de contrato no soportada; se requiere 1.")
        if not re.fullmatch(r"[A-Za-z0-9_-]{1,64}", self.id):
            raise MensajeInvalido("id inválido.")
        if not re.fullmatch(r"[A-Za-z0-9_-]{1,32}", self.operacion):
            raise MensajeInvalido("operacion inválida.")
        if self.tipo not in ("solicitud", "respuesta"):
            raise MensajeInvalido("tipo debe ser solicitud o respuesta.")
        try:
            tamano = len(self.contenido.encode("utf-8"))
        except UnicodeEncodeError as error:
            raise MensajeInvalido("contenido no es UTF-8 válido.") from error
        if tamano > 8192:
            raise MensajeInvalido("contenido supera 8192 bytes UTF-8.")
        if any(not (ord(c) in (9, 10, 13) or 0x20 <= ord(c) <= 0xD7FF
                    or 0xE000 <= ord(c) <= 0xFFFD) for c in self.contenido):
            raise MensajeInvalido("contenido debe ser texto XML 1.0 UTF-8 del plano básico, sin emojis.")
        if self.tipo == "solicitud" and self.codigo:
            raise MensajeInvalido("codigo debe estar vacío en solicitudes.")
        if self.tipo == "respuesta" and not re.fullmatch(r"[A-Za-z0-9_-]{1,64}", self.codigo):
            raise MensajeInvalido("codigo inválido en respuesta.")

    @classmethod
    def solicitud(cls, operacion: str, contenido: str = ""):
        return cls(id=uuid4().hex, tipo="solicitud", operacion=operacion, contenido=contenido)

    def serializar(self) -> dict[str, str]:
        return {"version": self.version, "id": self.id, "tipo": self.tipo,
                "operacion": self.operacion, "contenido": self.contenido, "codigo": self.codigo}

    @classmethod
    def deserializar(cls, campos):
        esperados = {"version", "id", "tipo", "operacion", "contenido", "codigo"}
        if type(campos) is not dict or set(campos) != esperados:
            raise MensajeInvalido("Se requieren exactamente los seis campos del contrato.")
        return cls(**campos)

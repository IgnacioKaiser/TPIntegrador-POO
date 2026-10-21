import unittest

from robot_client.cliente import ClienteXmlRpc
from robot_client.mensaje import MensajeInvalido, MensajeRPC


class MensajeTests(unittest.TestCase):
    def test_objeto_conserva_contenido_y_id(self):
        original = MensajeRPC.solicitud("eco", "¡Hola! <robot> & pinza")
        self.assertEqual(MensajeRPC.deserializar(original.serializar()), original)

    def test_rechaza_contratos_incompatibles(self):
        original = MensajeRPC.solicitud("ping").serializar()
        for cambio in ({"version": "2"}, {"id": ""}, {"contenido": 10},
                       {"tipo": "desconocido"}, {"codigo": "OK"}, {"extra": "campo"}):
            with self.subTest(cambio=cambio), self.assertRaises(MensajeInvalido):
                MensajeRPC.deserializar(original | cambio)

    def test_rechaza_campo_faltante(self):
        campos = MensajeRPC.solicitud("ping").serializar()
        del campos["contenido"]
        with self.assertRaises(MensajeInvalido):
            MensajeRPC.deserializar(campos)

    def test_limite_en_bytes_utf8(self):
        MensajeRPC.solicitud("eco", "ñ" * 4096)
        with self.assertRaises(MensajeInvalido):
            MensajeRPC.solicitud("eco", "ñ" * 4097)

    def test_rechaza_configuracion_invalida(self):
        for timeout in (0, -1, float("nan"), float("inf")):
            with self.subTest(timeout=timeout), self.assertRaises(ValueError):
                ClienteXmlRpc(timeout=timeout)
        with self.assertRaises(ValueError):
            ClienteXmlRpc("ftp://localhost/RPC2")

    def test_rechaza_texto_fuera_del_contrato(self):
        for texto in ("🤖", "\x00", "\uffff", "\ud800"):
            with self.subTest(texto=repr(texto)), self.assertRaises(MensajeInvalido):
                MensajeRPC.solicitud("eco", texto)


if __name__ == "__main__":
    unittest.main()

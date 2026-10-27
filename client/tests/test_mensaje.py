"""Pruebas unitarias locales: no necesitan servidor ni conexion de red."""

import unittest
from unittest.mock import patch

from robot_client.cliente import (ClienteXmlRpc, ErrorComunicacion,
                                  ServiciosPrueba)
from robot_client.mensaje import MensajeInvalido, MensajeRPC


class MensajeTests(unittest.TestCase):
    def test_solicitud_y_serializacion(self):
        original = MensajeRPC.solicitud("eco", {"texto": "Hola <robot> & pinza"})
        self.assertEqual(original.tipo, "solicitud")
        self.assertEqual(original.parametros["texto"], "Hola <robot> & pinza")
        self.assertEqual(original.resultado, {})
        self.assertEqual(original.codigo, "")
        self.assertEqual(set(original.serializar()), {
            "id", "tipo", "operacion", "parametros", "resultado", "codigo"
        })
        self.assertEqual(MensajeRPC.deserializar(original.serializar()), original)

    def test_respuesta_conserva_id_y_operacion(self):
        solicitud = MensajeRPC.solicitud("ping")
        respuesta = MensajeRPC.respuesta_a(solicitud, {"mensaje": "pong"})
        self.assertEqual(respuesta.id, solicitud.id)
        self.assertEqual(respuesta.operacion, "ping")
        self.assertEqual(respuesta.codigo, "OK")
        self.assertEqual(respuesta.parametros, {})
        self.assertEqual(respuesta.resultado["mensaje"], "pong")

    def test_rechaza_campos_incompatibles(self):
        base = MensajeRPC.solicitud("ping").serializar()
        for cambio in ({"version": "2"}, {"id": ""}, {"tipo": "desconocido"},
                       {"codigo": "OK"}, {"parametros": []},
                       {"resultado": {"mensaje": "no corresponde"}},
                       {"extra": "campo"}):
            with self.subTest(cambio=cambio), self.assertRaises(MensajeInvalido):
                MensajeRPC.deserializar(base | cambio)

    def test_rechaza_campo_faltante(self):
        campos = MensajeRPC.solicitud("ping").serializar()
        del campos["resultado"]
        with self.assertRaises(MensajeInvalido):
            MensajeRPC.deserializar(campos)

    def test_rechaza_tipos_y_nombres_invalidos(self):
        for datos in ({"posicion": None}, {"fecha": (1, 2)},
                      {"incorrecto-nombre": 10}, {"numero": 2**40},
                      {"numero": float("nan")}, {"numero": float("inf")}):
            with self.subTest(datos=datos), self.assertRaises(MensajeInvalido):
                MensajeRPC.solicitud("eco", datos)

    def test_admite_valores_tipados_y_anidados(self):
        parametros = {
            "x": 12.5, "velocidad": 50, "habilitado": True,
            "puntos": [1.0, 2.0], "config": {"modo": "prueba"}
        }
        solicitud = MensajeRPC.solicitud("mover", parametros)
        self.assertEqual(solicitud.parametros, parametros)
        parametros["config"]["modo"] = "modificado"
        self.assertEqual(solicitud.parametros["config"]["modo"], "prueba")
        salida = solicitud.serializar()
        salida["parametros"]["puntos"].append(3.0)
        self.assertEqual(solicitud.parametros["puntos"], [1.0, 2.0])

    def test_rechaza_texto_xml_invalido(self):
        for texto in ("\x00", "\uffff", "\ud800"):
            with self.subTest(texto=repr(texto)), self.assertRaises(MensajeInvalido):
                MensajeRPC.solicitud("eco", {"texto": texto})

    def test_rechaza_configuracion_invalida(self):
        for timeout in (0, -1, float("nan"), float("inf")):
            with self.subTest(timeout=timeout), self.assertRaises(ValueError):
                ClienteXmlRpc(timeout=timeout)
        with self.assertRaises(ValueError):
            ClienteXmlRpc("ftp://localhost/RPC2")

    def test_servicios_forman_parametros_con_nombre(self):
        class ClienteFalso:
            def enviar(self, solicitud):
                self.solicitud = solicitud
                return MensajeRPC.respuesta_a(solicitud, {"texto": "OK"})
        cliente = ClienteFalso()
        ServiciosPrueba(cliente, "operador", "ClavePrueba1").eco("Hola")
        self.assertEqual(cliente.solicitud.parametros, {"texto": "Hola", "credenciales": {"usuario": "operador", "clave": "ClavePrueba1"}})
        ServiciosPrueba(cliente, "operador", "ClavePrueba1").ping()
        self.assertEqual(cliente.solicitud.parametros, {"credenciales": {"usuario": "operador", "clave": "ClavePrueba1"}})

    def test_cliente_verifica_correlacion_de_respuesta(self):
        solicitud = MensajeRPC.solicitud("ping")
        respuesta = MensajeRPC.respuesta_a(solicitud, {"mensaje": "pong"}).serializar()
        respuesta["id"] = "otro"
        with patch("robot_client.cliente.ServerProxy") as remoto:
            remoto.return_value.__enter__.return_value.robot.intercambiar.return_value = respuesta
            with self.assertRaises(ErrorComunicacion):
                ClienteXmlRpc().enviar(solicitud)


if __name__ == "__main__":
    unittest.main()

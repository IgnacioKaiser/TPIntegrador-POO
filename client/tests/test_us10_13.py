"""Pruebas unitarias de las operaciones de control manual."""

import unittest
from unittest.mock import Mock

from robot_client.cliente import ServiciosPrueba
from robot_client.mensaje import MensajeRPC


class ServiciosControlRobotTests(unittest.TestCase):
    def setUp(self):
        self.transporte = Mock()
        self.servicios = ServiciosPrueba(
            self.transporte, "operador", "ClavePrueba1")

    def comprobar_solicitud(self, metodo, operacion, parametros):
        getattr(self.servicios, metodo)(*parametros)
        self.transporte.enviar.assert_called_once()
        solicitud = self.transporte.enviar.call_args.args[0]
        self.assertIsInstance(solicitud, MensajeRPC)
        self.assertEqual(solicitud.operacion, operacion)
        self.assertEqual(solicitud.codigo, "")
        self.assertEqual(solicitud.resultado, {})
        return solicitud.parametros

    def test_seleccion_de_modos(self):
        parametros = self.comprobar_solicitud(
            "seleccionar_modo_trabajo",
            "seleccionar_modo_trabajo",
            ("automatico",))
        self.assertEqual(parametros["modo"], "automatico")
        self.assertEqual(parametros["credenciales"]["usuario"], "operador")

        self.transporte.reset_mock()
        parametros = self.comprobar_solicitud(
            "seleccionar_modo_coordenadas",
            "seleccionar_modo_coordenadas",
            ("relativo",))
        self.assertEqual(parametros["modo"], "relativo")

    def test_origen_y_efector(self):
        for metodo, operacion in (
            ("ir_a_origen", "ir_a_origen"),
            ("activar_efector", "activar_efector"),
            ("desactivar_efector", "desactivar_efector"),
        ):
            with self.subTest(operacion=operacion):
                self.transporte.reset_mock()
                parametros = self.comprobar_solicitud(
                    metodo, operacion, ())
                self.assertEqual(set(parametros), {"credenciales"})

    def test_movimiento_con_y_sin_velocidad(self):
        parametros = self.comprobar_solicitud(
            "mover", "mover", (10.0, 20.0, 30.0))
        self.assertEqual(
            {clave: parametros[clave] for clave in ("x", "y", "z")},
            {"x": 10.0, "y": 20.0, "z": 30.0})
        self.assertNotIn("velocidad", parametros)
        self.transporte.reset_mock()

        parametros = self.comprobar_solicitud(
            "mover", "mover", (10.0, 20.0, 30.0, 7.5))
        self.assertEqual(parametros["velocidad"], 7.5)
        self.assertEqual(
            parametros["credenciales"],
            {"usuario": "operador", "clave": "ClavePrueba1"})


if __name__ == "__main__":
    unittest.main()

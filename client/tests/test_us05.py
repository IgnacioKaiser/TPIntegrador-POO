"""Pruebas unitarias del cliente US-05: no requieren servidor real."""
import unittest
from unittest.mock import Mock
from robot_client.cliente import ServiciosPrueba
from robot_client.mensaje import MensajeRPC


class ServiciosUS05Tests(unittest.TestCase):
    def setUp(self):
        self.transporte = Mock()
        self.servicios = ServiciosPrueba(self.transporte, "admin", "ClavePrueba1")

    def _verificar(self, nombre_metodo, operacion):
        getattr(self.servicios, nombre_metodo)("operador")
        self.transporte.enviar.assert_called_once()
        mensaje = self.transporte.enviar.call_args.args[0]
        self.assertIsInstance(mensaje, MensajeRPC)
        self.assertEqual(mensaje.operacion, operacion)
        self.assertEqual(mensaje.parametros["usuario"], "operador")
        self.assertEqual(mensaje.parametros["credenciales"],
                         {"usuario": "admin", "clave": "ClavePrueba1"})
        self.assertEqual(mensaje.codigo, "")
        self.assertEqual(mensaje.resultado, {})

    def test_habilitar_operador(self):
        self._verificar("habilitar_operador", "usuario.habilitar")

    def test_deshabilitar_operador(self):
        self._verificar("deshabilitar_operador", "usuario.deshabilitar")

    def test_ping_sigue_llevando_credenciales(self):
        self.servicios.ping()
        mensaje = self.transporte.enviar.call_args.args[0]
        self.assertEqual(mensaje.operacion, "ping")
        self.assertEqual(set(mensaje.parametros), {"credenciales"})

    def test_eco_sigue_llevando_texto_y_credenciales(self):
        self.servicios.eco("hola")
        mensaje = self.transporte.enviar.call_args.args[0]
        self.assertEqual(mensaje.operacion, "eco")
        self.assertEqual(mensaje.parametros["texto"], "hola")
        self.assertIn("credenciales", mensaje.parametros)


if __name__ == "__main__":
    unittest.main()

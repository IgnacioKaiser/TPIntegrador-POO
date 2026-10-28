"""US-20 y US-21: el cliente crea solicitudes XML-RPC tipadas y autenticadas."""
import unittest

from robot_client.cliente import ServiciosPrueba
from robot_client.mensaje import MensajeRPC


class ClienteSimulado:
    def __init__(self):
        self.solicitudes = []

    def enviar(self, solicitud):
        self.solicitudes.append(solicitud)
        return MensajeRPC.respuesta_a(solicitud, {"mensaje": "OK"})


class TestConsultas(unittest.TestCase):
    def setUp(self):
        self.transporte = ClienteSimulado()
        self.servicios = ServiciosPrueba(self.transporte, "hector", "clave-prueba")

    def test_informe_filtros(self):
        self.servicios.informe_usuarios("martin", "2026-10-01", "2026-10-31", "OK")
        solicitud = self.transporte.solicitudes[-1]
        self.assertEqual(solicitud.operacion, "informe.usuarios")
        self.assertEqual(solicitud.parametros["usuario"], "martin")
        self.assertEqual(solicitud.parametros["desde"], "2026-10-01")
        self.assertEqual(solicitud.parametros["hasta"], "2026-10-31")
        self.assertEqual(solicitud.parametros["codigo"], "OK")
        self.assertEqual(solicitud.parametros["credenciales"]["usuario"], "hector")

    def test_consulta_log_con_paginacion(self):
        self.servicios.consultar_log(tipo="EVENTO", codigo="ERROR_INTERNO", offset=10, limite=20)
        solicitud = self.transporte.solicitudes[-1]
        self.assertEqual(solicitud.operacion, "log.consultar")
        self.assertEqual(solicitud.parametros["tipo"], "EVENTO")
        self.assertEqual(solicitud.parametros["codigo"], "ERROR_INTERNO")
        self.assertEqual(solicitud.parametros["offset"], 10)
        self.assertEqual(solicitud.parametros["limite"], 20)

    def test_omitir_filtros_vacios(self):
        self.servicios.informe_usuarios()
        solicitud = self.transporte.solicitudes[-1]
        self.assertEqual(set(solicitud.parametros), {"credenciales"})


if __name__ == "__main__":
    unittest.main()

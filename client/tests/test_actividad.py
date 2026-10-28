"""US-19: consola, servicios y mensajes reales con transporte simulado."""
from contextlib import redirect_stdout, redirect_stderr
from io import StringIO
import unittest
from unittest.mock import patch

from robot_client.cli import main
from robot_client.mensaje import MensajeRPC


class ActividadClienteTests(unittest.TestCase):
    def ejecutar(self, argumentos, entradas, rol="OPERADOR", codigo="OK", total=0):
        pedidos = []

        def responder(campos):
            s = MensajeRPC.deserializar(campos)
            pedidos.append(s)
            if s.operacion == "usuario.actual":
                return MensajeRPC.respuesta_a(s, {"usuario": "ana", "rol": rol}).serializar()
            datos = {"usuario": "ana", "desde": "2026-10-28T12:00:00Z",
                     "referencia": "inicio", "inicio_actividad": "", "total": total,
                     "errores": 0, "offset": s.parametros['offset'], "mostrados": 0,
                     "registros": [], "codigo_estado": "OK",
                     "estado": {"conectado": False, "posicion": {"disponible": False}}}
            if codigo != "OK":
                datos = {"mensaje": "Acceso rechazado"}
            return MensajeRPC.respuesta_a(s, datos, codigo).serializar()

        salida, errores = StringIO(), StringIO()
        with patch("robot_client.cliente.ServerProxy") as proxy, \
                patch("builtins.input", side_effect=entradas), \
                patch("robot_client.cli.getpass", return_value="ClaveFicticia1"), \
                redirect_stdout(salida), redirect_stderr(errores):
            proxy.return_value.__enter__.return_value.robot.intercambiar.side_effect = responder
            retorno = main(argumentos)
        self.assertNotIn("ClaveFicticia1", salida.getvalue() + errores.getvalue())
        return retorno, salida.getvalue(), errores.getvalue(), pedidos

    def test_comando_paginado_y_sin_actividad(self):
        retorno, salida, errores, pedidos = self.ejecutar(
            ["actividad", "--offset", "10", "--limite", "2"], ["ana"])
        self.assertEqual(retorno, 0)
        self.assertEqual(errores, "")
        self.assertIn("Sin actividad propia", salida)
        self.assertIn("conectado: no", salida)
        self.assertIn("disponible: no", salida)
        self.assertEqual(len(pedidos), 1)
        self.assertEqual(pedidos[0].operacion, "informe.propio")
        self.assertEqual(pedidos[0].parametros, {
            "offset": 10, "limite": 2,
            "credenciales": {"usuario": "ana", "clave": "ClaveFicticia1"}})

    def test_menu_para_ambos_roles_y_pagina_vacia(self):
        for rol in ("OPERADOR", "ADMINISTRADOR"):
            with self.subTest(rol=rol):
                retorno, salida, _, pedidos = self.ejecutar(
                    [], ["ana", "21", "", "", "0"], rol, total=3)
                self.assertEqual(retorno, 0)
                self.assertIn("21. Consultar mi actividad", salida)
                self.assertIn("No hay registros en esta pagina", salida)
                self.assertEqual([p.operacion for p in pedidos], ["usuario.actual", "informe.propio"])
                self.assertEqual(pedidos[1].parametros['offset'], 0)
                self.assertEqual(pedidos[1].parametros['limite'], 50)

    def test_rechazo_y_menu_continua_tras_paginacion_no_numerica(self):
        for codigo in ("NO_AUTENTICADO", "PERMISO_DENEGADO", "VALIDACION", "ERROR_INTERNO"):
            retorno, salida, _, pedidos = self.ejecutar(["actividad"], ["ana"], codigo=codigo)
            self.assertEqual(retorno, 1)
            self.assertIn(f"[{codigo}] Acceso rechazado", salida)
            self.assertEqual(len(pedidos), 1)
        retorno, _, errores, pedidos = self.ejecutar([], ["ana", "21", "no", "0"])
        self.assertEqual(retorno, 0)
        self.assertIn("Error:", errores)
        self.assertEqual(len(pedidos), 1)


if __name__ == "__main__":
    unittest.main()

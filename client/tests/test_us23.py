"""US-23: respuestas simuladas; no abre puertos ni usa bases de usuarios."""

from contextlib import redirect_stderr, redirect_stdout
from io import StringIO
import unittest
from unittest.mock import Mock, patch

from robot_client.cli import main
from robot_client.cliente import (ClienteXmlRpc, ErrorComunicacion,
                                  ServiciosPrueba, TransporteConTimeout)
from robot_client.mensaje import MensajeRPC


def estado_simulado(conectado=False):
    # Desconocido se representa con disponible=False, sin coordenadas inventadas.
    desconocido = {"disponible": False, "mensaje": "Sin datos del firmware."}
    return {"conectado": conectado, "posicion": dict(desconocido),
            "actividad": {nombre: dict(desconocido) for nombre in
                          ("motores", "ventilador", "modo_coordenadas")}}


class ServiciosUS23Tests(unittest.TestCase):
    def test_solicitudes_y_respuestas_de_las_seis_operaciones(self):
        casos = [("conectar", (), {}),
                 ("conectar", ("COM4",), {"puerto": "COM4"}),
                 ("conectar", ("/dev/ttyACM1",), {"puerto": "/dev/ttyACM1"})]
        casos += [(nombre, (), {}) for nombre in
                  ("desconectar", "estado", "activar_motores", "desactivar_motores", "ayuda")]
        for nombre, argumentos, parametros in casos:
            with self.subTest(operacion=nombre, argumentos=argumentos):
                transporte = Mock()
                transporte.enviar.side_effect = lambda s: MensajeRPC.respuesta_a(s)
                servicios = ServiciosPrueba(transporte, "operador", "ClaveFicticia1")
                respuesta = getattr(servicios, nombre)(*argumentos)
                transporte.enviar.assert_called_once()
                pedido = transporte.enviar.call_args.args[0]
                self.assertEqual(pedido.operacion, nombre)
                self.assertEqual(pedido.parametros, {
                    **parametros, "credenciales": {
                        "usuario": "operador", "clave": "ClaveFicticia1"}})
                self.assertEqual(respuesta.id, pedido.id)
                self.assertEqual(respuesta.codigo, "OK")

    def test_timeout_no_reintenta_el_comando(self):
        # Usa el transporte existente y simula el fallo en su unico intento.
        with patch.object(TransporteConTimeout, "single_request",
                          side_effect=TimeoutError("Tiempo agotado")) as intento:
            with self.assertRaises(ErrorComunicacion):
                ServiciosPrueba(ClienteXmlRpc(), "operador", "ClaveFicticia1").activar_motores()
        intento.assert_called_once()


class CliUS23Tests(unittest.TestCase):
    def ejecutar_cli(self, argumentos, resultado, codigo="OK", entradas=None):
        salida, errores = StringIO(), StringIO()

        def responder(campos):
            solicitud = MensajeRPC.deserializar(campos)
            return MensajeRPC.respuesta_a(solicitud, resultado, codigo).serializar()

        # Se sustituye la red, pero se recorren CLI, servicios y MensajeRPC reales.
        with patch("robot_client.cliente.ServerProxy") as proxy, \
                patch("builtins.input", side_effect=entradas or ["operador"]), \
                patch("robot_client.cli.getpass", return_value="ClaveFicticia1"), \
                redirect_stdout(salida), redirect_stderr(errores):
            remoto = proxy.return_value.__enter__.return_value.robot.intercambiar
            remoto.side_effect = responder
            retorno = main(argumentos)
        pedidos = [MensajeRPC.deserializar(c.args[0]) for c in remoto.call_args_list]
        self.assertNotIn("ClaveFicticia1", salida.getvalue() + errores.getvalue())
        return retorno, salida.getvalue(), errores.getvalue(), pedidos

    def test_comandos_de_terminal(self):
        casos = [("conectar",), ("conectar", "COM4"), ("desconectar",),
                 ("estado",), ("activar_motores",), ("desactivar_motores",), ("ayuda",)]
        for argumentos in casos:
            with self.subTest(argumentos=argumentos):
                retorno, salida, errores, pedidos = self.ejecutar_cli(
                    list(argumentos), {"mensaje": "Respuesta de prueba"})
                self.assertEqual(retorno, 0)
                self.assertEqual(errores, "")
                self.assertIn("[OK]", salida)
                self.assertEqual(len(pedidos), 1)
                self.assertEqual(pedidos[0].operacion, argumentos[0])
                extras = {k: v for k, v in pedidos[0].parametros.items() if k != "credenciales"}
                self.assertEqual(extras, {"puerto": "COM4"} if len(argumentos) == 2 else {})

    def test_errores_del_servidor_se_muestran_sin_reintentar(self):
        for codigo in ("PERMISO_DENEGADO", "NO_AUTENTICADO", "ESTADO_INVALIDO",
                       "ERROR_CONEXION", "ERROR_INTERNO"):
            with self.subTest(codigo=codigo):
                retorno, salida, _, pedidos = self.ejecutar_cli(
                    ["conectar"], {"mensaje": "Explicacion del servidor"}, codigo)
                self.assertEqual(retorno, 1)
                self.assertIn(f"[{codigo}] Explicacion del servidor", salida)
                self.assertEqual(len(pedidos), 1)

    def test_estado_desconectado_no_inventa_ceros(self):
        _, salida, _, _ = self.ejecutar_cli(["estado"], estado_simulado())
        self.assertIn("conectado: no", salida)
        self.assertEqual(salida.count("disponible: no"), 4)
        self.assertIn("Sin datos del firmware.", salida)
        self.assertNotIn("x:", salida)
        self.assertNotIn("activo:", salida)

    def test_estado_con_datos_disponibles_y_desconocidos(self):
        datos = estado_simulado(True)
        datos["posicion"] = {"disponible": True, "unidad": "mm",
                             "x": 0.0, "y": -2.5, "z": 10.0, "e": 1.0}
        datos["actividad"]["motores"] = {"disponible": True, "activo": False}
        datos["actividad"]["modo_coordenadas"] = {"disponible": True, "valor": "absoluto"}
        _, salida, _, _ = self.ejecutar_cli(["estado"], datos)
        for texto in ("conectado: si", "x: 0.0", "y: -2.5", "unidad: mm",
                      "motores:\n    disponible: si\n    activo: no", "valor: absoluto"):
            self.assertIn(texto, salida)
        self.assertEqual(salida.count("disponible: no"), 1)

    def test_error_de_estado_conserva_los_datos_parciales(self):
        datos = estado_simulado(True)
        datos["mensaje"] = "Fallo la consulta serie; revise la conexion."
        retorno, salida, _, _ = self.ejecutar_cli(["estado"], datos, "ERROR_INTERNO")
        self.assertEqual(retorno, 1)
        self.assertIn("Fallo la consulta serie", salida)
        self.assertIn("conectado: si", salida)
        self.assertIn("disponible: no", salida)

    def test_ayuda_conserva_las_lineas_del_servidor(self):
        _, salida, _, _ = self.ejecutar_cli(["ayuda"], {"comandos": "conectar\nestado"})
        self.assertIn("comandos: conectar\nestado", salida)

    def test_motores_no_anuncian_una_accion_fisica_terminada(self):
        _, salida, _, _ = self.ejecutar_cli(["activar_motores"], {"respuesta": "OK"})
        self.assertIn("respuesta: OK", salida)
        self.assertIn("no verifica una accion fisica", salida)

    def test_menu_invoca_las_seis_operaciones(self):
        for puerto in ("", "COM4"):
            with self.subTest(puerto=puerto):
                _, salida, _, pedidos = self.ejecutar_cli(
                    [], {}, entradas=["operador", "5", puerto, "6", "7", "8", "9", "10", "0"])
                self.assertIn("Conectar robot (solo administrador)", salida)
                self.assertIn("Desconectar robot (solo administrador)", salida)
                self.assertEqual([s.operacion for s in pedidos], ["conectar", "desconectar",
                                 "estado", "activar_motores", "desactivar_motores", "ayuda"])
                self.assertEqual(pedidos[0].parametros.get("puerto"), puerto or None)

    def test_menu_continua_despues_de_un_rechazo(self):
        _, salida, _, pedidos = self.ejecutar_cli(
            [], {"mensaje": "Solo el administrador puede desconectar."}, "PERMISO_DENEGADO",
            entradas=["operador", "6", "10", "0"])
        self.assertIn("[PERMISO_DENEGADO] Solo el administrador", salida)
        self.assertEqual([s.operacion for s in pedidos], ["desconectar", "ayuda"])

    def test_fallo_de_comunicacion_no_se_presenta_como_exito(self):
        salida, errores = StringIO(), StringIO()
        with patch("robot_client.cliente.TransporteConTimeout.single_request",
                   side_effect=TimeoutError("Tiempo agotado")) as intento, \
                patch("builtins.input", return_value="operador"), \
                patch("robot_client.cli.getpass", return_value="ClaveFicticia1"), \
                redirect_stdout(salida), redirect_stderr(errores):
            retorno = main(["activar_motores"])
        self.assertEqual(retorno, 1)
        self.assertNotIn("[OK]", salida.getvalue())
        self.assertIn("Ejecucion no confirmada", errores.getvalue())
        self.assertNotIn("ClaveFicticia1", salida.getvalue() + errores.getvalue())
        intento.assert_called_once()


if __name__ == "__main__":
    unittest.main()

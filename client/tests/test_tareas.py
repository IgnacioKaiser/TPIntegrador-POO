"""Cliente de tareas: archivos temporales y respuestas simuladas, sin servidor."""
from contextlib import redirect_stderr, redirect_stdout
from io import StringIO
from pathlib import Path
import tempfile
import unittest
from unittest.mock import Mock, patch

from robot_client.cli import main
from robot_client.cliente import (ClienteXmlRpc, ErrorComunicacion, MAX_TAREA_BYTES,
                                  ServiciosPrueba, TransporteConTimeout)
from robot_client.mensaje import MensajeRPC


class TareasClienteTests(unittest.TestCase):
    def setUp(self):
        temporal = tempfile.TemporaryDirectory(prefix="poo_cliente_tareas_")
        self.addCleanup(temporal.cleanup)
        self.carpeta = Path(temporal.name)
        self.archivo = self.carpeta / "Pieza.GCODE"
        self.archivo.write_bytes(b"\xef\xbb\xbfG90\r\nM114\r\n")
        self.transporte = Mock()
        self.transporte.enviar.side_effect = lambda s: MensajeRPC.respuesta_a(s)
        self.servicios = ServiciosPrueba(self.transporte, "ana", "ClaveFicticia1")

    def test_solicitudes_solo_llevan_datos_de_tarea_y_credenciales(self):
        self.servicios.subir_tarea("pieza.nc", "G28")
        pedido = self.transporte.enviar.call_args.args[0]
        self.assertEqual(pedido.operacion, "tarea.subir")
        self.assertEqual(pedido.parametros, {"nombre": "pieza.nc", "contenido": "G28",
                         "credenciales": {"usuario": "ana", "clave": "ClaveFicticia1"}})
        self.transporte.enviar.reset_mock()
        self.servicios.listar_tareas()
        pedido = self.transporte.enviar.call_args.args[0]
        self.assertEqual(pedido.operacion, "tarea.listar")
        self.assertEqual(set(pedido.parametros), {"credenciales"})
        self.transporte.enviar.assert_called_once()

    def test_lectura_utf8_con_bom_envia_nombre_sin_ruta(self):
        self.servicios.subir_archivo(str(self.archivo))
        self.transporte.enviar.assert_called_once()
        datos = self.transporte.enviar.call_args.args[0].parametros
        self.assertEqual(datos["nombre"], "Pieza.GCODE")
        self.assertEqual(datos["contenido"], "G90\r\nM114\r\n")
        self.assertNotIn(str(self.carpeta), datos["nombre"])

    def test_archivo_grande_se_rechaza_antes_de_abrirlo(self):
        self.archivo.write_bytes(b"a" * (MAX_TAREA_BYTES + 1))
        with patch("robot_client.cliente.Path.open") as abrir:
            with self.assertRaisesRegex(ValueError, "1 MiB"):
                self.servicios.subir_archivo(str(self.archivo))
            abrir.assert_not_called()
        self.transporte.enviar.assert_not_called()

    def test_lectura_acotada_si_el_archivo_crece(self):
        with patch("robot_client.cliente.Path.open") as abrir:
            entrada = abrir.return_value.__enter__.return_value
            entrada.read.return_value = b"a" * (MAX_TAREA_BYTES + 1)
            with self.assertRaisesRegex(ValueError, "1 MiB"):
                self.servicios.subir_archivo(str(self.archivo))
            entrada.read.assert_called_once_with(MAX_TAREA_BYTES + 1)
        self.transporte.enviar.assert_not_called()

    def test_errores_locales_no_envian_solicitud(self):
        for ruta in (self.carpeta, self.carpeta / "inexistente.nc"):
            with self.subTest(ruta=ruta.name), self.assertRaises(ValueError):
                self.servicios.subir_archivo(str(ruta))
        self.archivo.write_bytes(b"\xff\xfeG28")
        with self.assertRaisesRegex(ValueError, "UTF-8"):
            self.servicios.subir_archivo(str(self.archivo))
        with patch("robot_client.cliente.Path.open", side_effect=PermissionError):
            with self.assertRaisesRegex(ValueError, "No se pudo leer"):
                self.servicios.subir_archivo(str(self.archivo))
        self.transporte.enviar.assert_not_called()

    def ejecutar_cli(self, argumentos, entradas, rol="OPERADOR", codigo="OK", tareas=None):
        def responder(s):
            if s.operacion == "usuario.actual":
                return MensajeRPC.respuesta_a(s, {"usuario": "ana", "rol": rol})
            if codigo != "OK":
                return MensajeRPC.respuesta_a(s, {"mensaje": "Solicitud rechazada"}, codigo)
            if s.operacion == "tarea.listar":
                return MensajeRPC.respuesta_a(s, {"tareas": [] if tareas is None else tareas})
            return MensajeRPC.respuesta_a(s, {"id": "2:pieza.gcode", "nombre": "pieza.gcode",
                "propietario": "ana", "mensaje": "Tarea almacenada. No se ejecutaron ordenes."})

        self.transporte.enviar.side_effect = responder
        salida, errores = StringIO(), StringIO()
        with patch("robot_client.cli.ClienteXmlRpc", return_value=self.transporte), \
                patch("builtins.input", side_effect=entradas), \
                patch("robot_client.cli.getpass", return_value="ClaveFicticia1"), \
                redirect_stdout(salida), redirect_stderr(errores):
            retorno = main(argumentos)
        self.assertNotIn("ClaveFicticia1", salida.getvalue() + errores.getvalue())
        return retorno, salida.getvalue(), errores.getvalue()

    def test_comandos_subir_y_tareas(self):
        retorno, salida, errores = self.ejecutar_cli(["subir", str(self.archivo)], ["ana"])
        self.assertEqual(retorno, 0)
        self.assertEqual(errores, "")
        self.assertIn("No se ejecutaron ordenes", salida)
        self.assertEqual(self.transporte.enviar.call_args.args[0].operacion, "tarea.subir")
        self.transporte.enviar.reset_mock()
        fila = {"id": "2:pieza.gcode", "nombre": "pieza.gcode", "propietario": "ana"}
        retorno, salida, _ = self.ejecutar_cli(["tareas"], ["ana"], tareas=[fila])
        self.assertEqual(retorno, 0)
        self.assertIn("ID | Nombre | Propietario", salida)
        self.assertIn("2:pieza.gcode | pieza.gcode | ana", salida)
        self.transporte.enviar.assert_called_once()

    def test_menu_de_ambos_perfiles_ofrece_subir_y_listar(self):
        for rol in ("OPERADOR", "ADMINISTRADOR"):
            with self.subTest(rol=rol):
                self.transporte.enviar.reset_mock()
                retorno, salida, errores = self.ejecutar_cli(
                    [], ["ana", "17", str(self.archivo), "18", "0"], rol)
                self.assertEqual(retorno, 0)
                self.assertEqual(errores, "")
                self.assertIn("17. Subir archivo", salida)
                self.assertIn("18. Listar tareas", salida)
                self.assertIn("No hay tareas disponibles", salida)
                self.assertEqual([c.args[0].operacion for c in self.transporte.enviar.call_args_list],
                                 ["usuario.actual", "tarea.subir", "tarea.listar"])

    def test_menu_continua_despues_de_error_local(self):
        retorno, salida, errores = self.ejecutar_cli(
            [], ["ana", "17", str(self.carpeta / "no.nc"), "18", "0"])
        self.assertEqual(retorno, 0)
        self.assertIn("archivo regular existente", errores)
        self.assertIn("No hay tareas disponibles", salida)
        self.assertEqual([c.args[0].operacion for c in self.transporte.enviar.call_args_list],
                         ["usuario.actual", "tarea.listar"])

    def test_rechazo_del_servidor_y_listado_mal_formado(self):
        retorno, salida, _ = self.ejecutar_cli(["subir", str(self.archivo)], ["ana"], codigo="ESTADO_INVALIDO")
        self.assertEqual(retorno, 1)
        self.assertIn("[ESTADO_INVALIDO] Solicitud rechazada", salida)
        retorno, _, errores = self.ejecutar_cli(["tareas"], ["ana"], tareas=[{"nombre": "incompleta.nc"}])
        self.assertEqual(retorno, 1)
        self.assertIn("listado de tareas valido", errores)

    def test_tareas_no_reintentan_ante_timeout(self):
        for metodo, argumentos in (("subir_tarea", ("pieza.nc", "G28")), ("listar_tareas", ())):
            with self.subTest(metodo=metodo):
                with patch.object(TransporteConTimeout, "single_request", side_effect=TimeoutError) as intento:
                    with self.assertRaises(ErrorComunicacion):
                        getattr(ServiciosPrueba(ClienteXmlRpc(), "ana", "ClaveFicticia1"), metodo)(*argumentos)
                intento.assert_called_once()


if __name__ == "__main__":
    unittest.main()

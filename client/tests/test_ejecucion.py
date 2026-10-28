"""US-17: preparacion antes de red, comandos y menu con transporte sustituido."""
from contextlib import redirect_stdout, redirect_stderr
from io import StringIO
import unittest
from unittest.mock import Mock, patch

from robot_client.cli import main
from robot_client.cliente import ServiciosPrueba, ErrorComunicacion
from robot_client.mensaje import MensajeRPC


def progreso(id="intento", estado="finalizada", registro=True):
    return {"ejecucion_id":id,"tarea_id":"2:pieza.nc","estado":estado,"total":3,"confirmadas":2,
            "orden_actual":3,"linea_actual":5,"resultado_orden":"desconocido" if estado=="fallida" else "confirmada",
            "codigo_ejecucion":"ERROR_INTERNO" if estado=="fallida" else "OK",
            "mensaje":"Mensaje de prueba","registro_completo":registro}


class EjecucionClienteTests(unittest.TestCase):
    def test_id_disponible_antes_de_enviar(self):
        cliente=Mock()
        servicios=ServiciosPrueba(cliente,"ana","ClavePrueba1")
        s=servicios.preparar_ejecucion("2:pieza.nc")
        self.assertTrue(s.id)
        cliente.enviar.assert_not_called()
        self.assertEqual(s.parametros['credenciales']['usuario'],"ana")
        servicios.ejecutar_tarea(s)
        cliente.enviar.assert_called_once_with(s)
        servicios.consultar_ejecucion(s.id)
        self.assertEqual(cliente.enviar.call_args.args[0].parametros['ejecucion_id'],s.id)

    def test_perdida_de_respuesta_conserva_id_y_no_reintenta(self):
        salida, errores=StringIO(),StringIO()
        def perder(s):
            self.assertIn("ID de ejecucion: "+s.id,salida.getvalue())
            self.assertIn("ejecucion "+s.id,salida.getvalue())
            raise ErrorComunicacion("Respuesta perdida")
        with patch("robot_client.cli.getpass",return_value="ClavePrueba1"), \
             patch("builtins.input",return_value="ana"), \
             patch("robot_client.cliente.ClienteXmlRpc.enviar",side_effect=perder) as enviar, \
             redirect_stdout(salida),redirect_stderr(errores):
            self.assertEqual(main(["ejecutar","2:pieza.nc"]),1)
        enviar.assert_called_once()
        self.assertIn("No se reintenta",errores.getvalue())
        self.assertNotIn("ClavePrueba1",salida.getvalue()+errores.getvalue())

    def test_menu_de_ambos_perfiles(self):
        for rol in ("OPERADOR","ADMINISTRADOR"):
            pedidos=[]
            def responder(s):
                pedidos.append(s)
                if s.operacion=="usuario.actual": return MensajeRPC.respuesta_a(s,{"usuario":"ana","rol":rol})
                return MensajeRPC.respuesta_a(s,progreso(s.id,"aceptada" if s.operacion=="tarea.ejecutar" else "finalizada"))
            salida=StringIO()
            with patch("robot_client.cli.getpass",return_value="ClavePrueba1"), \
                 patch("builtins.input",side_effect=["ana","22","2:pieza.nc","23","intento","0"]), \
                 patch("robot_client.cliente.ClienteXmlRpc.enviar",side_effect=responder),redirect_stdout(salida):
                self.assertEqual(main([]),0)
            self.assertEqual([s.operacion for s in pedidos],["usuario.actual","tarea.ejecutar","tarea.estado"])
            self.assertIn("22. Ejecutar tarea",salida.getvalue())
            self.assertIn("23. Consultar ejecucion",salida.getvalue())

    def test_consulta_fallida_y_advertencia_de_registro(self):
        salida=StringIO()
        def responder(s): return MensajeRPC.respuesta_a(s,progreso(estado="fallida",registro=False))
        with patch("robot_client.cli.getpass",return_value="ClavePrueba1"), \
             patch("builtins.input",return_value="ana"), \
             patch("robot_client.cliente.ClienteXmlRpc.enviar",side_effect=responder),redirect_stdout(salida):
            self.assertEqual(main(["ejecucion","intento"]),1)
        self.assertIn("fallida [ERROR_INTERNO]",salida.getvalue())
        self.assertIn("desconocido",salida.getvalue())
        self.assertIn("registro de ejecucion esta incompleto",salida.getvalue())


if __name__=="__main__": unittest.main()

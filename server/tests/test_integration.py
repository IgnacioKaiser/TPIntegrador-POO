"""Pruebas extremo a extremo contra el ejecutable C++ real (Windows y Linux)."""

from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import os
import socket
import subprocess
import sys
import tempfile
import threading
import time
import unittest
from unittest.mock import patch
from xmlrpc.client import Fault, ServerProxy

from robot_client.cliente import ClienteXmlRpc, ErrorComunicacion, ServiciosPrueba
from robot_client.mensaje import MensajeRPC

ROOT = Path(__file__).resolve().parents[2]
SERVIDOR = ROOT / "server" / "build" / (
    "robot_server.exe" if os.name == "nt" else "robot_server")


class IntegracionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        # Elegir un puerto disponible para no interferir con otros procesos.
        with socket.socket() as reserva:
            reserva.bind(("127.0.0.1", 0))
            cls.puerto = reserva.getsockname()[1]

        cls.url = f"http://127.0.0.1:{cls.puerto}/RPC2"
        cls.salida = tempfile.TemporaryFile(mode="w+")
        cls.proceso = subprocess.Popen(
            [str(SERVIDOR), "--port", str(cls.puerto)],
            stdout=cls.salida, stderr=subprocess.STDOUT,
        )
        cls.addClassCleanup(cls.cerrar)
        cls.cliente = ClienteXmlRpc(cls.url, timeout=1)
        cls.servicios = ServiciosPrueba(cls.cliente)

        limite = time.monotonic() + 5
        while time.monotonic() < limite:
            if cls.proceso.poll() is not None:
                break
            try:
                cls.servicios.ping()
                return
            except ErrorComunicacion:
                threading.Event().wait(0.05)
        cls.salida.seek(0)
        raise RuntimeError("El servidor no arranco: " + cls.salida.read())

    @classmethod
    def cerrar(cls):
        if cls.proceso.poll() is None:
            cls.proceso.terminate()
            try:
                cls.proceso.wait(timeout=7)
            except subprocess.TimeoutExpired:
                cls.proceso.kill()
                cls.proceso.wait()
                cls.salida.close()
                raise AssertionError("El servidor no termino al solicitar su cierre.")
        cls.salida.close()

    def test_ping(self):
        respuesta = self.servicios.ping()
        self.assertEqual(respuesta.codigo, "OK")
        self.assertEqual(respuesta.tipo, "respuesta")
        self.assertEqual(respuesta.parametros, {})
        self.assertIn("pong", respuesta.resultado["mensaje"])

    def test_eco_utf8_y_escape_xml(self):
        texto = 'Mecatronica: áéíóú ñ <robot X="100"> & pinza\nSegunda linea'
        self.assertEqual(self.servicios.eco(texto).resultado["texto"], texto)

    def test_eco_vacio(self):
        self.assertEqual(self.servicios.eco("").resultado["texto"], "")

    def test_operacion_desconocida_es_error_de_aplicacion(self):
        respuesta = self.cliente.enviar(MensajeRPC.solicitud("mover"))
        self.assertEqual(respuesta.codigo, "OPERACION_NO_SOPORTADA")
        self.assertIn("mensaje", respuesta.resultado)

    def test_error_de_parametros_es_respuesta_normal(self):
        for operacion, parametros in (
            ("ping", {"extra": 1}),
            ("eco", {}),
            ("eco", {"texto": 123}),
            ("eco", {"texto": "hola", "extra": True}),
        ):
            with self.subTest(operacion=operacion, parametros=parametros):
                respuesta = self.cliente.enviar(
                    MensajeRPC.solicitud(operacion, parametros)
                )
                self.assertEqual(respuesta.codigo, "VALIDACION")
                self.assertIn("mensaje", respuesta.resultado)
        self.assertEqual(self.servicios.ping().codigo, "OK")

    def test_argumentos_xmlrpc_invalidos_y_recuperacion(self):
        for argumentos in ((), ("texto",), ({}, {})):
            with self.subTest(argumentos=argumentos), ServerProxy(self.url) as remoto:
                with self.assertRaises(Fault):
                    remoto.robot.intercambiar(*argumentos)
        self.assertEqual(self.servicios.ping().codigo, "OK")

    def test_mensajes_invalidos_son_fault(self):
        base = MensajeRPC.solicitud("eco", {"texto": "OK"}).serializar()
        invalidos = [
            base | {"id": ""},
            base | {"tipo": "respuesta", "codigo": "OK"},
            base | {"version": "2"},
            base | {"extra": "campo"},
            base | {"parametros": "no-es-struct"},
            base | {"resultado": {"mensaje": "no corresponde"}},
            {k: v for k, v in base.items() if k != "resultado"},
        ]
        for campos in invalidos:
            with self.subTest(campos=list(campos)), ServerProxy(self.url) as remoto:
                with self.assertRaises(Fault):
                    remoto.robot.intercambiar(campos)
        self.assertEqual(self.servicios.ping().codigo, "OK")

    def test_introspeccion(self):
        with ServerProxy(self.url) as remoto:
            self.assertIn("robot.intercambiar", remoto.system.listMethods())
            self.assertIn("MensajeRPC", remoto.system.methodHelp("robot.intercambiar"))

    def test_metodo_desconocido(self):
        with ServerProxy(self.url) as remoto, self.assertRaises(Fault):
            remoto.robot.inexistente()

    def test_puerto_ocupado(self):
        resultado = subprocess.run(
            [str(SERVIDOR), "--port", str(self.puerto)],
            capture_output=True, text=True, timeout=3,
        )
        self.assertNotEqual(resultado.returncode, 0)
        self.assertIn("No se pudo escuchar", resultado.stderr)

    def test_escucha_local(self):
        with self.assertRaises(OSError):
            with socket.create_connection(("127.0.0.2", self.puerto), timeout=0.2):
                pass

    def test_clientes_concurrentes_no_mezclan_respuestas(self):
        def enviar(indice):
            servicio = ServiciosPrueba(ClienteXmlRpc(self.url, timeout=5))
            return servicio.eco(f"cliente-{indice}").resultado["texto"]
        with ThreadPoolExecutor(max_workers=4) as pool:
            resultados = list(pool.map(enviar, range(12)))
        self.assertEqual(resultados, [f"cliente-{i}" for i in range(12)])

    def test_cliente_rechaza_id_ajeno(self):
        solicitud = MensajeRPC.solicitud("ping")
        respuesta = MensajeRPC.respuesta_a(solicitud, {"mensaje": "pong"}).serializar()
        respuesta["id"] = "otro"
        with patch("robot_client.cliente.ServerProxy") as remoto:
            remoto.return_value.__enter__.return_value.robot.intercambiar.return_value = respuesta
            with self.assertRaises(ErrorComunicacion):
                self.cliente.enviar(solicitud)

    def test_servidor_ausente(self):
        with socket.socket() as reserva:
            reserva.bind(("127.0.0.1", 0))
            url = f"http://127.0.0.1:{reserva.getsockname()[1]}/RPC2"
            with self.assertRaises(ErrorComunicacion):
                ServiciosPrueba(ClienteXmlRpc(url, timeout=0.2)).ping()

    def test_timeout_sin_reintento(self):
        with socket.socket() as listener:
            listener.bind(("127.0.0.1", 0))
            listener.listen(1)
            url = f"http://127.0.0.1:{listener.getsockname()[1]}/RPC2"
            terminado = threading.Event()

            def aceptar():
                conexion, _ = listener.accept()
                with conexion:
                    terminado.wait(2)

            hilo = threading.Thread(target=aceptar, daemon=True)
            hilo.start()
            inicio = time.monotonic()
            try:
                with self.assertRaises(ErrorComunicacion):
                    ServiciosPrueba(ClienteXmlRpc(url, timeout=0.15)).ping()
                self.assertLess(time.monotonic() - inicio, 1.5)
            finally:
                terminado.set()
                hilo.join(timeout=3)

    def test_cli_real(self):
        resultado = subprocess.run(
            [sys.executable, "-m", "robot_client", "--url", self.url,
             "eco", "Hola desde CLI"],
            cwd=ROOT / "client", capture_output=True, text=True, timeout=5,
        )
        self.assertEqual(resultado.returncode, 0, resultado.stderr)
        self.assertIn("[OK] Hola desde CLI", resultado.stdout)


if __name__ == "__main__":
    unittest.main()

"""Python -> XML-RPC -> servidor C++ real con CanalPrueba. Solo datos temporales."""
from contextlib import redirect_stdout
from io import StringIO
from pathlib import Path
import os
import shutil
import socket
import subprocess
import sys
import tempfile
import time
import unittest
from unittest.mock import patch

from robot_client.cli import main
from robot_client.cliente import ClienteXmlRpc, ServiciosPrueba, ErrorComunicacion
from robot_client.mensaje import MensajeRPC

ROOT = Path(__file__).resolve().parents[2]
BUILD = ROOT / "server/build"
EXE = ".exe" if os.name == "nt" else ""


class EjecucionRpcTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix="poo_us17_rpc_")
        cls.addClassCleanup(cls.temp.cleanup)
        cls.base = Path(cls.temp.name) / "usuarios.sqlite"
        for accion, entrada in (("inicializar", "admin\nClaveAdmin1\nClaveAdmin1\n"),
                                ("agregar", "admin\nClaveAdmin1\nana\nClaveAna1\nClaveAna1\n"),
                                ("agregar", "admin\nClaveAdmin1\nbruno\nClaveBruno1\nClaveBruno1\n")):
            subprocess.run([str(BUILD / ("usuarios_admin" + EXE)), accion, "--db", str(cls.base)],
                           input=entrada, text=True, capture_output=True, check=True, timeout=15)

    def iniciar(self, caso="normal"):
        carpeta = Path(self.temp.name) / self._testMethodName / caso
        carpeta.mkdir(parents=True)
        base = carpeta / "usuarios.sqlite"
        shutil.copy2(self.base, base)
        with socket.socket() as s:
            s.bind(("127.0.0.1", 0))
            puerto = s.getsockname()[1]
        salida = tempfile.TemporaryFile(mode="w+")
        proceso = subprocess.Popen([str(BUILD / ("servidor_us17" + EXE)), str(base),
                                   str(carpeta), str(puerto), caso], stdout=salida, stderr=subprocess.STDOUT)

        def cerrar():
            (carpeta / "liberar").touch()
            if proceso.poll() is None:
                proceso.terminate()  # Limpieza del fixture; el join ordenado se prueba en C++.
                proceso.wait(timeout=10)
            salida.close()
        self.addCleanup(cerrar)
        cliente = ClienteXmlRpc(f"http://127.0.0.1:{puerto}/RPC2", timeout=2)
        self.cliente, self.carpeta = cliente, carpeta
        self.admin = ServiciosPrueba(cliente, "admin", "ClaveAdmin1")
        self.ana = ServiciosPrueba(cliente, "ana", "ClaveAna1")
        self.bruno = ServiciosPrueba(cliente, "bruno", "ClaveBruno1")
        limite = time.monotonic() + 7
        while time.monotonic() < limite:
            try:
                if self.admin.ping().codigo == "OK": break
            except ErrorComunicacion:
                time.sleep(.02)
        else:
            salida.seek(0)
            self.fail("El servidor sustituido no arranco: " + salida.read())

    def preparar(self, contenido="M3\nM5\nM3"):
        tarea = self.ana.subir_tarea("pieza.nc", contenido)
        self.assertEqual(tarea.codigo, "OK")
        self.tarea_id = tarea.resultado['id']
        self.assertEqual(self.admin.conectar("CANAL_SUSTITUIDO").codigo, "OK")
        self.assertEqual(self.ana.seleccionar_modo_trabajo("automatico").codigo, "OK")
        return self.tarea_id

    def empezar(self, servicios=None):
        servicios = servicios or self.ana
        s = servicios.preparar_ejecucion(self.tarea_id)
        r = servicios.ejecutar_tarea(s)
        self.assertEqual(r.codigo, "OK")
        self.assertEqual(r.resultado['ejecucion_id'], s.id)
        self.assertEqual(r.resultado['estado'], "aceptada")
        return s.id

    def esperar(self, id, servicios=None):
        servicios = servicios or self.ana
        limite = time.monotonic() + 7
        while time.monotonic() < limite:
            r = servicios.consultar_ejecucion(id)
            self.assertEqual(r.codigo, "OK")
            if r.resultado['estado'] in ('finalizada', 'fallida'): return r.resultado
            time.sleep(.01)
        self.fail("No termino la ejecucion")

    def trafico(self):
        return (self.carpeta / "trafico.txt").read_text().split()

    def test_secuencia_y_fallos_sin_reintentos(self):
        for caso, confirmadas, estado_orden in (("normal",3,"confirmada"), ("error2",1,"error_controlador"),
                                               ("error3",2,"error_controlador"), ("timeout3",2,"desconocido")):
            with self.subTest(caso=caso):
                self.iniciar(caso); self.preparar()
                id = self.empezar()
                p = self.esperar(id)
                self.assertEqual(p['estado'], "finalizada" if caso == "normal" else "fallida")
                self.assertEqual(p['confirmadas'], confirmadas)
                self.assertEqual(p['resultado_orden'], estado_orden)
                self.assertEqual(self.trafico(), ["M3", "M5", "M3"][:(3 if caso != "error2" else 2)])
                log = self.admin.consultar_log(tipo="EVENTO").resultado['registros']
                eventos = [r for r in log if r['id_solicitud'] == id]
                self.assertEqual([r['operacion'] for r in eventos],
                                 ['tarea.aceptada', 'tarea.finalizada' if caso == 'normal' else 'tarea.fallida'])
                contenido = (self.carpeta / "actividad.csv").read_text(encoding='utf-8')
                for secreto in ("ClaveAna1", "ClaveAdmin1", "$argon2", "M3\nM5"):
                    self.assertNotIn(secreto, contenido)

    def test_reserva_privacidad_y_deshabilitacion(self):
        self.iniciar("bloqueada"); self.preparar()
        id = self.empezar()
        limite = time.monotonic() + 3
        while not (self.carpeta / "bloqueada").exists() and time.monotonic() < limite: time.sleep(.01)
        self.assertTrue((self.carpeta / "bloqueada").exists())
        antes = self.trafico()
        self.assertEqual(self.bruno.consultar_ejecucion(id).codigo, "PERMISO_DENEGADO")
        self.assertEqual(self.admin.consultar_ejecucion(id).codigo, "OK")
        self.assertEqual(self.admin.ejecutar_tarea(self.admin.preparar_ejecucion(self.tarea_id)).codigo, "ESTADO_INVALIDO")
        for op in ("desconectar", "desactivar_motores", "ir_a_origen", "activar_efector", "seleccionar_modo_trabajo"):
            r = self.cliente.enviar(MensajeRPC.solicitud(op, {"credenciales": {"usuario":"admin", "clave":"ClaveAdmin1"}}))
            self.assertEqual(r.codigo, "ESTADO_INVALIDO")
        self.assertTrue(self.bruno.estado().resultado['datos_anteriores'])
        informe = self.bruno.actividad_propia().resultado
        self.assertTrue(informe['estado']['datos_anteriores'])
        self.assertNotIn(self.tarea_id, str(informe))
        self.assertNotIn(id, str(informe))
        self.assertEqual(self.trafico(), antes)
        self.assertEqual(self.admin.deshabilitar_operador("ana").codigo,"OK")
        try:
            self.assertEqual(self.ana.consultar_ejecucion(id).codigo,"PERMISO_DENEGADO")
            (self.carpeta/"liberar").touch()
            self.assertEqual(self.esperar(id,self.admin)['confirmadas'],3)
        finally:
            self.assertEqual(self.admin.habilitar_operador("ana").codigo,"OK")

    def test_permisos_formato_y_condiciones(self):
        self.iniciar()
        self.tarea_id = self.ana.subir_tarea("pieza.nc","M3").resultado['id']
        self.assertEqual(self.ana.ejecutar_tarea(self.ana.preparar_ejecucion(self.tarea_id)).codigo,"ESTADO_INVALIDO")
        self.admin.conectar("SUSTITUTO")
        self.assertEqual(self.ana.ejecutar_tarea(self.ana.preparar_ejecucion(self.tarea_id)).codigo,"ESTADO_INVALIDO")
        self.ana.seleccionar_modo_trabajo("automatico")
        self.assertEqual(self.bruno.ejecutar_tarea(self.bruno.preparar_ejecucion(self.tarea_id)).codigo,"PERMISO_DENEGADO")
        for id, codigo in (("2:ausente.nc","RECURSO_NO_ENCONTRADO"), ("../x.nc","VALIDACION")):
            self.assertEqual(self.ana.ejecutar_tarea(self.ana.preparar_ejecucion(id)).codigo,codigo)
        invalida = self.ana.subir_tarea("invalida.nc","G999 X1").resultado['id']
        self.assertEqual(self.ana.ejecutar_tarea(self.ana.preparar_ejecucion(invalida)).codigo,"VALIDACION")
        for op, parametros in (("tarea.ejecutar",{"tarea_id":self.tarea_id,"usuario":"ana"}),
                               ("tarea.ejecutar",{"tarea_id":7}), ("tarea.estado",{"ejecucion_id":"../x"})):
            r = self.cliente.enviar(MensajeRPC.solicitud(op,{**parametros,"credenciales":{"usuario":"ana","clave":"ClaveAna1"}}))
            self.assertEqual(r.codigo,"VALIDACION")
        mala = ServiciosPrueba(self.cliente,"ana","Incorrecta")
        self.assertEqual(mala.ejecutar_tarea(mala.preparar_ejecucion(self.tarea_id)).codigo,"NO_AUTENTICADO")
        self.assertEqual(mala.consultar_ejecucion("intento").codigo,"NO_AUTENTICADO")
        id = self.empezar(self.admin)
        self.assertEqual(self.esperar(id,self.admin)['estado'],"finalizada")
        self.assertEqual(self.ana.consultar_ejecucion(id).codigo,"PERMISO_DENEGADO")

    def test_cli_y_menu_reales(self):
        self.iniciar(); self.preparar()
        salida=StringIO()
        with patch("builtins.input",return_value="ana"), patch("robot_client.cli.getpass",return_value="ClaveAna1"), redirect_stdout(salida):
            retorno=main(["--url",self.cliente.url,"ejecutar",self.tarea_id])
        self.assertEqual(retorno,0)
        id=salida.getvalue().split("ID de ejecucion: ")[1].splitlines()[0]
        self.esperar(id)
        with patch("builtins.input",return_value="ana"), patch("robot_client.cli.getpass",return_value="ClaveAna1"), redirect_stdout(salida):
            self.assertEqual(main(["--url",self.cliente.url,"ejecucion",id]),0)
        self.assertIn("Ordenes confirmadas: 3/3",salida.getvalue())
        for usuario,clave in (("ana","ClaveAna1"),("admin","ClaveAdmin1")):
            salida=StringIO()
            with patch("builtins.input",side_effect=[usuario,"22",self.tarea_id,"23","menu_"+usuario,"0"]), \
                 patch("robot_client.mensaje.uuid4") as uuid, \
                 patch("robot_client.cli.getpass",return_value=clave),redirect_stdout(salida):
                uuid.return_value.hex = "menu_"+usuario
                self.assertEqual(main(["--url",self.cliente.url]),0)
            self.assertIn("22. Ejecutar tarea",salida.getvalue())
            self.assertIn("23. Consultar ejecucion",salida.getvalue())
            self.assertNotIn(clave,salida.getvalue())

    def test_movimiento_con_controlador_programado(self):
        self.iniciar(); self.preparar("M17\nG90\nG1 X1\nM5")
        p = self.esperar(self.empezar())
        self.assertEqual(p['estado'],"finalizada")
        self.assertEqual(p['confirmadas'],4)
        self.assertEqual(self.trafico().count("G1"),1)
        self.assertEqual(self.trafico().count("M114"),2)


if __name__ == "__main__": unittest.main()

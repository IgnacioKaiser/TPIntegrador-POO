"""Pruebas extremo a extremo contra el ejecutable C++ real (Windows y Linux)."""

from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import os
import sqlite3
from contextlib import closing
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


CREDENCIALES = {"usuario": "operador", "clave": "ClaveOperador1"}
ADMIN_CREDENCIALES = {"usuario": "admin", "clave": "ClaveAdmin1"}
BRUNO_CREDENCIALES = {"usuario": "bruno", "clave": "ClaveBruno1"}

def solicitud(operacion, parametros=None, credenciales=CREDENCIALES):
    return MensajeRPC.solicitud(
        operacion, {**(parametros or {}), "credenciales": credenciales}
    )

class IntegracionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        # Elegir un puerto disponible para no interferir con otros procesos.
        with socket.socket() as reserva:
            reserva.bind(("127.0.0.1", 0))
            cls.puerto = reserva.getsockname()[1]

        cls.url = f"http://127.0.0.1:{cls.puerto}/RPC2"
        cls.temporal = tempfile.TemporaryDirectory(prefix="poo_rpc_auth_")
        cls.addClassCleanup(cls.temporal.cleanup)
        cls.base = Path(cls.temporal.name) / "usuarios.sqlite"
        cls.tareas = Path(cls.temporal.name) / "tareas"
        herramienta = ROOT / "server/build" / ("usuarios_admin.exe" if os.name == "nt" else "usuarios_admin")
        for accion, datos in (
            ("inicializar", "admin\nClaveAdmin1\nClaveAdmin1\n"),
            ("agregar", "admin\nClaveAdmin1\noperador\nClaveOperador1\nClaveOperador1\n"),
            ("agregar", "admin\nClaveAdmin1\nbruno\nClaveBruno1\nClaveBruno1\n"),
            ("agregar", "admin\nClaveAdmin1\nvacio\nClaveVacio1\nClaveVacio1\n"),
        ):
            subprocess.run([str(herramienta), accion, "--db", str(cls.base)],
                           input=datos, text=True, capture_output=True, check=True, timeout=15)
        cls.salida = tempfile.TemporaryFile(mode="w+")
        cls.addClassCleanup(cls.cerrar)
        cls.cliente = ClienteXmlRpc(cls.url, timeout=1)
        cls.servicios = ServiciosPrueba(cls.cliente, **CREDENCIALES)
        cls.iniciar_servidor()

    @classmethod
    def iniciar_servidor(cls):
        cls.proceso = subprocess.Popen(
            [str(SERVIDOR), "--port", str(cls.puerto), "--db", str(cls.base),
             "--tareas-dir", str(cls.tareas)],
            stdout=cls.salida, stderr=subprocess.STDOUT,
        )
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

    def test_actividad_propia_aislamiento_detalle_y_sin_actividad(self):
        # Este usuario no envio pedidos antes de su primera consulta.
        nueva = ServiciosPrueba(self.cliente, "vacio", "ClaveVacio1")
        vacia = nueva.actividad_propia()
        self.assertEqual(vacia.codigo, "OK")
        self.assertEqual(vacia.resultado['total'], 0)
        self.assertEqual(vacia.resultado['errores'], 0)
        self.assertEqual(vacia.resultado['registros'], [])
        self.assertEqual(vacia.resultado['inicio_actividad'], "")
        self.assertEqual(vacia.resultado['referencia'], "inicio")
        self.assertFalse(vacia.resultado['estado']['conectado'])
        self.assertFalse(vacia.resultado['estado']['posicion']['disponible'])
        segunda = nueva.actividad_propia()
        self.assertEqual([r['id_solicitud'] for r in segunda.resultado['registros']], [vacia.id])

        primero = self.servicios.eco("contenido_privado_us19")
        rechazo = self.servicios.ir_a_origen()  # Desconectado: error de operacion propio.
        bruno = ServiciosPrueba(self.cliente, **BRUNO_CREDENCIALES)
        ajena = bruno.ping()
        for servicios, nombre, esperado in ((self.servicios, "operador", primero.id),
                                            (bruno, "bruno", ajena.id)):
            datos = servicios.actividad_propia(limite=100).resultado
            self.assertEqual(datos['usuario'], nombre)
            self.assertTrue(all(r['usuario'] == nombre for r in datos['registros']))
            ids = [r['id_solicitud'] for r in datos['registros']]
            self.assertIn(esperado, ids)
            self.assertNotIn(ajena.id if nombre == "operador" else primero.id, ids)
            self.assertNotIn("contenido_privado_us19", str(datos))
            self.assertNotIn("credenciales", str(datos))
            self.assertEqual(datos['codigo_estado'], "OK")
        propia = self.servicios.actividad_propia(limite=100).resultado
        self.assertIn(rechazo.id, [r['id_solicitud'] for r in propia['registros'] if r['codigo'] != 'OK'])
        self.assertGreaterEqual(propia['errores'], 1)
        pagina = self.servicios.actividad_propia(offset=1, limite=1).resultado
        self.assertEqual(pagina['registros'], propia['registros'][1:2])
        self.assertEqual(pagina['mostrados'], 1)
        self.assertEqual(pagina['offset'], 1)
        self.assertEqual(self.servicios.actividad_propia(offset=100000000).resultado['registros'], [])

        administrador = ServiciosPrueba(self.cliente, **ADMIN_CREDENCIALES)
        antes = administrador.informe_usuarios(usuario="bruno").resultado
        admin = administrador.actividad_propia().resultado
        self.assertEqual(admin['usuario'], 'admin')
        self.assertTrue(all(r['usuario'] == 'admin' for r in admin['registros']))
        self.assertEqual(administrador.informe_usuarios(usuario="bruno").resultado, antes)
        self.assertGreater(administrador.consultar_log(usuario="bruno").resultado['total'], 0)
        for op in ("log.consultar", "informe.usuarios"):
            self.assertEqual(self.cliente.enviar(solicitud(op)).codigo, "PERMISO_DENEGADO")

        import csv
        with (Path(self.temporal.name) / "actividad.csv").open(encoding='utf-8', newline='') as f:
            filas = list(csv.DictReader(f))
        self.assertTrue(any(r['id_solicitud'] == primero.id for r in filas))
        for secreto in ("contenido_privado_us19", "ClaveOperador1", "ClaveBruno1", "$argon2"):
            self.assertNotIn(secreto, str(filas))

    def test_actividad_rechaza_identidades_filtros_y_credenciales(self):
        for parametros in ({"usuario": "bruno"}, {"usuario": "operador"}, {"rol": "ADMINISTRADOR"},
                           {"usuario_id": 2}, {"desde": "2026-10-28"}, {"codigo": "OK"},
                           {"tipo": "PETICION"}, {"limite": 0}, {"limite": 101},
                           {"offset": -1}, {"offset": 100000001}, {"offset": True},
                           {"limite": 1.5}, {"offset": "0"}):
            with self.subTest(parametros=parametros):
                respuesta = self.cliente.enviar(solicitud("informe.propio", parametros))
                self.assertEqual(respuesta.codigo, "VALIDACION")
                self.assertEqual(set(respuesta.resultado), {"mensaje"})
        for credenciales, codigo in (({"usuario": "operador", "clave": "Incorrecta"}, "NO_AUTENTICADO"),
                                     ({"usuario": "nadie", "clave": "Incorrecta"}, "NO_AUTENTICADO"),
                                     ({"usuario": 5, "clave": "Incorrecta"}, "VALIDACION")):
            respuesta = self.cliente.enviar(solicitud("informe.propio", credenciales=credenciales))
            self.assertEqual(respuesta.codigo, codigo)
            self.assertEqual(set(respuesta.resultado), {"mensaje"})
        self.assertEqual(self.cliente.enviar(MensajeRPC.solicitud("informe.propio")).codigo, "NO_AUTENTICADO")
        administrador = ServiciosPrueba(self.cliente, **ADMIN_CREDENCIALES)
        self.assertEqual(administrador.deshabilitar_operador("bruno").codigo, "OK")
        try:
            bruno = ServiciosPrueba(self.cliente, **BRUNO_CREDENCIALES)
            self.assertEqual(bruno.actividad_propia().codigo, "PERMISO_DENEGADO")
        finally:
            self.assertEqual(administrador.habilitar_operador("bruno").codigo, "OK")
        intentos = administrador.consultar_log(codigo="NO_AUTENTICADO").resultado['registros']
        self.assertTrue(intentos)
        self.assertTrue(all(r['usuario'] == "" for r in intentos))

    def test_actividad_reinicio_y_cliente_real(self):
        pedido = self.servicios.ping()
        self.proceso.terminate()
        self.proceso.wait(timeout=7)
        self.iniciar_servidor()
        datos = self.servicios.actividad_propia().resultado
        self.assertEqual(datos['referencia'], "inicio")
        self.assertNotIn(pedido.id, [r['id_solicitud'] for r in datos['registros']])
        administrador = ServiciosPrueba(self.cliente, **ADMIN_CREDENCIALES)
        self.assertIn(pedido.id, [r['id_solicitud'] for r in
                      administrador.consultar_log(usuario="operador", limite=100).resultado['registros']])
        for nombre, clave in (("operador", "ClaveOperador1"), ("bruno", "ClaveBruno1"),
                              ("admin", "ClaveAdmin1")):
            for argumentos, opciones in ((["actividad", "--limite", "2"], ""),
                                         ([], "21\n0\n2\n0\n")):
                with self.subTest(usuario=nombre, argumentos=argumentos):
                    resultado = subprocess.run(
                        [sys.executable, "-B", "-c",
                         "import sys; import robot_client.cli as c; "
                         "c.getpass=lambda _:sys.stdin.readline().rstrip('\\n'); sys.exit(c.main())",
                         "--url", self.url, *argumentos], cwd=ROOT / "client",
                        input=f"{nombre}\n{clave}\n{opciones}", capture_output=True, text=True, timeout=7)
                    self.assertEqual(resultado.returncode, 0, resultado.stderr)
                    self.assertIn(f"Actividad propia de {nombre}", resultado.stdout)
                    self.assertNotIn(clave, resultado.stdout + resultado.stderr)

    def test_ping(self):
        respuesta = self.servicios.ping()
        self.assertEqual(respuesta.codigo, "OK")
        self.assertEqual(respuesta.tipo, "respuesta")
        self.assertEqual(respuesta.parametros, {})
        self.assertIn("pong", respuesta.resultado["mensaje"])

    def test_usuario_actual_devuelve_solo_la_identidad_autenticada(self):
        for credenciales, rol in ((CREDENCIALES, "OPERADOR"),
                                  (ADMIN_CREDENCIALES, "ADMINISTRADOR")):
            with self.subTest(rol=rol):
                respuesta = ServiciosPrueba(self.cliente, **credenciales).usuario_actual()
                self.assertEqual(respuesta.codigo, "OK")
                self.assertEqual(respuesta.resultado,
                                 {"usuario": credenciales["usuario"], "rol": rol})

    def test_usuario_actual_rechaza_credenciales_y_parametros_invalidos(self):
        for credenciales, codigo in (({"usuario": "operador", "clave": "Incorrecta"}, "NO_AUTENTICADO"),
                                     ({"usuario": "nadie", "clave": "Incorrecta"}, "NO_AUTENTICADO"),
                                     ({"usuario": 5, "clave": "Incorrecta"}, "VALIDACION")):
            with self.subTest(credenciales=credenciales["usuario"]):
                respuesta = self.cliente.enviar(solicitud("usuario.actual", credenciales=credenciales))
                self.assertEqual(respuesta.codigo, codigo)
                self.assertEqual(set(respuesta.resultado), {"mensaje"})
        self.assertEqual(self.cliente.enviar(
            MensajeRPC.solicitud("usuario.actual")).codigo, "NO_AUTENTICADO")
        for parametros in ({"usuario": "admin"}, {"rol": "ADMINISTRADOR"}, {"extra": True}):
            with self.subTest(parametros=parametros):
                respuesta = self.cliente.enviar(solicitud("usuario.actual", parametros))
                self.assertEqual(respuesta.codigo, "VALIDACION")
                self.assertEqual(set(respuesta.resultado), {"mensaje"})

    def test_eco_utf8_y_escape_xml(self):
        texto = 'Mecatronica: áéíóú ñ <robot X="100"> & pinza\nSegunda linea'
        self.assertEqual(self.servicios.eco(texto).resultado["texto"], texto)

    def test_eco_vacio(self):
        self.assertEqual(self.servicios.eco("").resultado["texto"], "")

    def test_operacion_desconocida_es_error_de_aplicacion(self):
        respuesta = self.cliente.enviar(solicitud("operacion_desconocida"))
        self.assertEqual(respuesta.codigo, "OPERACION_NO_SOPORTADA")
        self.assertIn("mensaje", respuesta.resultado)

    def test_error_de_parametros_es_respuesta_normal(self):
        for operacion, parametros in (
            ("ping", {"extra": 1}),
            ("eco", {}),
            ("eco", {"texto": 123}),
            ("eco", {"texto": "hola", "extra": True}),
            ("ayuda", {"extra": True}),
            ("activar_motores", {"extra": True}),
            ("seleccionar_modo_trabajo",
             {"modo": "manual", "extra": True}),
            ("seleccionar_modo_coordenadas",
             {"modo": "absoluto", "extra": True}),
            ("ir_a_origen", {"extra": True}),
            ("activar_efector", {"extra": True}),
            ("mover", {"x": 0.0, "y": 170.0, "z": 120.0,
                       "velocidad": 11.0}),
            ("mover", {"x": 0.0, "y": 170.0, "z": 120.0,
                       "velocidad": 4.0}),
            ("mover", {"x": "cero", "y": 170.0, "z": 120.0}),
        ):
            with self.subTest(operacion=operacion, parametros=parametros):
                respuesta = self.cliente.enviar(
                    solicitud(operacion, parametros)
                )
                self.assertEqual(respuesta.codigo, "VALIDACION")
                self.assertIn("mensaje", respuesta.resultado)
        self.assertEqual(self.servicios.ping().codigo, "OK")

    def test_operaciones_robot_sin_conexion(self):
        for operacion, parametros in (
            ("activar_motores", {}),
            ("desactivar_motores", {}),
            ("ir_a_origen", {}),
            ("activar_efector", {}),
            ("desactivar_efector", {}),
            ("mover", {"x": 0.0, "y": 170.0, "z": 120.0}),
        ):
            with self.subTest(operacion=operacion):
                respuesta = self.cliente.enviar(
                    solicitud(operacion, parametros)
                )
                self.assertEqual(respuesta.codigo, "ESTADO_INVALIDO")
                self.assertIn("mensaje", respuesta.resultado)

    def test_estado_desconectado_informa_datos_no_disponibles(self):
        respuesta = self.cliente.enviar(solicitud("estado"))
        self.assertEqual(respuesta.codigo, "OK")
        self.assertFalse(respuesta.resultado["conectado"])
        self.assertFalse(respuesta.resultado["posicion"]["disponible"])
        self.assertIn("mensaje", respuesta.resultado["posicion"])
        actividad = respuesta.resultado["actividad"]
        for nombre in ("motores", "ventilador"):
            self.assertFalse(actividad[nombre]["disponible"])
            self.assertIn("mensaje", actividad[nombre])
        self.assertTrue(actividad["modo_coordenadas"]["disponible"])
        self.assertEqual(actividad["modo_coordenadas"]["valor"], "absoluto")
        self.assertEqual(respuesta.resultado["modo_trabajo"], "manual")
        self.assertEqual(respuesta.resultado["modo_coordenadas"], "absoluto")

    def test_modos_independientes_y_operaciones_solo_manuales(self):
        trabajo = self.servicios.seleccionar_modo_trabajo("automatico")
        coordenadas = self.servicios.seleccionar_modo_coordenadas("relativo")
        self.assertEqual(trabajo.codigo, "OK")
        self.assertEqual(coordenadas.codigo, "OK")
        self.assertEqual(trabajo.resultado["modo_trabajo"], "automatico")
        self.assertEqual(coordenadas.resultado["modo_coordenadas"], "relativo")

        estado = self.servicios.estado_robot()
        self.assertEqual(estado.resultado["modo_trabajo"], "automatico")
        self.assertEqual(estado.resultado["modo_coordenadas"], "relativo")
        for operacion in (
            "ir_a_origen", "mover", "activar_efector", "desactivar_efector",
        ):
            parametros = (
                {"x": 0.0, "y": 170.0, "z": 120.0}
                if operacion == "mover" else {}
            )
            respuesta = self.cliente.enviar(solicitud(operacion, parametros))
            self.assertEqual(respuesta.codigo, "ESTADO_INVALIDO", operacion)

        manual = self.servicios.seleccionar_modo_trabajo("manual")
        self.assertEqual(manual.codigo, "OK")
        desconectado = self.servicios.ir_a_origen()
        self.assertEqual(desconectado.codigo, "ESTADO_INVALIDO")
        invalidos = (
            self.servicios.seleccionar_modo_trabajo("semiautomatico"),
            self.servicios.seleccionar_modo_coordenadas("local"),
        )
        self.assertTrue(all(respuesta.codigo == "VALIDACION"
                            for respuesta in invalidos))

    def test_ayuda_y_desconexion_no_requieren_robot(self):
        ayuda = self.cliente.enviar(solicitud("ayuda"))
        self.assertEqual(ayuda.codigo, "OK")
        self.assertIn("M17", ayuda.resultado["comandos"])
        self.assertIn("M18", ayuda.resultado["comandos"])
        self.assertIn("G28", ayuda.resultado["comandos"])
        self.assertIn("G1", ayuda.resultado["comandos"])
        self.assertIn("M3", ayuda.resultado["comandos"])
        self.assertIn("M5", ayuda.resultado["comandos"])
        desconexion = self.cliente.enviar(
            solicitud("desconectar", credenciales=ADMIN_CREDENCIALES)
        )
        self.assertEqual(desconexion.codigo, "OK")
        self.assertFalse(desconexion.resultado["conectado"])

    def test_conexion_rechaza_puerto_inexistente(self):
        respuesta = self.cliente.enviar(
            solicitud(
                "conectar", {"puerto": "puerto-inexistente"},
                credenciales=ADMIN_CREDENCIALES
            )
        )
        self.assertEqual(respuesta.codigo, "ERROR_CONEXION")
        self.assertFalse(respuesta.resultado["conectado"])
        self.assertIn("mensaje", respuesta.resultado)

    def test_solo_administrador_puede_conectar_o_desconectar(self):
        operador_conectar = self.cliente.enviar(
            solicitud(
                "conectar", {"puerto": "puerto-inexistente"},
                credenciales=CREDENCIALES
            )
        )
        self.assertEqual(operador_conectar.codigo, "PERMISO_DENEGADO")

        operador_desconectar = self.cliente.enviar(
            solicitud("desconectar", credenciales=CREDENCIALES)
        )
        self.assertEqual(operador_desconectar.codigo, "PERMISO_DENEGADO")

        admin_desconectar = self.cliente.enviar(
            solicitud("desconectar", credenciales=ADMIN_CREDENCIALES)
        )
        self.assertEqual(admin_desconectar.codigo, "OK")
        self.assertFalse(admin_desconectar.resultado["conectado"])

    def test_argumentos_xmlrpc_invalidos_y_recuperacion(self):
        for argumentos in ((), ("texto",), ({}, {})):
            with self.subTest(argumentos=argumentos), ServerProxy(self.url) as remoto:
                with self.assertRaises(Fault):
                    remoto.robot.intercambiar(*argumentos)
        self.assertEqual(self.servicios.ping().codigo, "OK")

    def test_mensajes_invalidos_son_fault(self):
        base = solicitud("eco", {"texto": "OK"}).serializar()
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
            [str(SERVIDOR), "--port", str(self.puerto), "--db", str(self.base),
             "--tareas-dir", str(Path(self.temporal.name) / "tareas_otro_servidor")],
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
            servicio = ServiciosPrueba(ClienteXmlRpc(self.url, timeout=5), **CREDENCIALES)
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
                ServiciosPrueba(ClienteXmlRpc(url, timeout=0.2), **CREDENCIALES).ping()

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
                    ServiciosPrueba(ClienteXmlRpc(url, timeout=0.15), **CREDENCIALES).ping()
                self.assertLess(time.monotonic() - inicio, 1.5)
            finally:
                terminado.set()
                hilo.join(timeout=3)

    def ejecutar_cliente(self, argumentos, credenciales=CREDENCIALES):
        # Solo reemplazar getpass para alimentar credenciales ficticias por stdin.
        return subprocess.run(
            [sys.executable, "-B", "-c",
             "import sys; import robot_client.cli as c; "
             "c.getpass=lambda _:sys.stdin.readline().rstrip('\\n'); sys.exit(c.main())",
             "--url", self.url, *argumentos], cwd=ROOT / "client",
            input=f"{credenciales['usuario']}\n{credenciales['clave']}\n",
            capture_output=True, text=True, timeout=10)

    def test_tareas_subida_desde_cli_propiedad_y_persistencia(self):
        archivo = Path(self.temporal.name) / "Visibilidad.NC"
        texto = "; contenido_ficticio_no_registrar\nG90\nM114\n"
        archivo.write_bytes(b"\xef\xbb\xbf" + texto.replace("\n", "\r\n").encode("utf-8"))
        for credenciales in (CREDENCIALES, BRUNO_CREDENCIALES, ADMIN_CREDENCIALES):
            resultado = self.ejecutar_cliente(["subir", str(archivo)], credenciales)
            self.assertEqual(resultado.returncode, 0, resultado.stderr)
            self.assertIn("No se ejecutaron ordenes", resultado.stdout)
            self.assertNotIn(credenciales["clave"], resultado.stdout + resultado.stderr)
        for credenciales in (CREDENCIALES, BRUNO_CREDENCIALES):
            tareas = ServiciosPrueba(self.cliente, **credenciales).listar_tareas().resultado["tareas"]
            self.assertTrue(tareas)
            self.assertTrue(all(t["propietario"] == credenciales["usuario"] for t in tareas))
            self.assertTrue(all(set(t) == {"id", "nombre", "propietario"} for t in tareas))
        admin = ServiciosPrueba(self.cliente, **ADMIN_CREDENCIALES)
        filas = [t for t in admin.listar_tareas().resultado["tareas"] if t["nombre"] == "visibilidad.nc"]
        self.assertEqual({t["propietario"] for t in filas}, {"operador", "bruno", "admin"})
        self.assertEqual(len({t["id"] for t in filas}), 3)
        with closing(sqlite3.connect(self.base)) as db:
            propietarios = dict(db.execute("SELECT nombre,id FROM usuarios"))
        for tarea in filas:
            propietario_id = propietarios[tarea["propietario"]]
            self.assertEqual(tarea["id"], f"{propietario_id}:visibilidad.nc")
            self.assertEqual((self.tareas / str(propietario_id) / "visibilidad.nc").read_bytes(), texto.encode("utf-8"))
        # Reiniciar el mismo servidor con la misma base y carpeta temporales.
        self.proceso.terminate()
        self.proceso.wait(timeout=7)
        self.iniciar_servidor()
        reabiertas = [t for t in admin.listar_tareas().resultado["tareas"] if t["nombre"] == "visibilidad.nc"]
        self.assertEqual(reabiertas, filas)
        self.salida.flush()
        with open(self.salida.fileno(), "r", encoding="utf-8", closefd=False) as log:
            log.seek(0)
            self.assertNotIn("contenido_ficticio_no_registrar", log.read())

    def test_tareas_listado_vacio(self):
        servicios = ServiciosPrueba(self.cliente, "vacio", "ClaveVacio1")
        respuesta = servicios.listar_tareas()
        self.assertEqual(respuesta.codigo, "OK")
        self.assertEqual(respuesta.resultado, {"tareas": []})
        resultado = self.ejecutar_cliente(["tareas"], {"usuario": "vacio", "clave": "ClaveVacio1"})
        self.assertEqual(resultado.returncode, 0, resultado.stderr)
        self.assertIn("No hay tareas disponibles", resultado.stdout)

    def test_tareas_duplicados_y_validacion_en_servidor(self):
        respuesta = self.servicios.subir_tarea("Duplicada.nc", "M114")
        self.assertEqual(respuesta.codigo, "OK")
        self.assertEqual(set(respuesta.resultado), {"id", "nombre", "propietario", "mensaje"})
        self.assertEqual(respuesta.resultado["propietario"], "operador")
        duplicada = self.servicios.subir_tarea("DUPLICADA.NC", "M18")
        self.assertEqual(duplicada.codigo, "ESTADO_INVALIDO")
        valido = {"nombre": "rechazada.nc", "contenido": "G28"}
        for parametros in ({}, {"nombre": "a.nc"}, {"contenido": "G28"},
                           {"nombre": 3, "contenido": "G28"}, {"nombre": "a.nc", "contenido": False},
                           {**valido, "propietario": "admin"}, {**valido, "usuario_id": 1},
                           {**valido, "ruta": "/tmp/otra.nc"}, {**valido, "rol": "ADMINISTRADOR"},
                           {**valido, "nombre": "../escape.nc"}, {**valido, "nombre": "archivo.txt"},
                           {**valido, "contenido": "hola"}, {**valido, "contenido": ""}):
            with self.subTest(parametros=list(parametros)):
                respuesta = self.cliente.enviar(solicitud("tarea.subir", parametros))
                self.assertEqual(respuesta.codigo, "VALIDACION")
        for parametros in ({"propietario": "admin"}, {"usuario_id": 1}, {"extra": True}):
            self.assertEqual(self.cliente.enviar(solicitud("tarea.listar", parametros)).codigo, "VALIDACION")
        # Saltar la comprobacion local de tamano: el servidor debe rechazar igualmente.
        grande = ServiciosPrueba(ClienteXmlRpc(self.url, timeout=10), **CREDENCIALES)
        self.assertEqual(grande.subir_tarea("grande.nc", "G1\n;" + "a" * (1024 * 1024)).codigo, "VALIDACION")
        tarea = self.servicios.listar_tareas()
        self.assertEqual(tarea.codigo, "OK")
        self.assertFalse(any(t["nombre"] in ("rechazada.nc", "grande.nc") for t in tarea.resultado["tareas"]))

    def test_tareas_rechazos_de_autenticacion_no_escriben(self):
        antes = set(self.tareas.rglob("*"))
        datos = {"nombre": "no_autorizada.nc", "contenido": "G28"}
        for operacion, parametros in (("tarea.subir", datos), ("tarea.listar", {})):
            for credenciales in ({"usuario": "operador", "clave": "Incorrecta"},
                                 {"usuario": "inexistente", "clave": "Incorrecta"}):
                self.assertEqual(self.cliente.enviar(solicitud(operacion, parametros, credenciales)).codigo,
                                 "NO_AUTENTICADO")
            self.assertEqual(self.cliente.enviar(MensajeRPC.solicitud(operacion, parametros)).codigo,
                             "NO_AUTENTICADO")
        admin = ServiciosPrueba(self.cliente, **ADMIN_CREDENCIALES)
        self.assertEqual(admin.deshabilitar_operador("bruno").codigo, "OK")
        try:
            for operacion, parametros in (("tarea.subir", datos), ("tarea.listar", {})):
                self.assertEqual(self.cliente.enviar(solicitud(operacion, parametros, BRUNO_CREDENCIALES)).codigo,
                                 "PERMISO_DENEGADO")
        finally:
            self.assertEqual(admin.habilitar_operador("bruno").codigo, "OK")
        self.assertEqual(set(self.tareas.rglob("*")), antes)

    def test_tareas_subidas_concurrentes_no_sobrescriben(self):
        def subir(indice):
            servicio = ServiciosPrueba(ClienteXmlRpc(self.url, timeout=10), **CREDENCIALES)
            return indice, servicio.subir_tarea("Concurrente.nc", f"G1 X{indice}\n")
        with ThreadPoolExecutor(max_workers=4) as pool:
            respuestas = list(pool.map(subir, range(4)))
        correctas = [(i, r) for i, r in respuestas if r.codigo == "OK"]
        self.assertEqual(len(correctas), 1)
        self.assertEqual(sum(r.codigo == "ESTADO_INVALIDO" for _, r in respuestas), 3)
        indice, respuesta = correctas[0]
        propietario_id = respuesta.resultado["id"].split(":")[0]
        self.assertEqual((self.tareas / propietario_id / "concurrente.nc").read_text(encoding="utf-8"),
                         f"G1 X{indice}\n")

    def test_rechazos_de_autenticacion(self):
        for credenciales, codigo in (
            ({"usuario": "operador", "clave": "Incorrecta"}, "NO_AUTENTICADO"),
            ({"usuario": "nadie", "clave": "Incorrecta"}, "NO_AUTENTICADO"),
            ({"usuario": 5, "clave": "Incorrecta"}, "VALIDACION"),
        ):
            respuesta = self.cliente.enviar(MensajeRPC.solicitud("ping", {"credenciales": credenciales}))
            self.assertEqual(respuesta.codigo, codigo)
        self.assertEqual(self.cliente.enviar(MensajeRPC.solicitud("ping")).codigo, "NO_AUTENTICADO")

    def test_cuenta_deshabilitada_entre_peticiones(self):
        self.assertEqual(self.servicios.ping().codigo, "OK")
        try:
            with closing(sqlite3.connect(self.base)) as db:
                with db:
                    db.execute("UPDATE usuarios SET habilitado=0 WHERE nombre='operador'")
            self.assertEqual(self.servicios.ping().codigo, "PERMISO_DENEGADO")
            respuesta = self.servicios.usuario_actual()
            self.assertEqual(respuesta.codigo, "PERMISO_DENEGADO")
            self.assertEqual(set(respuesta.resultado), {"mensaje"})
        finally:
            with closing(sqlite3.connect(self.base)) as db:
                with db:
                    db.execute("UPDATE usuarios SET habilitado=1 WHERE nombre='operador'")
        self.assertEqual(self.servicios.ping().codigo, "OK")

    def test_menu_real_segun_rol_y_rechazo_de_autenticacion(self):
        for nombre, clave, administrador, retorno in (
            ("operador", "ClaveOperador1", False, 0),
            ("admin", "ClaveAdmin1", True, 0),
            ("operador", "Incorrecta", False, 1),
        ):
            with self.subTest(usuario=nombre, retorno=retorno):
                resultado = subprocess.run(
                    [sys.executable, "-B", "-c",
                     "import sys; import robot_client.cli as c; "
                     "c.getpass=lambda _:sys.stdin.readline().rstrip('\\n'); sys.exit(c.main())",
                     "--url", self.url], cwd=ROOT / "client",
                    input=f"{nombre}\n{clave}\n18\n0\n", capture_output=True, text=True, timeout=5)
                self.assertEqual(resultado.returncode, retorno, resultado.stderr)
                self.assertNotIn(clave, resultado.stdout + resultado.stderr)
                self.assertEqual("5. Conectar robot" in resultado.stdout, administrador)
                self.assertEqual("3. Habilitar operador" in resultado.stdout, administrador)
                if retorno == 0:
                    self.assertIn("7. Consultar estado", resultado.stdout)
                    self.assertIn("10. Ayuda", resultado.stdout)
                    self.assertIn("17. Subir archivo", resultado.stdout)
                    self.assertIn("18. Listar tareas", resultado.stdout)
                    self.assertIn("tareas disponibles", resultado.stdout)
                else:
                    self.assertIn("NO_AUTENTICADO", resultado.stdout)
                    self.assertNotIn("1. Comprobar", resultado.stdout)

    def test_cli_real(self):
        resultado = subprocess.run(
            [sys.executable, "-c",
             "import sys; import robot_client.cli as c; "
             "c.getpass=lambda _:sys.stdin.readline().rstrip('\\n'); sys.exit(c.main())",
             "--url", self.url, "eco", "Hola desde CLI"],
            cwd=ROOT / "client", input="operador\nClaveOperador1\n",
            capture_output=True, text=True, timeout=5,
        )
        self.assertEqual(resultado.returncode, 0, resultado.stderr)
        self.assertIn("[OK] Hola desde CLI", resultado.stdout)


if __name__ == "__main__":
    unittest.main()

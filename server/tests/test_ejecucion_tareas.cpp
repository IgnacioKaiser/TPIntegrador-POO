#include "domain/Robot.h"
#include "robotrpc/repositorio_tareas.hpp"

#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace fs = std::filesystem;
using namespace robotrpc;
namespace {
int comprobaciones = 0, omitidas = 0;
void exigir(bool condicion, const char* mensaje) {
    if (!condicion) throw std::runtime_error(mensaje);
    ++comprobaciones;
}
template<class Excepcion, class Accion>
void rechazar(Accion accion) {
    bool rechazo = false;
    try { accion(); } catch (const Excepcion&) { rechazo = true; }
    exigir(rechazo, "Se esperaba un rechazo.");
}
void errorTarea(TipoErrorTarea tipo, const std::function<void()>& accion) {
    try { accion(); }
    catch (const ErrorTarea& e) { exigir(e.tipo() == tipo, "Tipo de error de tarea incorrecto."); return; }
    throw std::runtime_error("La lectura debia fallar.");
}
void escribir(const fs::path& ruta, const std::string& texto) {
    std::ofstream archivo(ruta, std::ios::binary);
    archivo << texto;
    archivo.close();
    exigir(bool(archivo), "No se pudo preparar el archivo temporal.");
}
const std::string POSICION = "INFO: CURRENT POSITION: [X:0 Y:170 Z:120 E:0]\r\n"
                             "INFO: MOTORS ENABLED\r\nOK\r\n";
const std::string FINAL = "INFO: CURRENT POSITION: [X:1 Y:170 Z:120 E:0]\r\nOK\r\n";
const std::string G1 = "G1 X1.000000 Y170.000000 Z120.000000 F5.000000\r";

struct PasoSerie {
    std::string comando, respuesta;
    unsigned int timeoutMs = 1000;
    bool fallo = false;
};

// Lista breve de respuestas, sin interpretar geometria ni simular movimiento.
class CanalSustituto final : public CanalSerie {
public:
    std::vector<PasoSerie> pasos;
    std::vector<std::string> enviados;
    bool conectado = true, coincidieron = true;
    explicit CanalSustituto(std::vector<PasoSerie> guion = {}) : pasos(std::move(guion)) {}
    bool conectar(const std::string&) override { conectado = true; return true; }
    void desconectar() noexcept override { conectado = false; }
    bool estaConectado() const noexcept override { return conectado; }
    std::string intercambiar(const std::string& comando, unsigned int timeoutMs) override {
        const auto indice = enviados.size();
        enviados.push_back(comando);
        if (indice >= pasos.size() || comando != pasos[indice].comando ||
            timeoutMs != pasos[indice].timeoutMs) {
            coincidieron = false;
            throw std::runtime_error("Intercambio diferente al esperado.");
        }
        const auto& paso = pasos[indice];
        if (paso.fallo) {
            conectado = false;
            throw ErrorSerie("Timeout simulado", paso.respuesta);
        }
        return paso.respuesta;
    }
    void verificar() const {
        exigir(coincidieron && enviados.size() == pasos.size(), "Secuencia o timeout inesperados; posible reintento.");
    }
};

OrdenTarea orden(const std::string& texto) { return ProgramaTarea(texto).ordenes().at(0); }
void errorOrden(ResultadoOrdenAutomatica esperado, Robot& robot, const OrdenTarea& o) {
    try { robot.ejecutarOrdenAutomatica(o); }
    catch (const ErrorOrdenAutomatica& e) {
        exigir(e.resultado() == esperado, "Clasificacion del resultado de orden incorrecta.");
        return;
    }
    throw std::runtime_error("La orden debia fallar.");
}

void probarLectura(const fs::path& temporal) {
    RepositorioTareas repo(temporal / "tareas");
    const Usuario ana{2, "ana", Rol::OPERADOR, true}, bruno{3, "bruno", Rol::OPERADOR, true};
    const Usuario admin{1, "admin", Rol::ADMINISTRADOR, true};
    const auto propia = repo.guardar(ana, "pieza.nc", "G90\nG1 X1\n");
    repo.guardar(bruno, "pieza.nc", "M3\n");
    const auto leida = repo.leer(ana, propia.id);
    exigir(leida.tarea.id == propia.id && leida.contenido == "G90\nG1 X1\n", "Se leyo otro archivo.");
    exigir(repo.leer(admin, "3:pieza.nc").contenido == "M3\n", "El admin no puede leer otra tarea.");
    const ProgramaTarea programaLeido(repo.leer(admin, "3:pieza.nc").contenido);
    CanalSustituto canal({{"M3\r", "OK\n", 5000}});
    Robot robot(canal);
    robot.seleccionarModoTrabajo(ModoTrabajo::AUTOMATICO);
    robot.ejecutarOrdenAutomatica(programaLeido.ordenes().at(0));
    canal.verificar(); // Archivo administrado -> parser -> Robot -> canal sustituido, sin RPC.
    errorTarea(TipoErrorTarea::ACCESO_DENEGADO, [&] { repo.leer(ana, "3:pieza.nc"); });
    errorTarea(TipoErrorTarea::ACCESO_DENEGADO, [&] { repo.leer(ana, "3:ausente.nc"); });
    errorTarea(TipoErrorTarea::NO_ENCONTRADA, [&] { repo.leer(ana, "2:ausente.nc"); });
    errorTarea(TipoErrorTarea::NO_ENCONTRADA, [&] { repo.leer(admin, "999:ausente.nc"); });
    for (const auto& id : {"", "pieza.nc", "02:pieza.nc", "0:pieza.nc", "-2:pieza.nc",
                            "2:../pieza.nc", "2:sub/pieza.nc", "2:PIEZA.nc", "2:C:\\pieza.nc",
                            "2:con.nc", "2:pieza.nc:otra.nc", "9999999999999999:pieza.nc"})
        errorTarea(TipoErrorTarea::VALIDACION, [&] { repo.leer(admin, id); });
    for (const auto& usuario : {Usuario{2,"ana",Rol::OPERADOR,false}, Usuario{0,"ana",Rol::OPERADOR,true}})
        errorTarea(TipoErrorTarea::ACCESO_DENEGADO, [&] { repo.leer(usuario, propia.id); });
    fs::create_directory(temporal / "tareas/2/carpeta.nc");
    errorTarea(TipoErrorTarea::ALMACENAMIENTO, [&] { repo.leer(ana, "2:carpeta.nc"); });
    escribir(temporal / "tareas/2/grande.nc", std::string(RepositorioTareas::MAX_CONTENIDO + 1, 'a'));
    errorTarea(TipoErrorTarea::VALIDACION, [&] { repo.leer(ana, "2:grande.nc"); });
    escribir(temporal / "tareas/2/vacia.nc", "");
    errorTarea(TipoErrorTarea::VALIDACION, [&] { repo.leer(ana, "2:vacia.nc"); });
    escribir(temporal / "tareas/2/bom.nc", "\xef\xbb\xbfG90\r\nM3\rM5");
    exigir(repo.leer(ana, "2:bom.nc").contenido == "G90\nM3\nM5", "Lectura sin normalizar.");
    // El almacen sigue sin certificar ejecucion: solo ProgramaTarea rechaza G999.
    repo.guardar(ana, "no_ejecutable.nc", "G999 X999999");
    rechazar<std::invalid_argument>([&] { ProgramaTarea p(repo.leer(ana, "2:no_ejecutable.nc").contenido); });
    escribir(temporal / "externa.nc", "M3");
    std::error_code ec;
    fs::create_symlink(temporal / "externa.nc", temporal / "tareas/2/enlace.nc", ec);
    if (ec) ++omitidas;
    else errorTarea(TipoErrorTarea::ALMACENAMIENTO, [&] { repo.leer(ana, "2:enlace.nc"); });
    ec.clear();
    fs::create_directory_symlink(temporal / "tareas/2", temporal / "tareas/4", ec);
    if (ec) ++omitidas;
    else errorTarea(TipoErrorTarea::ALMACENAMIENTO, [&] { repo.leer(admin, "4:pieza.nc"); });
}

void probarPrograma() {
    const ProgramaTarea p("\xef\xbb\xbf%\r\n; comentario\r\n(inicio\ncomentario)\n"
                          "N10 g01x+1.y170z.5f7.5 ; nota\rG 90\nM 03\n%\n");
    exigir(p.ordenes().size() == 3, "Cantidad de ordenes incorrecta.");
    exigir(p.ordenes()[0].linea == 5 && p.ordenes()[1].linea == 6, "Se perdio la linea original.");
    exigir(p.ordenes()[0].x == 1.0 && p.ordenes()[0].velocidad == 7.5, "Argumentos mal interpretados.");
    for (const auto& texto : {"G0 X0", "G28", "G90", "G91", "M3", "M5", "M17", "M18", "M114", "G1Z-1F10"})
        exigir(ProgramaTarea(texto).ordenes().size() == 1, "Orden admitida rechazada.");
    for (const auto& texto : {"", "%\n;sin orden", "G1", "G1 F5", "G92 X0", "G4 S1", "M30", "M1",
                             "M106", "G999 X1", "G1 E0", "G1 X1 S1", "G1 X1 X2", "G90 X1",
                             "G1 Xnan", "G1 Xinf", "G1 X1e2", "G1 X.", "G1 X+", "G1 X1,5",
                             "G1 X1F4", "G1 X1F11", "G1 X1*20", "N G90", "N10", "G90 M3",
                             "(abierto", "M3)", "((anidado)) M3", "M3\nG999", "G1 X1.2.3", "G1 X1%"})
        rechazar<std::invalid_argument>([&] { ProgramaTarea programa(texto); });
    rechazar<std::invalid_argument>([] { ProgramaTarea p("G1 X" + std::string(400, '9')); });
    rechazar<std::invalid_argument>([] { ProgramaTarea p(std::string(1024 * 1024 + 1, ' ')); });
}

void probarOrdenes() {
    {
        CanalSustituto canal;
        Robot robot(canal);
        rechazar<RobotModeException>([&] { robot.ejecutarOrdenAutomatica(orden("M3")); });
        robot.seleccionarModoTrabajo(ModoTrabajo::AUTOMATICO);
        canal.conectado = false;
        rechazar<RobotNotConnectedException>([&] { robot.ejecutarOrdenAutomatica(orden("M3")); });
        OrdenTarea invalida{TipoOrdenTarea::MOVIMIENTO, 1, {}, {}, {}, {}};
        rechazar<std::invalid_argument>([&] { robot.ejecutarOrdenAutomatica(invalida); });
        canal.verificar();
    }
    {
        CanalSustituto canal({{"M114\r",POSICION}, {"G90\r","OK\r\n"},
                              {G1,"OK\r\n",10200}, {"M114\r",FINAL,10200}});
        Robot robot(canal);
        robot.seleccionarModoTrabajo(ModoTrabajo::AUTOMATICO);
        exigir(robot.ejecutarOrdenAutomatica(orden("G1 X1")).finalizacionConsultada, "Falta M114 final.");
        exigir(robot.modoTrabajo() == ModoTrabajo::AUTOMATICO, "Se cambio temporalmente a manual.");
        canal.verificar();
    }
    {
        CanalSustituto canal({{"G91\r","OK\r\n"}, {"M114\r",POSICION}, {"G91\r","OK\r\n"},
            {"G1 X1.000000 Y0.000000 Z0.000000 F5.000000\r","OK\r\n",10200}, {"M114\r",FINAL,10200}});
        Robot robot(canal);
        robot.seleccionarModoTrabajo(ModoTrabajo::AUTOMATICO);
        robot.ejecutarOrdenAutomatica(orden("G91"));
        exigir(robot.modoCoordenadas() == ModoCoordenadas::RELATIVO, "No se actualizo el modo.");
        robot.ejecutarOrdenAutomatica(orden("G0 X1"));
        canal.verificar();
    }
    for (const auto& estado : {"INFO: MOTORS DISABLED\r\nOK\r\n", "OK\r\n",
                               "INFO: MOTORS ENABLED\r\nOK\r\n"}) {
        CanalSustituto canal({{"M114\r",estado}});
        Robot robot(canal); robot.seleccionarModoTrabajo(ModoTrabajo::AUTOMATICO);
        errorOrden(ResultadoOrdenAutomatica::NO_ENVIADA, robot, orden("G1 X1"));
        canal.verificar();
    }
    {
        CanalSustituto canal({{"M114\r",POSICION}});
        Robot robot(canal); robot.seleccionarModoTrabajo(ModoTrabajo::AUTOMATICO);
        errorOrden(ResultadoOrdenAutomatica::NO_ENVIADA, robot, orden("G1 Z151"));
        canal.verificar();
    }
    for (const auto& fallo : std::vector<std::vector<PasoSerie>>{
            {{"M114\r","",1000,true}},
            {{"M114\r",POSICION},{"G90\r","ERROR: fallo\nOK\n"}}}) {
        CanalSustituto canal(fallo);
        Robot robot(canal); robot.seleccionarModoTrabajo(ModoTrabajo::AUTOMATICO);
        errorOrden(ResultadoOrdenAutomatica::NO_ENVIADA, robot, orden("G1 X1"));
        canal.verificar();
    }
    {
        const std::string actual = "CURRENT POSITION: [X:0 Y:170 Z:0 E:0]\nMOTORS ENABLED\nOK\n";
        CanalSustituto canal({{"M114\r",actual}});
        Robot robot(canal); robot.seleccionarModoTrabajo(ModoTrabajo::AUTOMATICO);
        errorOrden(ResultadoOrdenAutomatica::NO_ENVIADA, robot, orden("G1 Y-170"));
        canal.verificar(); // El segmento cruza una zona no alcanzable, aunque sus extremos no.
    }
    for (const auto& pasoFinal : std::vector<PasoSerie>{
             {G1,"ERROR: COMMAND NOT RECOGNIZED\nOK\n",10200},
             {G1,"",10200,true}}) {
        CanalSustituto canal({{"M114\r",POSICION},{"G90\r","OK\n"},pasoFinal});
        Robot robot(canal); robot.seleccionarModoTrabajo(ModoTrabajo::AUTOMATICO);
        errorOrden(pasoFinal.fallo ? ResultadoOrdenAutomatica::DESCONOCIDO
                                  : ResultadoOrdenAutomatica::ERROR_CONTROLADOR, robot, orden("G1 X1"));
        canal.verificar();
    }
    for (const auto& barrera : std::vector<PasoSerie>{
             {"M114\r","ERROR: POINT IS OUTSIDE OF WORKSPACE\n" + FINAL,10200},
             {"M114\r","",10200,true}, {"M114\r",POSICION,10200},
             {"M114\r","OK\n",10200}}) {
        CanalSustituto canal({{"M114\r",POSICION},{"G90\r","OK\n"},{G1,"OK\n",10200},barrera});
        Robot robot(canal); robot.seleccionarModoTrabajo(ModoTrabajo::AUTOMATICO);
        errorOrden(barrera.respuesta.find("ERROR:") != std::string::npos
                       ? ResultadoOrdenAutomatica::ERROR_CONTROLADOR : ResultadoOrdenAutomatica::DESCONOCIDO,
                   robot, orden("G1 X1"));
        canal.verificar(); // Nunca se repite G1 tras perder la confirmacion final.
    }
    {
        CanalSustituto canal({{"M3\r","ERROR: fallo\n",5000,true}});
        Robot robot(canal); robot.seleccionarModoTrabajo(ModoTrabajo::AUTOMATICO);
        errorOrden(ResultadoOrdenAutomatica::ERROR_CONTROLADOR, robot, orden("M3"));
        canal.verificar();
    }
    for (const auto& paso : std::vector<PasoSerie>{{"M3\r","OK\n",5000},{"M5\r","OK\n",5000},
            {"M17\r","OK\n"},{"M18\r","OK\n"},{"G90\r","OK\n"},{"M114\r",POSICION}}) {
        CanalSustituto canal({paso}); Robot robot(canal);
        robot.seleccionarModoTrabajo(ModoTrabajo::AUTOMATICO);
        exigir(!robot.ejecutarOrdenAutomatica(orden(paso.comando)).finalizacionConsultada,
               "Se declaro una barrera inexistente.");
        canal.verificar();
    }
    {
        CanalSustituto canal({{"M114\r",POSICION},{"G28\r","INFO: HOMING COMPLETE\nOK\n",10000},
                              {"M114\r",POSICION,10000}});
        Robot robot(canal); robot.seleccionarModoTrabajo(ModoTrabajo::AUTOMATICO);
        exigir(robot.ejecutarOrdenAutomatica(orden("G28")).finalizacionConsultada, "Origen sin barrera.");
        canal.verificar();
    }
    {
        CanalSustituto canal({{"M114\r",POSICION},{"G28\r","OK\n",10000}});
        Robot robot(canal); robot.seleccionarModoTrabajo(ModoTrabajo::AUTOMATICO);
        errorOrden(ResultadoOrdenAutomatica::DESCONOCIDO, robot, orden("G28"));
        canal.verificar();
    }
    {
        // El control manual conserva su secuencia y usa la misma geometria.
        CanalSustituto canal({{"M114\r",POSICION},{"G90\r","OK\n"},{G1,"OK\n",10200}});
        Robot robot(canal);
        robot.mover(1,170,120,5);
        canal.verificar();
        robot.seleccionarModoTrabajo(ModoTrabajo::AUTOMATICO);
        rechazar<RobotModeException>([&] { robot.mover(1,170,120,5); });
    }
}
} // namespace

int main(int argc, char** argv) {
    if (argc != 2) return 1;
    try {
        probarLectura(fs::path(argv[1]));
        probarPrograma();
        probarOrdenes();
        std::cout << "US-17 Hito 1: " << comprobaciones << " comprobaciones correctas; "
                  << omitidas << " pruebas de enlaces omitidas. Canal sustituido, sin Arduino.\n";
    } catch (const std::exception& e) {
        std::cerr << "US-17 Hito 1: " << e.what() << '\n';
        return 1;
    }
}

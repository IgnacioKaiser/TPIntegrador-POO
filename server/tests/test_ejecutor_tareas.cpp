#include "robotrpc/ejecutor_tareas.hpp"
#include "robotrpc/servicio.hpp"
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <iostream>
#include <vector>

using namespace robotrpc;
using namespace std::chrono_literals;
namespace {
int comprobaciones = 0;
void exigir(bool b, const char* mensaje) {
    if (!b) throw std::runtime_error(mensaje);
    ++comprobaciones;
}
const Usuario ana{2,"ana",Rol::OPERADOR,true}, bruno{3,"bruno",Rol::OPERADOR,true},
              admin{1,"admin",Rol::ADMINISTRADOR,true};
const Tarea tarea{"2:pieza.nc",2,"pieza.nc"};

class CanalGuion final : public CanalSerie {
public:
    bool conectar(const std::string&) override { conectado_ = true; return true; }
    void desconectar() noexcept override { conectado_ = false; }
    bool estaConectado() const noexcept override { return conectado_; }
    bool bloquear = false;
    int fallar = 0;
    bool timeout = false;
    std::vector<std::string> comandos;
    std::string intercambiar(const std::string& comando, unsigned int) override {
        std::unique_lock<std::mutex> lock(mutex_);
        comandos.push_back(comando);
        if (bloquear && comandos.size() == 1) {
            entro_ = true;
            condicion_.notify_all();
            if (!condicion_.wait_for(lock, 5s, [&] { return liberar_; })) throw ErrorSerie("Fallo de la prueba: no se libero.");
        }
        if (fallar == static_cast<int>(comandos.size())) {
            if (timeout) { conectado_ = false; throw ErrorSerie("Timeout sustituido"); }
            return "ERROR: controlador sustituido\nOK\n";
        }
        return "OK\n";
    }
    void esperarEntrada() {
        std::unique_lock<std::mutex> lock(mutex_);
        exigir(condicion_.wait_for(lock, 3s, [&] { return entro_; }), "El hilo no envio la orden.");
    }
    void liberar() {
        std::lock_guard<std::mutex> lock(mutex_);
        liberar_ = true;
        condicion_.notify_all();
    }
    std::size_t enviados() {
        std::lock_guard<std::mutex> lock(mutex_);
        return comandos.size();
    }
private:
    bool conectado_ = true, entro_ = false, liberar_ = false;
    std::mutex mutex_;
    std::condition_variable condicion_;
};

ProgresoTarea esperar(EjecutorTareas& e, const Usuario& actor, const std::string& id) {
    const auto limite = std::chrono::steady_clock::now() + 3s;
    while (std::chrono::steady_clock::now() < limite) {
        const auto p = e.consultar(actor,id);
        if (p.estado == "finalizada" || p.estado == "fallida") return p;
        std::this_thread::sleep_for(1ms);
    }
    throw std::runtime_error("El ejecutor no termino.");
}
template<class Accion>
void rechazar(const std::string& codigo, Accion accion) {
    try { accion(); }
    catch (const ErrorEjecucion& e) { exigir(e.codigo() == codigo,"Codigo inesperado."); return; }
    throw std::runtime_error("Se acepto una operacion que debia rechazarse.");
}

void probarResultados(const std::filesystem::path& raiz) {
    for (int fallo : {0,2,3}) for (bool timeout : {false,true}) {
        CanalGuion canal; canal.fallar = fallo; canal.timeout = timeout;
        Robot robot(canal); robot.seleccionarModoTrabajo(ModoTrabajo::AUTOMATICO);
        RegistroActividadCsv log((raiz / ("resultado_" + std::to_string(fallo) + std::to_string(timeout)) / "actividad.csv").string());
        EjecutorTareas ejecutor(robot,log);
        const auto aceptada = ejecutor.iniciar(ana,tarea,ProgramaTarea("M3\nM5\nM3"),"run");
        exigir(aceptada.estado == "aceptada" && aceptada.confirmadas == 0,"La aceptacion anuncio finalizacion.");
        const auto p = esperar(ejecutor,ana,"run");
        exigir(p.total == 3 && p.confirmadas == (fallo ? static_cast<std::size_t>(fallo-1) : 3),"Prefijo confirmado incorrecto.");
        exigir(p.estado == (fallo ? "fallida" : "finalizada"),"Estado final incorrecto.");
        exigir(canal.enviados() == (fallo ? static_cast<std::size_t>(fallo) : 3),"Se reintento o continuo tras fallo.");
        exigir(p.resultado_orden == (fallo ? (timeout ? "desconocido" : "error_controlador") : "confirmada"),"Se perdio la incertidumbre.");
        if (fallo) exigir(p.orden_actual == static_cast<std::size_t>(fallo) && p.linea_actual == p.orden_actual,"Se perdio la linea fallida.");
        FiltroActividad f; f.usuario="ana"; f.tipo="EVENTO";
        const auto filas = log.consultar(f).registros;
        exigir(filas.size()==2 && filas[0].operacion=="tarea.aceptada" &&
               filas[1].operacion==(fallo ? "tarea.fallida" : "tarea.finalizada"),"Eventos incompletos o desordenados.");
        exigir(log.informeUsuarios(FiltroActividad{}).empty(),"Los eventos se contaron como peticiones RPC.");
        rechazar("ESTADO_INVALIDO",[&] { ejecutor.iniciar(ana,tarea,ProgramaTarea("M3"),"run"); });
    }
}

void probarReserva(const std::filesystem::path& raiz) {
    CanalGuion canal; canal.bloquear=true;
    Robot robot(canal); robot.seleccionarModoTrabajo(ModoTrabajo::AUTOMATICO);
    RegistroActividadCsv log((raiz/"reserva.csv").string());
    EjecutorTareas ejecutor(robot,log);
    ServicioRobot servicio(robot,&ejecutor);
    ejecutor.iniciar(ana,tarea,ProgramaTarea("M3\nM5"),"reservada");
    canal.esperarEntrada();
    const auto inicio=std::chrono::steady_clock::now();
    for(int i=0;i<100;++i) {
        const auto p=ejecutor.consultar(ana,"reservada");
        exigir(p.estado=="ejecutandose" && p.confirmadas==0,"Progreso incoherente con orden pendiente.");
    }
    for(const auto& op : {"mover","ir_a_origen","activar_efector","desactivar_efector",
                         "conectar","desconectar","activar_motores","desactivar_motores",
                         "seleccionar_modo_trabajo","seleccionar_modo_coordenadas"}) {
        const auto r=servicio.atender(MensajeRPC::solicitud("consulta",op));
        exigir(r.codigo()=="ESTADO_INVALIDO","No se rechazo operacion incompatible.");
    }
    const auto estado=servicio.atender(MensajeRPC::solicitud("estado","estado"));
    exigir(estado.codigo()=="OK" && estado.resultado().at("datos_anteriores").obtener<bool>(),"Estado no indica datos anteriores.");
    exigir(!estado.resultado().count("tarea_id") && !estado.resultado().count("ejecucion_id"),"Estado expuso identidad de tarea.");
    exigir(servicio.atender(MensajeRPC::solicitud("ping","ping")).codigo()=="OK","Ping bloqueado.");
    rechazar("ESTADO_INVALIDO",[&] { ejecutor.iniciar(admin,tarea,ProgramaTarea("M5"),"otra"); });
    rechazar("PERMISO_DENEGADO",[&] { ejecutor.consultar(bruno,"reservada"); });
    exigir(ejecutor.consultar(admin,"reservada").ejecucion_id=="reservada","Admin no consulta progreso.");
    exigir(canal.enviados()==1,"Se intercalo trafico serie.");
    exigir(std::chrono::steady_clock::now()-inicio<1s,"Las consultas esperaron al controlador.");
    canal.liberar();
    exigir(esperar(ejecutor,ana,"reservada").confirmadas==2,"No termino tras liberar controlador.");
    ejecutor.iniciar(admin,tarea,ProgramaTarea("M3"),"deladmin");
    exigir(esperar(ejecutor,admin,"deladmin").estado=="finalizada","Admin no ejecuta tarea ajena.");
    rechazar("PERMISO_DENEGADO",[&] { ejecutor.consultar(ana,"deladmin"); });
    rechazar("RECURSO_NO_ENCONTRADO",[&] { ejecutor.consultar(ana,"reservada"); });
}

void probarCierreYCondiciones(const std::filesystem::path& raiz) {
    CanalGuion canal; canal.bloquear=true;
    Robot robot(canal);
    RegistroActividadCsv log((raiz/"cierre.csv").string());
    EjecutorTareas ejecutor(robot,log);
    rechazar("ESTADO_INVALIDO",[&] { ejecutor.iniciar(ana,tarea,ProgramaTarea("M3"),"modo"); });
    robot.seleccionarModoTrabajo(ModoTrabajo::AUTOMATICO);
    canal.desconectar();
    rechazar("ESTADO_INVALIDO",[&] { ejecutor.iniciar(ana,tarea,ProgramaTarea("M3"),"conexion"); });
    canal.conectar("");
    rechazar("PERMISO_DENEGADO",[&] { ejecutor.iniciar(bruno,tarea,ProgramaTarea("M3"),"ajena"); });
    ejecutor.iniciar(ana,tarea,ProgramaTarea("M3\nM5"),"cierre");
    canal.esperarEntrada();
    std::atomic<bool> terminado{false};
    std::thread cierre([&] { ejecutor.cerrar(); terminado.store(true); });
    const auto limite=std::chrono::steady_clock::now()+2s;
    bool esperaCierre=false;
    while(std::chrono::steady_clock::now()<limite) {
        if(ejecutor.consultar(ana,"cierre").mensaje.find("se espera")!=std::string::npos) { esperaCierre=true; break; }
        std::this_thread::sleep_for(1ms);
    }
    const bool esperando=!terminado.load();
    canal.liberar(); cierre.join();
    exigir(esperaCierre && esperando,"El cierre no espero la orden pendiente.");
    const auto p=ejecutor.consultar(ana,"cierre");
    exigir(terminado.load() && p.estado=="fallida" && p.confirmadas==1,"No se detuvo entre ordenes al cerrar.");
    exigir(canal.enviados()==1 && p.resultado_orden=="no_enviada","Se envio otra orden durante cierre.");
    ejecutor.cerrar();
    rechazar("ESTADO_INVALIDO",[&] { ejecutor.iniciar(ana,tarea,ProgramaTarea("M3"),"cerrado"); });
}
} // namespace

int main(int argc,char** argv) {
    if(argc!=2) return 1;
    try {
        probarResultados(argv[1]); probarReserva(argv[1]); probarCierreYCondiciones(argv[1]);
        std::cout<<"US-17 Hito 2: "<<comprobaciones<<" comprobaciones de secuencia, reserva, progreso y cierre OK. Sin Arduino.\n";
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}

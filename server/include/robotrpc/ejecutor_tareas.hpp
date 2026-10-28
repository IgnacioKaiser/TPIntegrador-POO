#pragma once

#include "domain/Robot.h"
#include "robotrpc/tarea.hpp"
#include "robotrpc/usuario.hpp"
#include "robotrpc/registro_actividad.hpp"
#include <atomic>
#include <mutex>
#include <optional>
#include <thread>

namespace robotrpc {
struct ProgresoTarea {
    std::string ejecucion_id, tarea_id, estado, resultado_orden, codigo, mensaje;
    int solicitante_id = 0;
    std::size_t total = 0, confirmadas = 0, orden_actual = 0, linea_actual = 0;
    bool registro_completo = true;
};

// Datos locales conocidos ANTES de iniciar; no son una lectura actual del controlador.
struct FotoRobotReservado {
    bool conectado;
    ModoTrabajo trabajo;
    ModoCoordenadas coordenadas;
};

class ErrorEjecucion : public std::runtime_error {
public:
    ErrorEjecucion(std::string codigo, const std::string& mensaje)
        : std::runtime_error(mensaje), codigo_(std::move(codigo)) {}
    const std::string& codigo() const noexcept { return codigo_; }
private:
    std::string codigo_;
};

class EjecutorTareas {
public:
    EjecutorTareas(Robot& robot, RegistroActividadCsv& registro) : robot_(robot), registro_(registro) {}
    ~EjecutorTareas() { cerrar(); }
    EjecutorTareas(const EjecutorTareas&) = delete;
    EjecutorTareas& operator=(const EjecutorTareas&) = delete;

    // iniciar/cerrar se llaman desde el unico hilo de atencion RPC.
    ProgresoTarea iniciar(const Usuario& actor, const Tarea& tarea, ProgramaTarea programa,
                         const std::string& ejecucionId);
    ProgresoTarea consultar(const Usuario& actor, const std::string& ejecucionId) const;
    std::optional<FotoRobotReservado> fotoSiActiva() const;
    std::unique_lock<std::mutex> intentarAccesoRobot() const;
    void cerrar(); // Solo cierre del proceso, no cancelacion RPC. Espera la orden en curso.
private:
    Robot& robot_;
    RegistroActividadCsv& registro_;
    mutable std::mutex datos_, accesoRobot_;
    ProgresoTarea progreso_;
    FotoRobotReservado foto_{false, ModoTrabajo::MANUAL, ModoCoordenadas::ABSOLUTO};
    bool activa_ = false, cerrando_ = false;
    std::atomic<bool> detener_{false};
    std::thread hilo_;
    void ejecutar(ProgramaTarea programa, std::string actor);
};
} // namespace robotrpc

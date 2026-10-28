#include "robotrpc/ejecutor_tareas.hpp"
#include <iostream>

namespace robotrpc {
std::unique_lock<std::mutex> EjecutorTareas::intentarAccesoRobot() const {
    return std::unique_lock<std::mutex>(accesoRobot_, std::try_to_lock);
}

std::optional<FotoRobotReservado> EjecutorTareas::fotoSiActiva() const {
    std::lock_guard<std::mutex> lock(datos_);
    if (activa_) return foto_;
    return std::nullopt;
}

ProgresoTarea EjecutorTareas::iniciar(const Usuario& actor, const Tarea& tarea,
                                     ProgramaTarea programa, const std::string& id) {
    if (!actor.habilitado || actor.id <= 0 ||
        (actor.rol != Rol::ADMINISTRADOR && (actor.rol != Rol::OPERADOR || tarea.propietario_id != actor.id)))
        throw ErrorEjecucion("PERMISO_DENEGADO", "No tiene permiso para ejecutar esta tarea.");
    auto acceso = intentarAccesoRobot();
    if (!acceso.owns_lock()) throw ErrorEjecucion("ESTADO_INVALIDO", "El robot esta ocupado.");
    {
        std::lock_guard<std::mutex> lock(datos_);
        if (activa_ || cerrando_ || progreso_.ejecucion_id == id)
            throw ErrorEjecucion("ESTADO_INVALIDO", "No se puede iniciar esta ejecucion ahora.");
    }
    if (!robot_.estaConectado() || robot_.modoTrabajo() != ModoTrabajo::AUTOMATICO)
        throw ErrorEjecucion("ESTADO_INVALIDO", "Se requiere conexion y modo automatico.");
    // Un hilo terminado no vuelve a tocar Robot ni el registro despues de publicar activa_=false.
    if (hilo_.joinable()) hilo_.join();
    ProgresoTarea aceptada{id, tarea.id, "aceptada", "no_enviada", "OK",
                           "Tarea aceptada; todavia no confirma ejecucion.", actor.id,
                           programa.ordenes().size(), 0, 0, 0, true};
    {
        std::lock_guard<std::mutex> lock(datos_);
        foto_ = {robot_.estaConectado(), robot_.modoTrabajo(), robot_.modoCoordenadas()};
        progreso_ = aceptada;
        activa_ = true; // Reserva desde antes de crear el hilo, tambien durante su arranque.
    }
    bool aceptacionRegistrada = false;
    try {
        registro_.registrarEjecucion(id, actor.nombre, "tarea.aceptada", "OK",
                                     "Tarea=" + tarea.id + "; ordenes=" + std::to_string(aceptada.total));
        aceptacionRegistrada = true;
        hilo_ = std::thread(&EjecutorTareas::ejecutar, this, std::move(programa), actor.nombre);
    } catch (...) {
        auto fallida = aceptada;
        fallida.estado = "fallida";
        fallida.codigo = "ERROR_INTERNO";
        fallida.mensaje = "No se pudo iniciar el hilo o registrar la aceptacion.";
        fallida.registro_completo = aceptacionRegistrada;
        if (aceptacionRegistrada) {
            try { registro_.registrarEjecucion(id, actor.nombre, "tarea.fallida", "ERROR_INTERNO",
                                              "No se creo el hilo; ninguna orden enviada."); }
            catch (...) { fallida.registro_completo = false; }
        }
        std::lock_guard<std::mutex> lock(datos_);
        activa_ = false;
        progreso_ = fallida;
        throw ErrorEjecucion("ERROR_INTERNO", progreso_.mensaje);
    }
    return aceptada;
}

ProgresoTarea EjecutorTareas::consultar(const Usuario& actor, const std::string& id) const {
    std::lock_guard<std::mutex> lock(datos_);
    if (progreso_.ejecucion_id.empty() || progreso_.ejecucion_id != id)
        throw ErrorEjecucion("RECURSO_NO_ENCONTRADO", "La ejecucion no esta disponible en este proceso.");
    if (!actor.habilitado || actor.id <= 0 ||
        (actor.rol != Rol::ADMINISTRADOR && (actor.rol != Rol::OPERADOR || progreso_.solicitante_id != actor.id)))
        throw ErrorEjecucion("PERMISO_DENEGADO", "No puede consultar esta ejecucion.");
    return progreso_;
}

void EjecutorTareas::ejecutar(ProgramaTarea programa, std::string actor) {
    ProgresoTarea final;
    bool fallo = false;
    {
        // Solo el trabajador espera este lock: la entrada RPC siempre usa try_lock.
        std::unique_lock<std::mutex> acceso(accesoRobot_);
        {
            std::lock_guard<std::mutex> lock(datos_);
            progreso_.estado = "ejecutandose";
            progreso_.mensaje = "Ejecucion en curso; progreso confirmado por protocolo.";
        }
        for (std::size_t i = 0; i < programa.ordenes().size(); ++i) {
            if (detener_.load()) {
                std::lock_guard<std::mutex> lock(datos_);
                fallo = true;
                progreso_.codigo = "ESTADO_INVALIDO";
                progreso_.resultado_orden = "no_enviada";
                progreso_.orden_actual = i + 1;
                progreso_.linea_actual = programa.ordenes()[i].linea;
                progreso_.mensaje = "Cierre del servidor: no se enviaron las ordenes restantes.";
                break;
            }
            {
                std::lock_guard<std::mutex> lock(datos_);
                progreso_.orden_actual = i + 1;
                progreso_.linea_actual = programa.ordenes()[i].linea;
                progreso_.resultado_orden = "pendiente";
            }
            try {
                robot_.ejecutarOrdenAutomatica(programa.ordenes()[i]);
                std::lock_guard<std::mutex> lock(datos_);
                ++progreso_.confirmadas;
                progreso_.resultado_orden = "confirmada";
            } catch (const ErrorOrdenAutomatica& e) {
                std::lock_guard<std::mutex> lock(datos_);
                fallo = true;
                progreso_.codigo = e.resultado() == ResultadoOrdenAutomatica::DESCONOCIDO
                                      ? "ERROR_INTERNO" : "ESTADO_INVALIDO";
                progreso_.resultado_orden = e.resultado() == ResultadoOrdenAutomatica::NO_ENVIADA ? "no_enviada"
                    : e.resultado() == ResultadoOrdenAutomatica::ERROR_CONTROLADOR ? "error_controlador" : "desconocido";
                progreso_.mensaje = e.what();
                break;
            } catch (const std::exception&) {
                std::lock_guard<std::mutex> lock(datos_);
                fallo = true;
                progreso_.codigo = "ESTADO_INVALIDO";
                progreso_.resultado_orden = "no_enviada";
                progreso_.mensaje = "No se pudo preparar la orden; la secuencia se detuvo.";
                break;
            }
        }
        std::lock_guard<std::mutex> lock(datos_);
        if (!fallo) {
            progreso_.mensaje = "Secuencia confirmada segun protocolo; no es verificacion fisica.";
        }
        final = progreso_;
        final.estado = fallo ? "fallida" : "finalizada";
    }
    try {
        registro_.registrarEjecucion(final.ejecucion_id, actor,
            final.estado == "finalizada" ? "tarea.finalizada" : "tarea.fallida", final.codigo,
            "Tarea=" + final.tarea_id + "; confirmadas=" + std::to_string(final.confirmadas) +
            "/" + std::to_string(final.total) + "; orden=" + std::to_string(final.orden_actual) +
            "; resultado=" + final.resultado_orden);
    } catch (const std::exception&) {
        final.registro_completo = false;
        final.mensaje += " Advertencia: no se pudo registrar el resultado final.";
        std::cerr << "Advertencia: fallo el registro final de una ejecucion.\n";
    }
    {
        std::lock_guard<std::mutex> lock(datos_);
        progreso_ = final;
        activa_ = false; // Ultima publicacion; no hay mas acceso a Robot o CSV desde este hilo.
    }
}

void EjecutorTareas::cerrar() {
    {
        std::lock_guard<std::mutex> lock(datos_);
        cerrando_ = true;
        detener_.store(true);
        if (activa_) progreso_.mensaje = "Cierre del servidor: se espera la orden en curso.";
    }
    if (hilo_.joinable()) hilo_.join();
}
} // namespace robotrpc

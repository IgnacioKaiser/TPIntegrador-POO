#include "robotrpc/servicio_protegido.hpp"
#include <string>
#include <variant>
#include <cstdint>
#include <iostream>
#include <limits>
#include <set>

namespace robotrpc {
namespace {

bool nombreUsuarioValido(const std::string& nombre) {
    if (nombre.empty() || nombre.size() > 64) return false;
    for (char c : nombre) {
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-'))
            return false;
    }
    return true;
}

MensajeRPC error(const MensajeRPC& solicitud, const std::string& codigo,
                 const std::string& mensaje) {
    return MensajeRPC::respuestaA(solicitud, {{"mensaje", mensaje}}, codigo);
}

EstructuraRPC datosTarea(const Tarea& tarea, const std::string& propietario) {
    return {{"id", tarea.id}, {"nombre", tarea.nombre}, {"propietario", propietario}};
}

std::int32_t enteroRpc(std::size_t n) {
    const auto maximo = static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max());
    return static_cast<std::int32_t>(n > maximo ? maximo : n);
}

} // namespace

MensajeRPC ServicioProtegido::atender(const MensajeRPC& solicitud) const {
    std::string usuarioVerificado;
    try {
        MensajeRPC respuesta = atenderInterno(solicitud, usuarioVerificado);
        if (registro_) {
            try {
                // La escucha actual es exclusivamente por loopback.
                // Nunca tomar el usuario declarado como identidad verificada.
                registro_->registrarPeticion(solicitud.id(), usuarioVerificado,
                    "127.0.0.1", solicitud.operacion(), respuesta.codigo());
                if (solicitud.operacion() == "conectar" ||
                    solicitud.operacion() == "desconectar")
                    registro_->registrarEvento("Robot", solicitud.operacion(),
                        respuesta.codigo(), "Resultado de operacion de conexion serie");
            } catch (const std::exception& e) {
                std::cerr << "Advertencia: no se pudo registrar actividad: " << e.what() << '\n';
            }
        }
        return respuesta;
    } catch (...) {
        if (registro_) {
            try {
                registro_->registrarPeticion(solicitud.id(), usuarioVerificado,
                    "127.0.0.1", solicitud.operacion(), "ERROR_INTERNO");
            } catch (...) { /* Se mantiene el error original. */ }
        }
        throw;
    }
}

MensajeRPC ServicioProtegido::atenderInterno(
    const MensajeRPC& solicitud, std::string& usuarioVerificado) const {
    if (solicitud.tipo() != "solicitud")
        throw MensajeInvalido("El servidor recibe solicitudes, no respuestas.");

    Usuario solicitante;
    try {
        solicitante = autenticador_.validar(solicitud);
        usuarioVerificado = solicitante.nombre;
    } catch (const ErrorAutenticacion& e) {
        return error(solicitud, e.codigo(), e.what());
    } catch (const ErrorUsuarios&) {
        return error(solicitud, "ERROR_INTERNO", "No se pudo consultar la base de usuarios.");
    }

    // Quitar las credenciales antes de procesar la operacion solicitada.
    auto parametros = solicitud.parametros();
    parametros.erase("credenciales");
    const auto limpia = MensajeRPC::solicitud(solicitud.id(), solicitud.operacion(), parametros);
    if (solicitud.operacion() == "tarea.ejecutar" || solicitud.operacion() == "tarea.estado") {
        const bool iniciar = solicitud.operacion() == "tarea.ejecutar";
        const std::string campo = iniciar ? "tarea_id" : "ejecucion_id";
        if (parametros.size() != 1 || !parametros.count(campo) ||
            !std::holds_alternative<std::string>(parametros.at(campo).dato()))
            return error(solicitud, "VALIDACION", "Se requiere solamente " + campo + " de tipo string.");
        if (!ejecutor_) return error(solicitud, "ERROR_INTERNO", "Ejecutor no configurado.");
        const auto id = parametros.at(campo).obtener<std::string>();
        if (!iniciar) {
            try { MensajeRPC::solicitud(id, "ping"); }
            catch (const MensajeInvalido&) { return error(solicitud, "VALIDACION", "Identificador de ejecucion invalido."); }
        }
        try {
            ProgresoTarea progreso;
            if (iniciar) {
                const auto contenido = tareas_.leer(solicitante, id);
                ProgramaTarea programa(contenido.contenido);
                progreso = ejecutor_->iniciar(solicitante, contenido.tarea, std::move(programa), solicitud.id());
            } else progreso = ejecutor_->consultar(solicitante, id);
            return MensajeRPC::respuestaA(solicitud,
                {{"ejecucion_id", progreso.ejecucion_id}, {"tarea_id", progreso.tarea_id},
                 {"estado", progreso.estado}, {"total", enteroRpc(progreso.total)},
                 {"confirmadas", enteroRpc(progreso.confirmadas)}, {"orden_actual", enteroRpc(progreso.orden_actual)},
                 {"linea_actual", enteroRpc(progreso.linea_actual)}, {"resultado_orden", progreso.resultado_orden},
                 {"codigo_ejecucion", progreso.codigo}, {"mensaje", progreso.mensaje},
                 {"registro_completo", progreso.registro_completo}});
        } catch (const ErrorEjecucion& e) {
            return error(solicitud, e.codigo(), e.what());
        } catch (const ErrorTarea& e) {
            const char* codigo = e.tipo() == TipoErrorTarea::VALIDACION ? "VALIDACION"
                : e.tipo() == TipoErrorTarea::ACCESO_DENEGADO ? "PERMISO_DENEGADO"
                : e.tipo() == TipoErrorTarea::NO_ENCONTRADA ? "RECURSO_NO_ENCONTRADO" : "ERROR_INTERNO";
            return error(solicitud, codigo, e.what());
        } catch (const std::invalid_argument& e) {
            return error(solicitud, "VALIDACION", e.what());
        } catch (const std::exception&) {
            return error(solicitud, "ERROR_INTERNO", "No se pudo preparar la ejecucion.");
        }
    }
    if (solicitud.operacion() == "informe.propio") {
        std::size_t offset = 0, limite = 50;
        for (const auto& campo : parametros) {
            if (campo.first != "offset" && campo.first != "limite")
                return error(solicitud, "VALIDACION", "Solo se aceptan offset y limite.");
            const auto* n = std::get_if<std::int32_t>(&campo.second.dato());
            if (!n || *n < 0)
                return error(solicitud, "VALIDACION", "La paginacion requiere enteros no negativos.");
            if (campo.first == "offset") offset = static_cast<std::size_t>(*n);
            else limite = static_cast<std::size_t>(*n);
        }
        if (!registro_) return error(solicitud, "ERROR_INTERNO", "Registro no configurado.");
        try {
            const auto informe = registro_->actividadPropia(solicitante.nombre, offset, limite);
            ValorRPC::Arreglo registros;
            for (const auto& r : informe.pagina.registros) {
                registros.emplace_back(EstructuraRPC{
                    {"fecha_hora", r.fecha_hora}, {"tipo", r.tipo}, {"id_solicitud", r.id_solicitud},
                    {"usuario", r.usuario}, {"nodo_cliente", r.nodo_cliente}, {"modulo", r.modulo},
                    {"operacion", r.operacion}, {"codigo", r.codigo}, {"detalle", r.detalle}});
            }
            // Reutilizar la consulta de estado; no generar otra peticion ni mover el robot.
            const auto estado = servicio_.atender(MensajeRPC::solicitud(solicitud.id(), "estado"));
            auto datosEstado = estado.resultado();
            datosEstado.erase("reporte_firmware");
            if (estado.codigo() != "OK") {
                datosEstado["mensaje"] = "No se pudo completar la consulta de estado.";
                datosEstado["posicion"] = EstructuraRPC{
                    {"disponible", false}, {"mensaje", "Posicion no disponible."}};
            }
            return MensajeRPC::respuestaA(solicitud,
                {{"usuario", solicitante.nombre}, {"desde", informe.desde},
                 {"referencia", informe.referencia}, {"inicio_actividad", informe.inicio_actividad},
                 {"estado", datosEstado}, {"codigo_estado", estado.codigo()},
                 {"registros", registros}, {"total", enteroRpc(informe.pagina.total)},
                 {"errores", enteroRpc(informe.errores)}, {"offset", enteroRpc(offset)},
                 {"mostrados", enteroRpc(registros.size())}});
        } catch (const std::invalid_argument& e) {
            return error(solicitud, "VALIDACION", e.what());
        } catch (const std::exception&) {
            return error(solicitud, "ERROR_INTERNO", "No se pudo consultar la actividad propia.");
        }
    }
    if (solicitud.operacion() == "log.consultar" ||
        solicitud.operacion() == "informe.usuarios") {
        if (solicitante.rol != Rol::ADMINISTRADOR)
            return error(solicitud, "PERMISO_DENEGADO",
                         "Solo el administrador puede consultar logs e informes.");
        if (!registro_)
            return error(solicitud, "ERROR_INTERNO", "Registro no configurado.");

        const bool esLog = solicitud.operacion() == "log.consultar";
        const std::set<std::string> permitidos = esLog
            ? std::set<std::string>{"usuario", "desde", "hasta", "tipo", "codigo", "offset", "limite"}
            : std::set<std::string>{"usuario", "desde", "hasta", "codigo"};
        FiltroActividad filtro;
        for (const auto& campo : parametros) {
            if (!permitidos.count(campo.first))
                return error(solicitud, "VALIDACION", "Filtro desconocido: " + campo.first);
            if (campo.first == "offset" || campo.first == "limite") {
                const auto* n = std::get_if<std::int32_t>(&campo.second.dato());
                if (!n || *n < 0)
                    return error(solicitud, "VALIDACION", "offset y limite deben ser enteros no negativos.");
                if (campo.first == "offset") filtro.offset = static_cast<std::size_t>(*n);
                else filtro.limite = static_cast<std::size_t>(*n);
            } else {
                const auto* s = std::get_if<std::string>(&campo.second.dato());
                if (!s) return error(solicitud, "VALIDACION", "Los filtros deben ser texto.");
                if (campo.first == "usuario") filtro.usuario = *s;
                else if (campo.first == "desde") filtro.desde = *s;
                else if (campo.first == "hasta") filtro.hasta = *s;
                else if (campo.first == "tipo") filtro.tipo = *s;
                else if (campo.first == "codigo") filtro.codigo = *s;
            }
        }
        try {
            if (!esLog) {
                const auto resumen = registro_->informeUsuarios(filtro);
                ValorRPC::Arreglo usuarios;
                std::size_t total = 0, exitosas = 0, rechazadas = 0;
                for (const auto& r : resumen) {
                    usuarios.emplace_back(EstructuraRPC{
                        {"usuario", r.usuario}, {"peticiones", enteroRpc(r.peticiones)},
                        {"exitosas", enteroRpc(r.exitosas)}, {"rechazadas", enteroRpc(r.rechazadas)}});
                    total += r.peticiones; exitosas += r.exitosas; rechazadas += r.rechazadas;
                }
                return MensajeRPC::respuestaA(solicitud,
                    {{"usuarios", usuarios}, {"total_peticiones", enteroRpc(total)},
                     {"total_ok", enteroRpc(exitosas)}, {"total_errores", enteroRpc(rechazadas)}});
            }
            const auto pagina = registro_->consultar(filtro);
            ValorRPC::Arreglo registros;
            for (const auto& r : pagina.registros) {
                registros.emplace_back(EstructuraRPC{
                    {"fecha_hora",r.fecha_hora}, {"tipo",r.tipo}, {"id_solicitud",r.id_solicitud},
                    {"usuario",r.usuario}, {"nodo_cliente",r.nodo_cliente}, {"modulo",r.modulo},
                    {"operacion",r.operacion}, {"codigo",r.codigo}, {"detalle",r.detalle}});
            }
            return MensajeRPC::respuestaA(solicitud,
                {{"registros", registros}, {"total", enteroRpc(pagina.total)},
                 {"offset", enteroRpc(filtro.offset)}, {"mostrados", enteroRpc(registros.size())}});
        } catch (const std::invalid_argument& e) {
            return error(solicitud, "VALIDACION", e.what());
        } catch (const std::exception&) {
            return error(solicitud, "ERROR_INTERNO", "No se pudo consultar el CSV de actividad.");
        }
    }

    if (solicitud.operacion() == "tarea.subir" || solicitud.operacion() == "tarea.listar") {
        try {
            if (solicitud.operacion() == "tarea.subir") {
                if (parametros.size() != 2 || !parametros.count("nombre") || !parametros.count("contenido"))
                    return error(solicitud, "VALIDACION", "tarea.subir requiere solo nombre y contenido.");
                const auto* nombre = std::get_if<std::string>(&parametros.at("nombre").dato());
                const auto* contenido = std::get_if<std::string>(&parametros.at("contenido").dato());
                if (!nombre || !contenido)
                    return error(solicitud, "VALIDACION", "nombre y contenido deben ser texto.");
                // El propietario es el usuario autenticado, no un parametro remoto.
                const auto tarea = tareas_.guardar(solicitante, *nombre, *contenido);
                auto datos = datosTarea(tarea, solicitante.nombre);
                datos.emplace("mensaje", "Tarea almacenada. No se ejecutaron ordenes.");
                return MensajeRPC::respuestaA(solicitud, datos);
            }
            if (!parametros.empty())
                return error(solicitud, "VALIDACION", "tarea.listar no acepta parametros adicionales.");
            const auto tareas = tareas_.listar(solicitante);
            // Resolver nombres sin duplicarlos en los archivos ni exponer otras cuentas.
            const auto propietarios = solicitante.rol == Rol::ADMINISTRADOR
                ? usuarios_.listar() : std::vector<Usuario>{solicitante};
            ValorRPC::Arreglo listado;
            for (const auto& tarea : tareas) {
                std::string propietario;
                for (const auto& usuario : propietarios)
                    if (usuario.id == tarea.propietario_id) { propietario = usuario.nombre; break; }
                if (propietario.empty())
                    throw ErrorTarea(TipoErrorTarea::ALMACENAMIENTO,
                                     "Una tarea no tiene propietario en esta base; revise el respaldo conjunto.");
                listado.emplace_back(datosTarea(tarea, propietario));
            }
            return MensajeRPC::respuestaA(solicitud, {{"tareas", listado}});
        } catch (const ErrorTarea& e) {
            const char* codigo = "ERROR_INTERNO";
            if (e.tipo() == TipoErrorTarea::VALIDACION) codigo = "VALIDACION";
            else if (e.tipo() == TipoErrorTarea::DUPLICADA) codigo = "ESTADO_INVALIDO";
            else if (e.tipo() == TipoErrorTarea::ACCESO_DENEGADO) codigo = "PERMISO_DENEGADO";
            return error(solicitud, codigo, e.what());
        } catch (const ErrorUsuarios&) {
            return error(solicitud, "ERROR_INTERNO", "No se pudieron consultar los propietarios de las tareas.");
        }
    }

    if (solicitud.operacion() == "usuario.actual") {
        if (!parametros.empty())
            return error(solicitud, "VALIDACION",
                         "usuario.actual no acepta parametros adicionales.");

        // La identidad proviene de la autenticacion, nunca de datos del cliente.
        return MensajeRPC::respuestaA(solicitud,
            {{"usuario", solicitante.nombre},
             {"rol", solicitante.rol == Rol::ADMINISTRADOR ? "ADMINISTRADOR" : "OPERADOR"}});
    }

    if ((solicitud.operacion() == "conectar" ||
         solicitud.operacion() == "desconectar") &&
        solicitante.rol != Rol::ADMINISTRADOR)
        return error(solicitud, "PERMISO_DENEGADO",
                     "Solo el administrador puede conectar o desconectar el robot.");

    if (solicitud.operacion() == "usuario.habilitar" ||
        solicitud.operacion() == "usuario.deshabilitar") {

        // La comprobacion debe hacerse EN EL SERVIDOR, no solo en la CLI.
        if (solicitante.rol != Rol::ADMINISTRADOR)
            return error(solicitud, "PERMISO_DENEGADO",
                         "Solo el administrador puede habilitar o deshabilitar operadores.");

        const auto it = parametros.find("usuario");
        if (parametros.size() != 1 || it == parametros.end())
            return error(solicitud, "VALIDACION", "Se requiere solamente el parametro usuario.");

        const auto* nombre = std::get_if<std::string>(&it->second.dato());
        if (!nombre || !nombreUsuarioValido(*nombre))
            return error(solicitud, "VALIDACION", "El parametro usuario debe ser un nombre valido.");

        // Impide modificar la cuenta ADMINISTRADOR aun con solicitud autentica.
        const bool habilitar = solicitud.operacion() == "usuario.habilitar";
        try {
            const auto existente = usuarios_.buscar(*nombre);
            if (!existente)
                return error(solicitud, "RECURSO_NO_ENCONTRADO", "El usuario no existe.");
            if (existente->rol != Rol::OPERADOR)
                return error(solicitud, "PERMISO_DENEGADO",
                             "Solo se pueden habilitar o deshabilitar operadores.");

            if (!usuarios_.cambiarHabilitacionOperador(*nombre, habilitar))
                return error(solicitud, "ESTADO_INVALIDO",
                             "El operador cambio o dejo de existir antes de actualizarlo.");

            return MensajeRPC::respuestaA(solicitud,
                {{"usuario", *nombre},
                 {"habilitado", habilitar},
                 {"mensaje", habilitar ? "Operador habilitado." : "Operador deshabilitado."}});
        } catch (const ErrorUsuarios&) {
            return error(solicitud, "ERROR_INTERNO", "No se pudo actualizar la base de usuarios.");
        }
    }

    // Las credenciales se eliminan antes de delegar cualquier operacion.
    return servicio_.atender(limpia);
}
} // namespace robotrpc

#include "robotrpc/servicio_protegido.hpp"
#include <string>
#include <variant>

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

} // namespace

MensajeRPC ServicioProtegido::atender(const MensajeRPC& solicitud) const {
    if (solicitud.tipo() != "solicitud")
        throw MensajeInvalido("El servidor recibe solicitudes, no respuestas.");

    Usuario solicitante;
    try {
        solicitante = autenticador_.validar(solicitud);
    } catch (const ErrorAutenticacion& e) {
        return error(solicitud, e.codigo(), e.what());
    } catch (const ErrorUsuarios&) {
        return error(solicitud, "ERROR_INTERNO", "No se pudo consultar la base de usuarios.");
    }

    // Quitar las credenciales antes de procesar la operacion solicitada.
    auto parametros = solicitud.parametros();
    parametros.erase("credenciales");
    const auto limpia = MensajeRPC::solicitud(solicitud.id(), solicitud.operacion(), parametros);

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

    // ping y eco mantienen su implementacion previa y nunca ven la clave.
    return servicio_.atender(limpia);
}
} // namespace robotrpc

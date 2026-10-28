#include "robotrpc/autenticador.hpp"
#include <variant>

namespace robotrpc {
Usuario Autenticador::validar(const MensajeRPC& solicitud) const {
    const auto& parametros = solicitud.parametros();
    const auto campo = parametros.find("credenciales");
    if (campo == parametros.end())
        throw ErrorAutenticacion("NO_AUTENTICADO", "Faltan las credenciales.");
    const auto* datos = std::get_if<EstructuraRPC>(&campo->second.dato());
    if (!datos || datos->size() != 2 || !datos->count("usuario") || !datos->count("clave"))
        throw ErrorAutenticacion("VALIDACION", "Credenciales requiere usuario y clave.");
    const auto* nombre = std::get_if<std::string>(&datos->at("usuario").dato());
    const auto* clave = std::get_if<std::string>(&datos->at("clave").dato());
    if (!nombre || !clave)
        throw ErrorAutenticacion("VALIDACION", "Usuario y clave deben ser texto.");
    // No se conserva una sesion: se consulta la base en cada solicitud.
    const auto usuario = usuarios_.buscar(*nombre);
    if (!usuario || !usuarios_.verificarClave(*nombre, *clave))
        throw ErrorAutenticacion("NO_AUTENTICADO", "Usuario o clave incorrectos.");
    if (!usuario->habilitado)
        throw ErrorAutenticacion("PERMISO_DENEGADO", "La cuenta esta deshabilitada.");
    return *usuario;
}
}

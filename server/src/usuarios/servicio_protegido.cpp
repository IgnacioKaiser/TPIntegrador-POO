#include "robotrpc/servicio_protegido.hpp"

namespace robotrpc {
MensajeRPC ServicioProtegido::atender(const MensajeRPC& solicitud) const {
    if (solicitud.tipo() != "solicitud")
        throw MensajeInvalido("El servidor recibe solicitudes, no respuestas.");
    try {
        autenticador_.validar(solicitud);
    } catch (const ErrorAutenticacion& error) {
        return MensajeRPC::respuestaA(solicitud, {{"mensaje", error.what()}}, error.codigo());
    } catch (const ErrorUsuarios&) {
        return MensajeRPC::respuestaA(solicitud, {{"mensaje", "No se pudo consultar la base de usuarios."}}, "ERROR_INTERNO");
    }
    // La operacion recibe solo sus parametros, sin claves ni hashes.
    auto parametros = solicitud.parametros();
    parametros.erase("credenciales");
    const auto limpia = MensajeRPC::solicitud(solicitud.id(), solicitud.operacion(), parametros);
    return servicio_.atender(limpia);
}
}

#include "robotrpc/servicio.hpp"

namespace robotrpc {

MensajeRPC ServicioPrueba::atender(const MensajeRPC& solicitud) const {
    if (solicitud.tipo() != "solicitud")
        throw MensajeInvalido("El servidor recibe solicitudes, no respuestas.");
    if (solicitud.operacion() == "ping")
        return MensajeRPC::respuestaA(solicitud, "pong: servidor C++ disponible");
    if (solicitud.operacion() == "eco")
        return MensajeRPC::respuestaA(solicitud, solicitud.contenido());
    return MensajeRPC::respuestaA(solicitud, "Operacion no soportada en v0.2.",
                                  "OPERACION_NO_SOPORTADA");
}

} // namespace robotrpc

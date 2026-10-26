#include "robotrpc/servicio.hpp"

#include <string>
#include <variant>

namespace robotrpc {
namespace {

MensajeRPC errorDeAplicacion(
    const MensajeRPC& solicitud,
    const std::string& codigo,
    const std::string& descripcion
) {
    return MensajeRPC::respuestaA(
        solicitud,
        {{"mensaje", descripcion}},
        codigo
    );
}

} // namespace

MensajeRPC ServicioPrueba::atender(
    const MensajeRPC& solicitud
) const {

    if (solicitud.tipo() != "solicitud")
        throw MensajeInvalido(
            "El servidor recibe solicitudes, no respuestas.");

    if (solicitud.operacion() == "ping") {

        if (!solicitud.parametros().empty())
            return errorDeAplicacion(
                solicitud,
                "VALIDACION",
                "ping no recibe parametros."
            );

        return MensajeRPC::respuestaA(
            solicitud,
            {{"mensaje", "pong: servidor C++ disponible"}}
        );
    }

    if (solicitud.operacion() == "eco") {

        const auto& parametros = solicitud.parametros();
        const auto encontrado = parametros.find("texto");

        if (parametros.size() != 1 ||
            encontrado == parametros.end())
            return errorDeAplicacion(
                solicitud,
                "VALIDACION",
                "eco requiere solamente el parametro texto."
            );

        const auto* texto =
            std::get_if<std::string>(
                &encontrado->second.dato());

        if (!texto)
            return errorDeAplicacion(
                solicitud,
                "VALIDACION",
                "El parametro texto debe ser string."
            );

        return MensajeRPC::respuestaA(
            solicitud,
            {{"texto", *texto}}
        );
    }

    return errorDeAplicacion(
        solicitud,
        "OPERACION_NO_SOPORTADA",
        "Operacion no soportada."
    );
}

} // namespace robotrpc
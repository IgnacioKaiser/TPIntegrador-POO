#include "robotrpc/mensaje.hpp"
#include "robotrpc/servicio.hpp"
#include <iostream>
#include <stdexcept>

void exigir(bool condicion, const char* detalle) {
    if (!condicion) throw std::runtime_error(detalle);
}

template <typename Accion>
void exigirInvalido(Accion accion) {
    try { accion(); }
    catch (const robotrpc::MensajeInvalido&) { return; }
    throw std::runtime_error("Se esperaba MensajeInvalido.");
}

int main() {
    try {
        using robotrpc::MensajeRPC;
        robotrpc::ServicioPrueba servicio;
        const auto solicitud = MensajeRPC::solicitud("prueba_1", "eco", "Mecatrónica <robot> & pinza");
        exigir(MensajeRPC::deserializar(solicitud.serializar()).contenido() == solicitud.contenido(),
               "La serializacion altera el mensaje.");
        const auto respuesta = servicio.atender(solicitud);
        exigir(respuesta.id() == solicitud.id() && respuesta.tipo() == "respuesta" &&
               respuesta.contenido() == solicitud.contenido(), "Eco incorrecto.");
        exigir(servicio.atender(MensajeRPC::solicitud("ping-1", "ping")).codigo() == "OK", "Ping incorrecto.");
        exigir(servicio.atender(MensajeRPC::solicitud("desconocida", "mover")).codigo() ==
               "OPERACION_NO_SOPORTADA", "Se acepto una operacion desconocida.");
        exigirInvalido([&] { servicio.atender(respuesta); });
        auto campos = solicitud.serializar();
        campos.erase("contenido");
        exigirInvalido([&] { MensajeRPC::deserializar(campos); });
        campos = solicitud.serializar();
        campos["version"] = "2";
        exigirInvalido([&] { MensajeRPC::deserializar(campos); });
        exigirInvalido([] { MensajeRPC::solicitud("", "eco"); });
        exigirInvalido([] { MensajeRPC::solicitud("id", "eco", std::string(8193, 'x')); });
        exigirInvalido([] { MensajeRPC::solicitud("id", "eco", "🤖"); });
        exigirInvalido([] { MensajeRPC::solicitud("id", "eco", std::string(1, '\0')); });
        std::cout << "C++: 11 comprobaciones de dominio/servicio correctas.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

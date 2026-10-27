#include "robotrpc/mensaje.hpp"
#include "robotrpc/servicio.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <variant>

void exigir(bool condicion, const char* descripcion) {
    if (!condicion) throw std::runtime_error(descripcion);
}

template <typename Accion>
void exigirInvalido(Accion accion) {
    try { accion(); }
    catch (const robotrpc::MensajeInvalido&) { return; }
    throw std::runtime_error("Se esperaba MensajeInvalido.");
}

int main() {
    try {
        using namespace robotrpc;
        Robot robot;
        ServicioRobot servicio(robot);

        const auto solicitud = MensajeRPC::solicitud(
            "prueba_1", "eco", {{"texto", "Mecatronica <robot> & pinza"}});
        const auto recuperado = MensajeRPC::deserializar(solicitud.serializar());
        exigir(recuperado.parametros().at("texto").obtener<std::string>() ==
                   solicitud.parametros().at("texto").obtener<std::string>(),
               "Serializacion alterada.");

        const auto respuesta = servicio.atender(solicitud);
        exigir(respuesta.id() == solicitud.id() &&
                   respuesta.tipo() == "respuesta" && respuesta.codigo() == "OK",
               "Identificacion de respuesta incorrecta.");
        exigir(respuesta.resultado().at("texto").obtener<std::string>() ==
                   solicitud.parametros().at("texto").obtener<std::string>(),
               "Eco incorrecto.");
        exigir(servicio.atender(MensajeRPC::solicitud("p1", "ping")).codigo() == "OK",
               "Ping incorrecto.");
        exigir(servicio.atender(MensajeRPC::solicitud("p2", "ping", {{"extra", 1}})).codigo() ==
                   "VALIDACION", "Ping debe rechazar parametros.");
        exigir(servicio.atender(MensajeRPC::solicitud("e1", "eco")).codigo() ==
                   "VALIDACION", "Eco debe requerir texto.");
        exigir(servicio.atender(MensajeRPC::solicitud("e2", "eco", {{"texto", 3}})).codigo() ==
                   "VALIDACION", "Eco debe requerir string.");
        exigir(servicio.atender(MensajeRPC::solicitud("u1", "operacion_desconocida")).codigo() ==
                   "OPERACION_NO_SOPORTADA", "Operacion desconocida incorrecta.");
        const auto estadoDesconectado =
            servicio.atender(MensajeRPC::solicitud("u2", "estado"));
        exigir(estadoDesconectado.codigo() == "OK" &&
                   !estadoDesconectado.resultado().at("conectado").obtener<bool>(),
               "Estado debe informar que el robot esta desconectado.");
        const auto& posicion = estadoDesconectado.resultado()
                                   .at("posicion").obtener<EstructuraRPC>();
        exigir(!posicion.at("disponible").obtener<bool>() &&
                   !posicion.at("mensaje").obtener<std::string>().empty(),
               "Estado debe indicar que la posicion no esta disponible.");
        const auto& actividad = estadoDesconectado.resultado()
                                    .at("actividad").obtener<EstructuraRPC>();
        exigir(!actividad.at("motores").obtener<EstructuraRPC>()
                    .at("disponible").obtener<bool>(),
               "Estado debe indicar que la actividad no esta disponible.");
        exigir(servicio.atender(MensajeRPC::solicitud("u3", "mover")).codigo() ==
                   "OPERACION_NO_SOPORTADA",
               "El servicio no debe exponer comandos de movimiento.");
        exigir(servicio.atender(MensajeRPC::solicitud(
                   "u5", "activar_motores", {{"extra", true}})).codigo() ==
                   "VALIDACION", "Motores debe rechazar parametros desconocidos.");
        exigir(servicio.atender(MensajeRPC::solicitud("u6", "ayuda")).codigo() ==
                   "OK", "Ayuda debe estar disponible sin conexion.");
        const auto respuestaAyuda =
            servicio.atender(MensajeRPC::solicitud("u8", "ayuda"));
        const auto& ayuda = respuestaAyuda.resultado().at("comandos")
                                .obtener<std::string>();
        exigir(ayuda.find("M17") != std::string::npos &&
                   ayuda.find("M18") != std::string::npos &&
                   ayuda.find("G0") == std::string::npos,
               "Ayuda debe limitarse a comandos de control implementados.");
        exigir(servicio.atender(MensajeRPC::solicitud("u7", "desconectar")).codigo() ==
                   "OK", "Desconectar debe poder ejecutarse sin conexion.");

        exigirInvalido([&] { servicio.atender(respuesta); });
        auto campos = solicitud.serializar();
        campos.erase("parametros");
        exigirInvalido([&] { MensajeRPC::deserializar(campos); });
        campos = solicitud.serializar();
        campos["version"] = "2";
        exigirInvalido([&] { MensajeRPC::deserializar(campos); });
        exigirInvalido([] { MensajeRPC::solicitud("", "eco"); });
        exigirInvalido([] { MensajeRPC::solicitud("id", "eco", {{"3x", 1}}); });
        exigirInvalido([] { MensajeRPC::solicitud("id", "eco", {{"texto", 1.0 / 0.0}}); });

        const auto tipados = MensajeRPC::solicitud(
            "tipos", "eco", {{"lista", ValorRPC::Arreglo{
                 ValorRPC(std::int32_t{12}), ValorRPC(2.5), ValorRPC(true),
                 ValorRPC(EstructuraRPC{{"interno", "dato"}})}}});
        const auto copiado = MensajeRPC::deserializar(tipados.serializar());
        exigir(copiado.parametros().at("lista").obtener<ValorRPC::Arreglo>().size() == 4,
               "Los valores anidados se perdieron.");

        std::cout << "Comprobaciones del modelo y el servicio: OK\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

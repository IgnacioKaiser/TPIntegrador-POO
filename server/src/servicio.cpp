#include "robotrpc/servicio.hpp"

#include <cmath>
#include <exception>
#include <cmath>
#include <optional>
#include <regex>
#include <set>
#include <string>
#include <variant>

namespace robotrpc {
namespace {

class ParametrosInvalidos : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

void validarParametros(
    const EstructuraRPC& parametros,
    const std::set<std::string>& esperados
) {
    for (const auto& parametro : parametros) {
        if (esperados.count(parametro.first) == 0)
            throw ParametrosInvalidos(
                "Parametro no reconocido: " + parametro.first + ".");
    }
    for (const auto& esperado : esperados) {
        if (parametros.count(esperado) == 0)
            throw ParametrosInvalidos(
                "Falta el parametro requerido: " + esperado + ".");
    }
}

void validarSinParametros(const EstructuraRPC& parametros) {
    if (!parametros.empty())
        throw ParametrosInvalidos(
            "La operacion no acepta parametros.");
}

const std::string& texto(
    const EstructuraRPC& parametros,
    const std::string& nombre
) {
    const auto* valor = std::get_if<std::string>(
        &parametros.at(nombre).dato());
    if (!valor)
        throw ParametrosInvalidos(
            "El parametro " + nombre + " debe ser string.");
    return *valor;
}

MensajeRPC respuestaError(
    const MensajeRPC& solicitud,
    const std::string& codigo,
    const std::string& mensaje
) {
    return MensajeRPC::respuestaA(
        solicitud, {{"mensaje", ValorRPC(mensaje)}}, codigo);
}

EstructuraRPC datoNoDisponible(const std::string& mensaje) {
    return {
        {"disponible", false},
        {"mensaje", mensaje}
    };
}

EstructuraRPC datoBooleano(
    const std::string& respuesta,
    const std::string& activo,
    const std::string& inactivo,
    const std::string& nombre
) {
    if (respuesta.find(activo) != std::string::npos)
        return {{"disponible", true}, {"activo", true}};
    if (respuesta.find(inactivo) != std::string::npos)
        return {{"disponible", true}, {"activo", false}};
    return datoNoDisponible(
        "El firmware no informo el estado de " + nombre + ".");
}

EstructuraRPC datoModo(const std::string& respuesta) {
    if (respuesta.find("ABSOLUTE MODE") != std::string::npos)
        return {{"disponible", true}, {"valor", "absoluto"}};
    if (respuesta.find("RELATIVE MODE") != std::string::npos)
        return {{"disponible", true}, {"valor", "relativo"}};
    return datoNoDisponible("El firmware no informo el modo de coordenadas.");
}

EstructuraRPC posicionRobot(const std::string& respuesta) {
    static const std::regex formato(
        R"(CURRENT POSITION:\s*\[X:\s*([-+]?(?:[0-9]*\.)?[0-9]+(?:[eE][-+]?[0-9]+)?)\s+Y:\s*([-+]?(?:[0-9]*\.)?[0-9]+(?:[eE][-+]?[0-9]+)?)\s+Z:\s*([-+]?(?:[0-9]*\.)?[0-9]+(?:[eE][-+]?[0-9]+)?)\s+E:\s*([-+]?(?:[0-9]*\.)?[0-9]+(?:[eE][-+]?[0-9]+)?)\s*\])"
    );
    std::smatch coincidencia;
    if (!std::regex_search(respuesta, coincidencia, formato))
        return datoNoDisponible(
            "El firmware no informo la posicion actual.");

    try {
        EstructuraRPC posicion{
            {"disponible", true},
            {"unidad", "mm"}
        };
        const char* ejes[] = {"x", "y", "z", "e"};
        for (std::size_t i = 0; i < 4; ++i) {
            const double valor = std::stod(coincidencia[i + 1].str());
            if (!std::isfinite(valor))
                return datoNoDisponible(
                    "El firmware informo una posicion invalida.");
            posicion.emplace(ejes[i], valor);
        }
        return posicion;
    } catch (const std::exception&) {
        return datoNoDisponible(
            "No se pudo interpretar la posicion informada por el firmware.");
    }
}

EstructuraRPC estadoRobotDesconocido() {
    return {
        {"motores", datoNoDisponible(
             "El estado de los motores requiere consultar el firmware.")},
        {"ventilador", datoNoDisponible(
             "El estado del ventilador requiere consultar el firmware.")},
        {"modo_coordenadas", datoNoDisponible(
             "El modo de coordenadas requiere consultar el firmware.")}
    };
}

EstructuraRPC crearEstado(
    bool conectado,
    const EstructuraRPC& posicion,
    const EstructuraRPC& actividad
) {
    return {
        {"conectado", conectado},
        {"posicion", posicion},
        {"actividad", actividad}
    };
}

} // namespace

MensajeRPC ServicioRobot::atender(const MensajeRPC& solicitud) const {
    if (solicitud.tipo() != "solicitud")
        throw MensajeInvalido(
            "El servidor recibe solicitudes, no respuestas.");

    const auto& parametros = solicitud.parametros();
    const std::string& operacion = solicitud.operacion();

    try {
        if (operacion == "ping") {
            validarSinParametros(parametros);
            return MensajeRPC::respuestaA(
                solicitud,
                {{"mensaje", "pong: servidor C++ disponible"}});
        }

        if (operacion == "eco") {
            validarParametros(parametros, {"texto"});
            return MensajeRPC::respuestaA(
                solicitud, {{"texto", texto(parametros, "texto")}});
        }

        if (operacion == "conectar") {
            if (parametros.size() > 1 ||
                (!parametros.empty() && parametros.count("puerto") == 0))
                throw ParametrosInvalidos(
                    "conectar acepta solamente el parametro opcional puerto.");

#ifdef _WIN32
            const std::string puerto =
                parametros.empty() ? "COM3" : texto(parametros, "puerto");
#else
            const std::string puerto =
                parametros.empty() ? "/dev/ttyACM0" : texto(parametros, "puerto");
#endif
            if (!robot_.conectar(puerto))
                return MensajeRPC::respuestaA(
                    solicitud,
                    {{"conectado", false},
                     {"mensaje", "No se pudo abrir o configurar el puerto serie: " +
                                     puerto}},
                    "ERROR_CONEXION");
            return MensajeRPC::respuestaA(
                solicitud,
                {{"conectado", true},
                 {"mensaje", "Conexion establecida con el robot."}});
        }

        if (operacion == "desconectar") {
            validarSinParametros(parametros);
            robot_.desconectar();
            return MensajeRPC::respuestaA(
                solicitud,
                {{"conectado", false},
                 {"mensaje", "Robot desconectado correctamente."}});
        }

        if (operacion == "estado") {
            validarSinParametros(parametros);
            if (!robot_.estaConectado()) {
                return MensajeRPC::respuestaA(
                    solicitud,
                    {{"conectado", false},
                     {"posicion", datoNoDisponible(
                          "El robot esta desconectado; no se puede consultar la posicion.")},
                     {"actividad", estadoRobotDesconocido()}});
            }

            try {
                const std::string respuesta = robot_.obtenerEstado();
                const auto posicion = posicionRobot(respuesta);
                const EstructuraRPC actividad{
                    {"motores", datoBooleano(
                         respuesta, "MOTORS ENABLED", "MOTORS DISABLED",
                         "los motores")},
                    {"ventilador", datoBooleano(
                         respuesta, "FAN ENABLED", "FAN DISABLED",
                         "el ventilador")},
                    {"modo_coordenadas", datoModo(respuesta)}
                };
                auto resultado = crearEstado(
                    robot_.estaConectado(), posicion, actividad);
                resultado.emplace("reporte_firmware", respuesta);
                return MensajeRPC::respuestaA(solicitud, resultado);
            } catch (const std::exception& error) {
                auto resultado = crearEstado(
                    robot_.estaConectado(),
                    datoNoDisponible(
                        "No se pudo obtener la posicion: " +
                        std::string(error.what())),
                    estadoRobotDesconocido());
                resultado.emplace(
                    "mensaje", "Fallo la consulta serie; revise la conexion.");
                return MensajeRPC::respuestaA(
                    solicitud, resultado, "ERROR_INTERNO");
            }
        }

        if (operacion == "activar_motores" ||
            operacion == "desactivar_motores") {
            validarSinParametros(parametros);
            const std::string respuesta = robot_.setMotores(
                operacion == "activar_motores");
            return MensajeRPC::respuestaA(
                solicitud, {{"respuesta", respuesta}});
        }

        if (operacion == "ayuda") {
            validarSinParametros(parametros);
            const std::string ayuda =
                "Comandos de control implementados:\n"
                "conectar [puerto:string]: conecta a 115200 baudios, 8N1. "
                "Puerto por defecto: COM3 en Windows o /dev/ttyACM0 en Linux.\n"
                "desconectar: cierra la conexion serie.\n"
                "estado: consulta M114; posicion en mm, ejes X/Y/Z/E. "
                "Posicion y actividad se marcan no disponibles si el firmware "
                "no las informa. No hay limites de coordenadas configurados "
                "por esta interfaz.\n"
                "activar_motores: envia M17.\n"
                "desactivar_motores: envia M18.\n"
                "No se implementan movimientos ni otros comandos G-code.";
            return MensajeRPC::respuestaA(
                solicitud, {{"comandos", ayuda}});
        }

        return respuestaError(
            solicitud, "OPERACION_NO_SOPORTADA",
            "Operacion no reconocida o no soportada.");
    } catch (const ParametrosInvalidos& error) {
        return respuestaError(solicitud, "VALIDACION", error.what());
    } catch (const RobotNotConnectedException& error) {
        return respuestaError(solicitud, "ESTADO_INVALIDO", error.what());
    } catch (const std::exception& error) {
        return respuestaError(
            solicitud, "ERROR_INTERNO",
            std::string("Error al comunicarse con el robot: ") + error.what());
    }
}

} // namespace robotrpc

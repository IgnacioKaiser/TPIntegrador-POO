#include "robotrpc/servicio.hpp"

#include <exception>
#include <cmath>
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

double numero(
    const EstructuraRPC& parametros,
    const std::string& nombre
) {
    const auto& dato = parametros.at(nombre).dato();
    double valor;
    if (const auto* decimal = std::get_if<double>(&dato)) {
        valor = *decimal;
    } else if (const auto* entero = std::get_if<std::int32_t>(&dato)) {
        valor = static_cast<double>(*entero);
    } else {
        throw ParametrosInvalidos(
            "El parametro " + nombre + " debe ser numerico.");
    }
    if (!std::isfinite(valor))
        throw ParametrosInvalidos(
            "El parametro " + nombre + " debe ser un numero finito.");
    return valor;
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

std::string nombreModoTrabajo(ModoTrabajo modo) {
    return modo == ModoTrabajo::MANUAL ? "manual" : "automatico";
}

std::string nombreModoCoordenadas(ModoCoordenadas modo) {
    return modo == ModoCoordenadas::ABSOLUTO ? "absoluto" : "relativo";
}

EstructuraRPC datoModo(ModoCoordenadas modo) {
    return {
        {"disponible", true},
        {"valor", nombreModoCoordenadas(modo)}
    };
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

EstructuraRPC estadoRobotDesconocido(ModoCoordenadas modoCoordenadas) {
    return {
        {"motores", datoNoDisponible(
             "El estado de los motores requiere consultar el firmware.")},
        {"ventilador", datoNoDisponible(
             "El estado del ventilador requiere consultar el firmware.")},
        {"modo_coordenadas", datoModo(modoCoordenadas)}
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
        std::unique_lock<std::mutex> acceso;
        if (ejecutor_ && operacion != "ping" && operacion != "eco" && operacion != "ayuda") {
            auto foto = ejecutor_->fotoSiActiva();
            if (!foto) {
                acceso = ejecutor_->intentarAccesoRobot();
                if (!acceso.owns_lock())
                    return respuestaError(solicitud, "ESTADO_INVALIDO", "El robot esta reservado.");
                foto = ejecutor_->fotoSiActiva(); // Cierra la carrera con un inicio concurrente.
            }
            if (foto) {
                if (operacion != "estado")
                    return respuestaError(solicitud, "ESTADO_INVALIDO", "Hay una tarea en ejecucion; operacion incompatible.");
                validarSinParametros(parametros);
                return MensajeRPC::respuestaA(solicitud,
                    {{"conectado", foto->conectado}, {"modo_trabajo", nombreModoTrabajo(foto->trabajo)},
                     {"modo_coordenadas", nombreModoCoordenadas(foto->coordenadas)},
                     {"posicion", datoNoDisponible("Posicion actual no consultada durante la tarea.")},
                     {"actividad", estadoRobotDesconocido(foto->coordenadas)},
                     {"datos_anteriores", true},
                     {"mensaje", "Datos locales anteriores al inicio; el puerto esta reservado por una tarea."}});
            }
        }
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

        if (operacion == "seleccionar_modo_trabajo") {
            validarParametros(parametros, {"modo"});
            const std::string valor = texto(parametros, "modo");
            if (valor == "manual")
                robot_.seleccionarModoTrabajo(ModoTrabajo::MANUAL);
            else if (valor == "automatico")
                robot_.seleccionarModoTrabajo(ModoTrabajo::AUTOMATICO);
            else
                throw ParametrosInvalidos(
                    "El modo de trabajo debe ser manual o automatico.");
            return MensajeRPC::respuestaA(
                solicitud,
                {{"modo_trabajo", valor},
                 {"mensaje", "Modo de trabajo seleccionado: " + valor + "."}});
        }

        if (operacion == "seleccionar_modo_coordenadas") {
            validarParametros(parametros, {"modo"});
            const std::string valor = texto(parametros, "modo");
            ModoCoordenadas modo;
            if (valor == "absoluto")
                modo = ModoCoordenadas::ABSOLUTO;
            else if (valor == "relativo")
                modo = ModoCoordenadas::RELATIVO;
            else
                throw ParametrosInvalidos(
                    "El modo de coordenadas debe ser absoluto o relativo.");
            const std::string respuesta =
                robot_.seleccionarModoCoordenadas(modo);
            return MensajeRPC::respuestaA(
                solicitud,
                {{"modo_coordenadas", valor},
                 {"respuesta", respuesta},
                 {"mensaje", "Modo de coordenadas seleccionado: " +
                                 valor + "."}});
        }

        if (operacion == "ir_a_origen") {
            validarSinParametros(parametros);
            return MensajeRPC::respuestaA(
                solicitud, {{"respuesta", robot_.irAOrigen()}});
        }

        if (operacion == "mover") {
            for (const auto& parametro : parametros) {
                if (parametro.first != "x" && parametro.first != "y" &&
                    parametro.first != "z" &&
                    parametro.first != "velocidad")
                    throw ParametrosInvalidos(
                        "Parametro no reconocido: " + parametro.first + ".");
            }
            for (const auto* eje : {"x", "y", "z"}) {
                if (parametros.count(eje) == 0)
                    throw ParametrosInvalidos(
                        "Falta el parametro requerido: " +
                        std::string(eje) + ".");
            }
            const double x = numero(parametros, "x");
            const double y = numero(parametros, "y");
            const double z = numero(parametros, "z");
            const double velocidad = parametros.count("velocidad") == 0
                ? Robot::VELOCIDAD_PREDETERMINADA_MM_S
                : numero(parametros, "velocidad");
            const std::string respuesta =
                robot_.mover(x, y, z, velocidad);
            return MensajeRPC::respuestaA(
                solicitud,
                {{"respuesta", respuesta},
                 {"x", x}, {"y", y}, {"z", z},
                 {"velocidad", velocidad},
                 {"unidad_velocidad", "mm/s"}});
        }

        if (operacion == "activar_efector" ||
            operacion == "desactivar_efector") {
            validarSinParametros(parametros);
            const bool activar = operacion == "activar_efector";
            const std::string respuesta = activar
                ? robot_.activarEfector()
                : robot_.desactivarEfector();
            return MensajeRPC::respuestaA(
                solicitud,
                {{"respuesta", respuesta},
                 {"mensaje", activar
                     ? "Efector activado."
                     : "Efector desactivado."}});
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
                auto actividad = estadoRobotDesconocido(
                    robot_.modoCoordenadas());
                return MensajeRPC::respuestaA(
                    solicitud,
                    {{"conectado", false},
                     {"posicion", datoNoDisponible(
                          "El robot esta desconectado; no se puede consultar la posicion.")},
                     {"actividad", actividad},
                     {"modo_trabajo",
                      nombreModoTrabajo(robot_.modoTrabajo())},
                     {"modo_coordenadas",
                      nombreModoCoordenadas(robot_.modoCoordenadas())}});
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
                    {"modo_coordenadas",
                     datoModo(robot_.modoCoordenadas())}
                };
                auto resultado = crearEstado(
                    robot_.estaConectado(), posicion, actividad);
                resultado.emplace("reporte_firmware", respuesta);
                resultado.emplace(
                    "modo_trabajo", nombreModoTrabajo(robot_.modoTrabajo()));
                resultado.emplace(
                    "modo_coordenadas",
                    nombreModoCoordenadas(robot_.modoCoordenadas()));
                return MensajeRPC::respuestaA(solicitud, resultado);
            } catch (const std::exception& error) {
                auto resultado = crearEstado(
                    robot_.estaConectado(),
                    datoNoDisponible(
                        "No se pudo obtener la posicion: " +
                        std::string(error.what())),
                    estadoRobotDesconocido(robot_.modoCoordenadas()));
                resultado.emplace(
                    "mensaje", "Fallo la consulta serie; revise la conexion.");
                resultado.emplace(
                    "modo_trabajo", nombreModoTrabajo(robot_.modoTrabajo()));
                resultado.emplace(
                    "modo_coordenadas",
                    nombreModoCoordenadas(robot_.modoCoordenadas()));
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
                "Informa tambien los modos seleccionados.\n"
                "seleccionar_modo_trabajo(modo): manual o automatico.\n"
                "seleccionar_modo_coordenadas(modo): absoluto o relativo.\n"
                "ir_a_origen: envia G28 en modo manual.\n"
                "mover(x, y, z, velocidad?): envia G1 en modo manual. "
                "Coordenadas en mm; velocidad entre 5 y 10 mm/s "
                "(predeterminada: 5 mm/s).\n"
                "activar_efector: envia M3 (cerrar pinza).\n"
                "desactivar_efector: envia M5 (abrir pinza).\n"
                "activar_motores: envia M17.\n"
                "desactivar_motores: envia M18.\n"
                "El movimiento valida el espacio geometrico configurado "
                "para el firmware.";
            return MensajeRPC::respuestaA(
                solicitud, {{"comandos", ayuda}});
        }

        return respuestaError(
            solicitud, "OPERACION_NO_SOPORTADA",
            "Operacion no reconocida o no soportada.");
    } catch (const ParametrosInvalidos& error) {
        return respuestaError(solicitud, "VALIDACION", error.what());
    } catch (const std::invalid_argument& error) {
        return respuestaError(solicitud, "VALIDACION", error.what());
    } catch (const RobotModeException& error) {
        return respuestaError(solicitud, "ESTADO_INVALIDO", error.what());
    } catch (const RobotNotConnectedException& error) {
        return respuestaError(solicitud, "ESTADO_INVALIDO", error.what());
    } catch (const RobotCommandRejectedException& error) {
        return respuestaError(
            solicitud, "ESTADO_INVALIDO",
            "El controlador rechazo la orden: " + std::string(error.what()));
    } catch (const std::exception& error) {
        return respuestaError(
            solicitud, "ERROR_INTERNO",
            std::string("Error al comunicarse con el robot: ") + error.what());
    }
}

} // namespace robotrpc

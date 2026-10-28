#include "domain/Robot.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <regex>
#include <sstream>
#include <locale>

Robot::Robot()
    : canal_(puerto_), modoTrabajo_(ModoTrabajo::MANUAL),
      modoCoordenadas_(ModoCoordenadas::ABSOLUTO) {}

Robot::Robot(CanalSerie& canal)
    : canal_(canal), modoTrabajo_(ModoTrabajo::MANUAL),
      modoCoordenadas_(ModoCoordenadas::ABSOLUTO) {}

Robot::Robot(CanalSerie* canal)
    : canal_(canal ? *canal : puerto_), modoTrabajo_(ModoTrabajo::MANUAL),
      modoCoordenadas_(ModoCoordenadas::ABSOLUTO) {}

Robot::~Robot() { desconectar(); }
bool Robot::conectar(const std::string& puerto) { return canal_.conectar(puerto); }
void Robot::desconectar() { canal_.desconectar(); }
bool Robot::estaConectado() const { return canal_.estaConectado(); }

// Implementación sugerida por la cátedra para limpiar saltos de línea
std::string Robot::limpiarRespuesta(const std::string& raw) const {
    std::string r = raw;
    r.erase(std::remove(r.begin(), r.end(), '\r'), r.end());
    r.erase(std::remove(r.begin(), r.end(), '\n'), r.end());
    return r;
}

std::string Robot::enviarComandoRaw(const std::string& comando, unsigned int timeoutMs) {
    if (!estaConectado()) throw RobotNotConnectedException();
    try {
        return limpiarRespuesta(canal_.intercambiar(comando, timeoutMs));
    } catch (const ErrorSerie& error) {
        validarRespuestaControlador(error.respuestaParcial());
        throw;
    }
}

std::string Robot::obtenerEstado() {
    return enviarComandoRaw("M114\r");
}

std::string Robot::setMotores(bool activar) {
    std::string comando = activar ? "M17\r" : "M18\r";
    return enviarComandoRaw(comando);
}

ModoTrabajo Robot::modoTrabajo() const {
    return modoTrabajo_;
}

ModoCoordenadas Robot::modoCoordenadas() const {
    return modoCoordenadas_;
}

void Robot::seleccionarModoTrabajo(ModoTrabajo modo) {
    modoTrabajo_ = modo;
}

std::string Robot::seleccionarModoCoordenadas(ModoCoordenadas modo) {
    if (estaConectado()) {
        const std::string comando =
            modo == ModoCoordenadas::ABSOLUTO ? "G90\r" : "G91\r";
        const std::string respuesta = enviarComandoRaw(comando);
        validarRespuestaControlador(respuesta);
        modoCoordenadas_ = modo;
        return respuesta;
    }

    modoCoordenadas_ = modo;
    return "Modo seleccionado; se aplicara al conectar el robot.";
}

void Robot::exigirModoManual() const {
    if (modoTrabajo_ != ModoTrabajo::MANUAL)
        throw RobotModeException(
            "La operacion solo esta disponible en modo manual.");
}

void Robot::validarRespuestaControlador(
    const std::string& respuesta
) const {
    if (respuesta.find("ERROR:") != std::string::npos ||
        respuesta.find("COMMAND NOT RECOGNIZED") != std::string::npos ||
        respuesta.find("POINT IS OUTSIDE OF WORKSPACE") != std::string::npos) {
        throw RobotCommandRejectedException(respuesta);
    }
}

PosicionCartesiana Robot::obtenerPosicionActual() {
    const std::string respuesta = enviarComandoRaw("M114\r");
    validarRespuestaControlador(respuesta);

    return interpretarPosicion(respuesta);
}

PosicionCartesiana Robot::interpretarPosicion(const std::string& respuesta) const {
    static const std::regex formato(
        R"(CURRENT POSITION:\s*\[X:\s*([-+]?(?:[0-9]*\.)?[0-9]+(?:[eE][-+]?[0-9]+)?)\s+Y:\s*([-+]?(?:[0-9]*\.)?[0-9]+(?:[eE][-+]?[0-9]+)?)\s+Z:\s*([-+]?(?:[0-9]*\.)?[0-9]+(?:[eE][-+]?[0-9]+)?)\s+E:\s*([-+]?(?:[0-9]*\.)?[0-9]+(?:[eE][-+]?[0-9]+)?)\s*\])"
    );
    std::smatch coincidencia;
    if (!std::regex_search(respuesta, coincidencia, formato))
        throw std::runtime_error(
            "El controlador no informo una posicion cartesiana valida.");

    PosicionCartesiana posicion{
        std::stod(coincidencia[1].str()),
        std::stod(coincidencia[2].str()),
        std::stod(coincidencia[3].str())
    };
    if (!std::isfinite(posicion.x) || !std::isfinite(posicion.y) ||
        !std::isfinite(posicion.z))
        throw std::runtime_error(
            "El controlador informo una posicion cartesiana invalida.");
    return posicion;
}

bool Robot::posicionPermitida(const PosicionCartesiana& posicion) {
    if (!std::isfinite(posicion.x) || !std::isfinite(posicion.y) ||
        !std::isfinite(posicion.z))
        return false;

    constexpr double longitudBrazoInferior = 120.0;
    constexpr double longitudBrazoSuperior = 120.0;
    constexpr double desplazamientoEfector = 50.0;
    constexpr double cosenoAnguloMinimo = 0.791436948;
    constexpr double cosenoAnguloMaximo = -0.774944489;
    constexpr double alturaMinima = -115.0;
    constexpr double alturaMaxima = 150.0;

    if (posicion.z < alturaMinima || posicion.z > alturaMaxima)
        return false;

    const double radioEfector = std::hypot(posicion.x, posicion.y);
    const double radioBrazo = radioEfector - desplazamientoEfector;
    const double radioCuadrado =
        radioBrazo * radioBrazo + posicion.z * posicion.z;
    const double sumaLongitudesCuadradas =
        longitudBrazoInferior * longitudBrazoInferior +
        longitudBrazoSuperior * longitudBrazoSuperior;
    const double productoLongitudes =
        2.0 * longitudBrazoInferior * longitudBrazoSuperior;
    const double radioMinimoCuadrado =
        sumaLongitudesCuadradas -
        productoLongitudes * cosenoAnguloMinimo;
    const double radioMaximoCuadrado =
        sumaLongitudesCuadradas -
        productoLongitudes * cosenoAnguloMaximo;

    return radioCuadrado >= radioMinimoCuadrado &&
           radioCuadrado <= radioMaximoCuadrado;
}

std::string Robot::irAOrigen() {
    exigirModoManual();
    const std::string respuesta = enviarComandoRaw("G28\r", 10000);
    validarRespuestaControlador(respuesta);
    return respuesta;
}

void Robot::validarEntradaMovimiento(double x, double y, double z, double velocidad) {
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z))
        throw std::invalid_argument(
            "Las coordenadas deben ser valores numericos finitos.");
    if (!std::isfinite(velocidad) ||
        velocidad < VELOCIDAD_PREDETERMINADA_MM_S ||
        velocidad > VELOCIDAD_MAXIMA_MM_S) {
        throw std::invalid_argument(
            "La velocidad debe estar entre 5 y 10 mm/s.");
    }

}

Robot::Movimiento Robot::prepararMovimiento(
    double x, double y, double z, double velocidad, const PosicionCartesiana& actual) const {
    validarEntradaMovimiento(x, y, z, velocidad);
    const PosicionCartesiana destino =
        modoCoordenadas_ == ModoCoordenadas::ABSOLUTO
            ? PosicionCartesiana{x, y, z}
            : PosicionCartesiana{actual.x + x, actual.y + y, actual.z + z};
    if (!posicionPermitida(actual))
        throw std::runtime_error(
            "La posicion actual informada por el controlador esta fuera "
            "del espacio de trabajo.");
    if (!posicionPermitida(destino))
        throw std::invalid_argument(
            "La posicion solicitada esta fuera del espacio de trabajo.");

    const double distancia = std::hypot(
        std::hypot(destino.x - actual.x, destino.y - actual.y),
        destino.z - actual.z);
    const int pasos = std::max(1, static_cast<int>(std::ceil(distancia)));
    for (int paso = 1; paso <= pasos; ++paso) {
        const double proporcion =
            static_cast<double>(paso) / static_cast<double>(pasos);
        const PosicionCartesiana intermedia{
            actual.x + (destino.x - actual.x) * proporcion,
            actual.y + (destino.y - actual.y) * proporcion,
            actual.z + (destino.z - actual.z) * proporcion
        };
        if (!posicionPermitida(intermedia))
            throw std::invalid_argument(
                "La trayectoria atraviesa una zona fuera del espacio "
                "de trabajo.");
    }

    std::ostringstream comando;
    comando.imbue(std::locale::classic());
    comando << std::fixed << std::setprecision(6)
            << "G1 X" << x << " Y" << y << " Z" << z
            << " F" << velocidad << '\r';
    const unsigned int timeoutMs = static_cast<unsigned int>(
        std::ceil(distancia / velocidad * 1000.0) + 10000.0);
    return {destino, comando.str(), timeoutMs};
}

std::string Robot::mover(double x, double y, double z, double velocidad) {
    exigirModoManual();
    validarEntradaMovimiento(x, y, z, velocidad);
    const auto movimiento = prepararMovimiento(x, y, z, velocidad, obtenerPosicionActual());
    const std::string modo = modoCoordenadas_ == ModoCoordenadas::ABSOLUTO ? "G90\r" : "G91\r";
    validarRespuestaControlador(enviarComandoRaw(modo));
    const auto respuesta = enviarComandoRaw(movimiento.comando, movimiento.timeoutMs);
    validarRespuestaControlador(respuesta);
    return respuesta;
}

std::string Robot::activarEfector() {
    exigirModoManual();
    const std::string respuesta = enviarComandoRaw("M3\r", 5000);
    validarRespuestaControlador(respuesta);
    return respuesta;
}

std::string Robot::desactivarEfector() {
    exigirModoManual();
    const std::string respuesta = enviarComandoRaw("M5\r", 5000);
    validarRespuestaControlador(respuesta);
    return respuesta;
}

void Robot::exigirMotoresActivos(const std::string& respuesta) const {
    if (respuesta.find("MOTORS ENABLED") == std::string::npos ||
        respuesta.find("MOTORS DISABLED") != std::string::npos)
        throw std::runtime_error("El controlador no confirma motores activados.");
}

ConfirmacionOrdenAutomatica Robot::ejecutarOrdenAutomatica(const OrdenTarea& orden) {
    orden.validar(); // Tambien protege a quien construya una OrdenTarea sin usar el parser.
    if (modoTrabajo_ != ModoTrabajo::AUTOMATICO)
        throw RobotModeException("La orden automatica requiere modo automatico.");
    if (!estaConectado()) throw RobotNotConnectedException();

    bool envioIntentado = false;
    try {
        std::string comando;
        unsigned int timeoutMs = 1000;
        std::optional<PosicionCartesiana> destino;
        if (orden.tipo == TipoOrdenTarea::MOVIMIENTO || orden.tipo == TipoOrdenTarea::ORIGEN) {
            const auto estado = enviarComandoRaw("M114\r");
            validarRespuestaControlador(estado);
            exigirMotoresActivos(estado);
            if (orden.tipo == TipoOrdenTarea::MOVIMIENTO) {
                const auto actual = interpretarPosicion(estado);
                const bool absoluto = modoCoordenadas_ == ModoCoordenadas::ABSOLUTO;
                const auto movimiento = prepararMovimiento(
                    orden.x.value_or(absoluto ? actual.x : 0.0),
                    orden.y.value_or(absoluto ? actual.y : 0.0),
                    orden.z.value_or(absoluto ? actual.z : 0.0),
                    orden.velocidad.value_or(VELOCIDAD_PREDETERMINADA_MM_S), actual);
                validarRespuestaControlador(enviarComandoRaw(absoluto ? "G90\r" : "G91\r"));
                comando = movimiento.comando;
                timeoutMs = movimiento.timeoutMs;
                destino = movimiento.destino;
            } else {
                comando = "G28\r";
                timeoutMs = 10000;
            }
        } else {
            switch (orden.tipo) {
                case TipoOrdenTarea::ABSOLUTO: comando = "G90\r"; break;
                case TipoOrdenTarea::RELATIVO: comando = "G91\r"; break;
                case TipoOrdenTarea::ACTIVAR_EFECTOR: comando = "M3\r"; timeoutMs = 5000; break;
                case TipoOrdenTarea::DESACTIVAR_EFECTOR: comando = "M5\r"; timeoutMs = 5000; break;
                case TipoOrdenTarea::ACTIVAR_MOTORES: comando = "M17\r"; break;
                case TipoOrdenTarea::DESACTIVAR_MOTORES: comando = "M18\r"; break;
                case TipoOrdenTarea::ESTADO: comando = "M114\r"; break;
                default: throw std::invalid_argument("Tipo de orden invalido.");
            }
        }
        envioIntentado = true; // Una escritura parcial tambien deja resultado desconocido.
        const auto respuesta = enviarComandoRaw(comando, timeoutMs);
        validarRespuestaControlador(respuesta);
        if (orden.tipo == TipoOrdenTarea::ORIGEN && respuesta.find("HOMING COMPLETE") == std::string::npos)
            throw std::runtime_error("El controlador no confirmo el origen.");
        if (orden.tipo == TipoOrdenTarea::ABSOLUTO) modoCoordenadas_ = ModoCoordenadas::ABSOLUTO;
        if (orden.tipo == TipoOrdenTarea::RELATIVO) modoCoordenadas_ = ModoCoordenadas::RELATIVO;
        if (orden.tipo == TipoOrdenTarea::ESTADO) interpretarPosicion(respuesta);
        if (orden.tipo == TipoOrdenTarea::MOVIMIENTO || orden.tipo == TipoOrdenTarea::ORIGEN) {
            // Hipotesis del firmware versionado: M114 se procesa despues de la interpolacion.
            // No certificar finalizacion fisica hasta verificarlo con el Arduino corregido.
            const auto final = enviarComandoRaw("M114\r", timeoutMs);
            validarRespuestaControlador(final);
            const auto posicion = interpretarPosicion(final);
            if (destino && (std::abs(posicion.x - destino->x) > 0.05 ||
                            std::abs(posicion.y - destino->y) > 0.05 ||
                            std::abs(posicion.z - destino->z) > 0.05))
                throw std::runtime_error("La posicion informada no coincide con el destino esperado.");
            return {true};
        }
        return {false};
    } catch (const RobotCommandRejectedException&) {
        throw ErrorOrdenAutomatica(envioIntentado ? ResultadoOrdenAutomatica::ERROR_CONTROLADOR
                                                 : ResultadoOrdenAutomatica::NO_ENVIADA,
                                   "El controlador informo un error; no se reintenta.");
    } catch (const std::exception& e) {
        throw ErrorOrdenAutomatica(envioIntentado ? ResultadoOrdenAutomatica::DESCONOCIDO
                                                 : ResultadoOrdenAutomatica::NO_ENVIADA,
                                   envioIntentado ? "Resultado de la orden desconocido; no se reintenta."
                                                  : std::string("Orden no enviada: ") + e.what());
    }
}

#ifndef ROBOT_H
#define ROBOT_H

#include <string>
#include <stdexcept>
#include <cstdint>
#include "domain/CanalSerie.h"
#include "domain/ProgramaTarea.h"

enum class ModoTrabajo {
    MANUAL,
    AUTOMATICO
};

enum class ModoCoordenadas {
    ABSOLUTO,
    RELATIVO
};

struct PosicionCartesiana {
    double x;
    double y;
    double z;
};

// Excepción personalizada para manejo de errores de conexión
class RobotNotConnectedException : public std::runtime_error {
public:
    RobotNotConnectedException() 
        : std::runtime_error("El robot no se encuentra conectado al servidor.") {}
};

class RobotModeException : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class RobotCommandRejectedException : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

enum class ResultadoOrdenAutomatica { NO_ENVIADA, ERROR_CONTROLADOR, DESCONOCIDO };

class ErrorOrdenAutomatica : public std::runtime_error {
public:
    ErrorOrdenAutomatica(ResultadoOrdenAutomatica resultado, const std::string& mensaje)
        : std::runtime_error(mensaje), resultado_(resultado) {}
    ResultadoOrdenAutomatica resultado() const noexcept { return resultado_; }
private:
    ResultadoOrdenAutomatica resultado_;
};

struct ConfirmacionOrdenAutomatica {
    // Solo confirmacion del protocolo. M114 como barrera aun requiere prueba con Arduino.
    bool finalizacionConsultada;
};

class Robot {
private:
    PuertoSerie puerto_;
    CanalSerie& canal_;
    ModoTrabajo modoTrabajo_;
    ModoCoordenadas modoCoordenadas_;

    // Métodos internos de utilidad
    std::string limpiarRespuesta(const std::string& raw) const;
    std::string enviarComandoRaw(
        const std::string& comando,
        unsigned int timeoutMs = 1000);
    PosicionCartesiana obtenerPosicionActual();
    PosicionCartesiana interpretarPosicion(const std::string& respuesta) const;
    void exigirMotoresActivos(const std::string& respuesta) const;
    struct Movimiento {
        PosicionCartesiana destino;
        std::string comando;
        unsigned int timeoutMs;
    };
    Movimiento prepararMovimiento(double x, double y, double z, double velocidad,
                                 const PosicionCartesiana& actual) const;
    static void validarEntradaMovimiento(double x, double y, double z, double velocidad);
    void exigirModoManual() const;
    void validarRespuestaControlador(const std::string& respuesta) const;

public:
    static constexpr double VELOCIDAD_PREDETERMINADA_MM_S = OrdenTarea::VELOCIDAD_PREDETERMINADA;
    static constexpr double VELOCIDAD_MAXIMA_MM_S = OrdenTarea::VELOCIDAD_MAXIMA;

    Robot();
    explicit Robot(CanalSerie& canal);
    ~Robot();
    Robot(const Robot&) = delete;
    Robot& operator=(const Robot&) = delete;

    // Control de conexión
    bool conectar(const std::string& puerto);
    void desconectar();
    bool estaConectado() const;

    // Comandos de dominio (Interfaz pública)
    std::string obtenerEstado();          // M114
    std::string setMotores(bool activar); // M17 / M18
    ModoTrabajo modoTrabajo() const;
    ModoCoordenadas modoCoordenadas() const;
    void seleccionarModoTrabajo(ModoTrabajo modo);
    std::string seleccionarModoCoordenadas(ModoCoordenadas modo);
    std::string irAOrigen();
    std::string mover(double x, double y, double z, double velocidad);
    std::string activarEfector();
    std::string desactivarEfector();
    ConfirmacionOrdenAutomatica ejecutarOrdenAutomatica(const OrdenTarea& orden);

    static bool posicionPermitida(const PosicionCartesiana& posicion);
};

#endif // ROBOT_H

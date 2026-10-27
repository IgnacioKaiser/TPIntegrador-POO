#ifndef ROBOT_H
#define ROBOT_H

#include <string>
#include <stdexcept>
#include <cstdint>

// Excepción personalizada para manejo de errores de conexión
class RobotNotConnectedException : public std::runtime_error {
public:
    RobotNotConnectedException() 
        : std::runtime_error("El robot no se encuentra conectado al servidor.") {}
};

class Robot {
private:
    std::intptr_t fd_serie;
    bool conectado;

    // Métodos internos de utilidad
    std::string limpiarRespuesta(const std::string& raw) const;
    std::string enviarComandoRaw(const std::string& comando);
    std::string leerRespuesta();
    void cerrarPuerto();

public:
    Robot();
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
};

#endif // ROBOT_H

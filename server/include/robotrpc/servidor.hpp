#pragma once
#include "robotrpc/servicio_protegido.hpp"
#include "domain/Robot.h"
#include <stdexcept>
#include <string>

namespace robotrpc {

class ErrorServidor : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class ServidorXmlRpc {
public:
    explicit ServidorXmlRpc(unsigned short puerto,
                            const std::string& ruta = "data/db/usuarios.sqlite",
                            const std::string& directorioTareas = "data/trayectorias");
    void ejecutar();
private:
    unsigned short puerto_;
    RepositorioUsuarios usuarios_;
    Autenticador autenticador_;
    Robot robot_;
    ServicioRobot servicio_;
    RepositorioTareas tareas_;
    RegistroActividadCsv registro_;
    ServicioProtegido protegido_;
};

} // namespace robotrpc

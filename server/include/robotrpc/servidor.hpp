#pragma once
#include "robotrpc/servicio.hpp"
#include "domain/Robot.h"
#include <stdexcept>

namespace robotrpc {

class ErrorServidor : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class ServidorXmlRpc {
public:
    explicit ServidorXmlRpc(unsigned short puerto);
    void ejecutar();

private:
    unsigned short puerto_;
    Robot robot_;
    ServicioRobot servicio_;
};

} // namespace robotrpc

#pragma once
#include "robotrpc/mensaje.hpp"

namespace robotrpc {

// Caso de uso sin consola, red, estado compartido ni acceso al robot.
class ServicioPrueba {
public:
    virtual ~ServicioPrueba() = default;
    virtual MensajeRPC atender(const MensajeRPC& solicitud) const;
};

} // namespace robotrpc

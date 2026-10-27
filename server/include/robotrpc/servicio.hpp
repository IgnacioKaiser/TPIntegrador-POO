#pragma once
#include "robotrpc/mensaje.hpp"
#include "domain/Robot.h"

namespace robotrpc {

class ServicioRobot {
private:
    Robot& robot_;

public:
    explicit ServicioRobot(Robot& robot) : robot_(robot) {}

    virtual ~ServicioRobot() = default;
    virtual MensajeRPC atender(const MensajeRPC& solicitud) const;
};

} // namespace robotrpc
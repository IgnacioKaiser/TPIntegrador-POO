#pragma once
#include "robotrpc/mensaje.hpp"
#include "domain/Robot.h"
#include "robotrpc/ejecutor_tareas.hpp"

namespace robotrpc {

class ServicioRobot {
private:
    Robot& robot_;
    EjecutorTareas* ejecutor_;

public:
    explicit ServicioRobot(Robot& robot, EjecutorTareas* ejecutor = nullptr)
        : robot_(robot), ejecutor_(ejecutor) {}

    virtual ~ServicioRobot() = default;
    virtual MensajeRPC atender(const MensajeRPC& solicitud) const;
};

} // namespace robotrpc

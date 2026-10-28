#pragma once
#include "robotrpc/autenticador.hpp"
#include "robotrpc/servicio.hpp"
#include "robotrpc/repositorio_tareas.hpp"

namespace robotrpc {
// Unico acceso del servidor RPC a los servicios de aplicacion.
class ServicioProtegido {
public:
    ServicioProtegido(const Autenticador& autenticador,
                      const ServicioRobot& servicio,
                      RepositorioUsuarios& usuarios, RepositorioTareas& tareas)
        : autenticador_(autenticador), servicio_(servicio), usuarios_(usuarios), tareas_(tareas) {}

    MensajeRPC atender(const MensajeRPC& solicitud) const;

private:
    const Autenticador& autenticador_;
    const ServicioRobot& servicio_;
    RepositorioUsuarios& usuarios_;
    RepositorioTareas& tareas_;
};
}

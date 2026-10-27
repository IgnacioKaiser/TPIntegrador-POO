#pragma once
#include "robotrpc/autenticador.hpp"
#include "robotrpc/servicio.hpp"

namespace robotrpc {
// Unico acceso del servidor RPC a los servicios de aplicacion.
class ServicioProtegido {
public:
    ServicioProtegido(const Autenticador& autenticador,
                      const ServicioPrueba& servicio,
                      RepositorioUsuarios& usuarios)
        : autenticador_(autenticador), servicio_(servicio), usuarios_(usuarios) {}

    MensajeRPC atender(const MensajeRPC& solicitud) const;

private:
    const Autenticador& autenticador_;
    const ServicioPrueba& servicio_;
    RepositorioUsuarios& usuarios_;
};
}

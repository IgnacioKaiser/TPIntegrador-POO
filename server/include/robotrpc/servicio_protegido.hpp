#pragma once
#include "robotrpc/autenticador.hpp"
#include "robotrpc/servicio.hpp"
#include "robotrpc/repositorio_tareas.hpp"
#include "robotrpc/registro_actividad.hpp"

namespace robotrpc {
// Unico acceso del servidor RPC a los servicios de aplicacion.
class ServicioProtegido {
public:
    ServicioProtegido(const Autenticador& autenticador,
                      const ServicioRobot& servicio,
                      RepositorioUsuarios& usuarios, RepositorioTareas& tareas,
                      RegistroActividadCsv* registro = nullptr, EjecutorTareas* ejecutor = nullptr)
        : autenticador_(autenticador), servicio_(servicio), usuarios_(usuarios),
          tareas_(tareas), registro_(registro), ejecutor_(ejecutor) {}

    MensajeRPC atender(const MensajeRPC& solicitud) const;
    MensajeRPC atenderInterno(const MensajeRPC& solicitud,
                             std::string& usuarioVerificado) const;

private:
    const Autenticador& autenticador_;
    const ServicioRobot& servicio_;
    RepositorioUsuarios& usuarios_;
    RepositorioTareas& tareas_;
    RegistroActividadCsv* registro_;
    EjecutorTareas* ejecutor_;
};
}

#pragma once
#include "robotrpc/servicio_protegido.hpp"
#include <stdexcept>

namespace robotrpc {

class ErrorServidor : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class ServidorXmlRpc {
public:
    explicit ServidorXmlRpc(unsigned short puerto,
                            const std::string& ruta = "data/db/usuarios.sqlite");
    void ejecutar();

private:
    unsigned short puerto_;
    RepositorioUsuarios usuarios_;
    Autenticador autenticador_;
    ServicioPrueba servicio_;
    ServicioProtegido protegido_;
};

} // namespace robotrpc

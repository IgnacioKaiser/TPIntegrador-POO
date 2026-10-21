#pragma once

#include <map>
#include <stdexcept>
#include <string>

namespace robotrpc {

class MensajeInvalido : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// Representación neutral: el dominio no depende de ninguna biblioteca XML-RPC.
using CamposMensaje = std::map<std::string, std::string>;

class MensajeRPC {
public:
    static MensajeRPC solicitud(std::string id, std::string operacion,
                                std::string contenido = "");
    static MensajeRPC respuestaA(const MensajeRPC& solicitud,
                                 std::string contenido, std::string codigo = "OK");
    static MensajeRPC deserializar(const CamposMensaje& campos);
    CamposMensaje serializar() const;

    const std::string& id() const noexcept { return id_; }
    const std::string& tipo() const noexcept { return tipo_; }
    const std::string& operacion() const noexcept { return operacion_; }
    const std::string& contenido() const noexcept { return contenido_; }
    const std::string& codigo() const noexcept { return codigo_; }

private:
    MensajeRPC(std::string id, std::string tipo, std::string operacion,
               std::string contenido, std::string codigo);
    std::string id_, tipo_, operacion_, contenido_, codigo_;
};

} // namespace robotrpc

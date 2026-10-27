#pragma once
#include <utility>
#include "robotrpc/repositorio_usuarios.hpp"
#include "robotrpc/mensaje.hpp"

namespace robotrpc {
class ErrorAutenticacion : public std::runtime_error {
public:
    ErrorAutenticacion(std::string codigo, const std::string& mensaje)
        : std::runtime_error(mensaje), codigo_(std::move(codigo)) {}
    const std::string& codigo() const { return codigo_; }
private:
    std::string codigo_;
};

class Autenticador {
public:
    explicit Autenticador(const RepositorioUsuarios& usuarios) : usuarios_(usuarios) {}
    Usuario validar(const MensajeRPC& solicitud) const;
private:
    const RepositorioUsuarios& usuarios_;
};
}

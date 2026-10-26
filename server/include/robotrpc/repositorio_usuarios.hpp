#pragma once
#include "robotrpc/usuario.hpp"
#include <optional>
#include <stdexcept>
#include <vector>

struct sqlite3;

namespace robotrpc {
class ErrorUsuarios : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class RepositorioUsuarios {
public:
    explicit RepositorioUsuarios(const std::string& ruta);
    ~RepositorioUsuarios();
    RepositorioUsuarios(const RepositorioUsuarios&) = delete;
    RepositorioUsuarios& operator=(const RepositorioUsuarios&) = delete;

    // Solo funciona si la tabla esta vacia. Nunca restablece una clave.
    void inicializarAdministrador(const std::string& nombre, const std::string& clave);
    void agregarOperador(const std::string& nombre, const std::string& clave);
    std::optional<Usuario> buscar(const std::string& nombre) const;
    std::vector<Usuario> listar() const;
    bool verificarClave(const std::string& nombre, const std::string& clave) const;

private:
    sqlite3* db_ = nullptr;
    void ejecutar(const char* sql) const;
    void insertar(const std::string& nombre, const std::string& clave, Rol rol);
};
}

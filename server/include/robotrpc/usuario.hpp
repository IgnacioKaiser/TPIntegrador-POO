#pragma once
#include <string>

namespace robotrpc {

enum class Rol {
    ADMINISTRADOR, OPERADOR
};

// Vista del usuario: nunca contiene la clave ni su hash.
struct Usuario {
    int id;
    std::string nombre;
    Rol rol;
    bool habilitado;
};
}

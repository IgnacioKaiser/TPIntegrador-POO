#pragma once
#include <string>

namespace robotrpc {

// Datos para identificar y listar una tarea sin cargar su contenido en memoria.
struct Tarea {
    std::string id;
    int propietario_id;
    std::string nombre;
};

struct ContenidoTarea {
    Tarea tarea;
    std::string contenido;
};

} // namespace robotrpc

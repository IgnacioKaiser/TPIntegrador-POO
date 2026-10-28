#pragma once
#include "robotrpc/tarea.hpp"
#include "robotrpc/usuario.hpp"
#include <cstddef>
#include <filesystem>
#include <stdexcept>
#include <vector>

namespace robotrpc {

enum class TipoErrorTarea { VALIDACION, DUPLICADA, ACCESO_DENEGADO, ALMACENAMIENTO, NO_ENCONTRADA };

class ErrorTarea : public std::runtime_error {
public:
    ErrorTarea(TipoErrorTarea tipo, const std::string& mensaje)
        : std::runtime_error(mensaje), tipo_(tipo) {}
    TipoErrorTarea tipo() const { return tipo_; }
private:
    TipoErrorTarea tipo_;
};

class RepositorioTareas {
public:
    static constexpr std::size_t MAX_CONTENIDO = 1024 * 1024;

    explicit RepositorioTareas(const std::filesystem::path& directorio);

    // Un solo escritor con llamadas secuenciales. No recibe rutas del cliente.
    // Recibir el Usuario de la capa autenticada, nunca un propietario del cliente.
    Tarea guardar(const Usuario& autenticado, const std::string& nombre,
                  const std::string& contenido);
    std::vector<Tarea> listar(const Usuario& autenticado) const;
    ContenidoTarea leer(const Usuario& autenticado, const std::string& id) const;

private:
    std::filesystem::path raiz_;
    void verificarRaiz() const;
    std::filesystem::path carpetaPropietario(int id) const;
};

} // namespace robotrpc

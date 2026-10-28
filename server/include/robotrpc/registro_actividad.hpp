#pragma once

#include <cstddef>
#include <mutex>
#include <string>
#include <vector>

namespace robotrpc {

struct EntradaActividad {
    std::string fecha_hora, tipo, id_solicitud, usuario, nodo_cliente;
    std::string modulo, operacion, codigo, detalle;
};

struct FiltroActividad {
    std::string usuario, desde, hasta, tipo, codigo;
    std::size_t offset = 0;
    std::size_t limite = 50;
};

struct PaginaActividad {
    std::size_t total = 0;
    std::vector<EntradaActividad> registros;
};

struct ResumenUsuario {
    std::string usuario;
    std::size_t peticiones = 0, exitosas = 0, rechazadas = 0;
};

struct ActividadPropia {
    std::string desde, referencia, inicio_actividad;
    PaginaActividad pagina;
    std::size_t errores = 0;
};

// Modulo independiente de XML-RPC, SQLite y Robot.
class RegistroActividadCsv {
public:
    explicit RegistroActividadCsv(std::string rutaCsv);

    void registrarPeticion(const std::string& id, const std::string& usuarioVerificado,
                          const std::string& nodo, const std::string& operacion,
                          const std::string& codigo);
    void registrarEvento(const std::string& modulo, const std::string& operacion,
                         const std::string& codigo, const std::string& detalle);
    void registrarEjecucion(const std::string& id, const std::string& usuarioVerificado,
                           const std::string& operacion, const std::string& codigo,
                           const std::string& detalle);

    PaginaActividad consultar(const FiltroActividad& filtro) const;
    std::vector<ResumenUsuario> informeUsuarios(const FiltroActividad& filtro) const;
    ActividadPropia actividadPropia(const std::string& usuario,
                                   std::size_t offset = 0, std::size_t limite = 50) const;

    const std::string& ruta() const noexcept { return ruta_; }
private:
    void anexar(EntradaActividad fila);
    static void validarFiltro(const FiltroActividad& filtro);
    std::string ruta_;
    mutable std::mutex mutex_;
};

} // namespace robotrpc

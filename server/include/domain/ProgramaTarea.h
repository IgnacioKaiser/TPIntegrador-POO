#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

enum class TipoOrdenTarea {
    MOVIMIENTO, ORIGEN, ABSOLUTO, RELATIVO,
    ACTIVAR_EFECTOR, DESACTIVAR_EFECTOR, ACTIVAR_MOTORES, DESACTIVAR_MOTORES, ESTADO
};

// Una orden ya interpretada, no una cadena que pueda enviarse sin revisar.
struct OrdenTarea {
    static constexpr double VELOCIDAD_PREDETERMINADA = 5.0;
    static constexpr double VELOCIDAD_MAXIMA = 10.0;
    TipoOrdenTarea tipo;
    std::size_t linea = 0;
    std::optional<double> x, y, z, velocidad;
    void validar() const;
};

class ProgramaTarea {
public:
    explicit ProgramaTarea(const std::string& contenido);
    const std::vector<OrdenTarea>& ordenes() const noexcept { return ordenes_; }
private:
    std::vector<OrdenTarea> ordenes_;
};

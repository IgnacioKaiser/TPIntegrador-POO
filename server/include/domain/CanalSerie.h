#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>

class ErrorSerie : public std::runtime_error {
public:
    ErrorSerie(const std::string& mensaje, std::string parcial = {})
        : std::runtime_error(mensaje), parcial_(std::move(parcial)) {}
    const std::string& respuestaParcial() const noexcept { return parcial_; }
private:
    std::string parcial_;
};

// El sustituto de pruebas implementa solo este intercambio, sin abrir un puerto.
class CanalSerie {
public:
    virtual ~CanalSerie() = default;
    virtual bool conectar(const std::string& puerto) = 0;
    virtual void desconectar() noexcept = 0;
    virtual bool estaConectado() const noexcept = 0;
    virtual std::string intercambiar(const std::string& comando, unsigned int timeoutMs) = 0;
};

// Posee y libera el recurso del sistema operativo. No interpreta G-Code.
class PuertoSerie final : public CanalSerie {
public:
    ~PuertoSerie() override { desconectar(); }
    PuertoSerie() = default;
    PuertoSerie(const PuertoSerie&) = delete;
    PuertoSerie& operator=(const PuertoSerie&) = delete;
    bool conectar(const std::string& puerto) override;
    void desconectar() noexcept override;
    bool estaConectado() const noexcept override { return descriptor_ >= 0; }
    std::string intercambiar(const std::string& comando, unsigned int timeoutMs) override;
private:
    std::intptr_t descriptor_ = -1;
    std::string pendientes_;
    std::string leerRespuesta(unsigned int timeoutMs);
};

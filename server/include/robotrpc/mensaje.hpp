#pragma once

#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace robotrpc {

class MensajeInvalido : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// Representa un valor del contrato sin depender de XmlRpcValue.
// Un valor puede ser texto, entero de 32 bits, double, booleano,
// arreglo o estructura con campos identificados por nombre.
class ValorRPC {
public:
    using Arreglo = std::vector<ValorRPC>;
    using Estructura = std::map<std::string, ValorRPC>;
    using Dato = std::variant<std::string, std::int32_t, double, bool,
                              Arreglo, Estructura>;

    // Por defecto, representa una estructura vacia.
    ValorRPC() : dato_(Estructura{}) {}
    ValorRPC(const char* texto) : dato_(std::string(texto)) {}
    ValorRPC(std::string texto) : dato_(std::move(texto)) {}
    ValorRPC(std::int32_t numero) : dato_(numero) {}
    ValorRPC(double numero) : dato_(numero) {}
    ValorRPC(bool valor) : dato_(valor) {}
    ValorRPC(Arreglo arreglo) : dato_(std::move(arreglo)) {}
    ValorRPC(Estructura estructura) : dato_(std::move(estructura)) {}

    const Dato& dato() const noexcept { return dato_; }

    // Ejemplo: valor.obtener<double>() o valor.obtener<std::string>().
    template <typename T>
    const T& obtener() const { return std::get<T>(dato_); }

private:
    Dato dato_;
};

using EstructuraRPC = ValorRPC::Estructura;
using CamposMensaje = EstructuraRPC;

class MensajeRPC {
public:
    static MensajeRPC solicitud(std::string id, std::string operacion,
                                EstructuraRPC parametros = {});

    static MensajeRPC respuestaA(const MensajeRPC& solicitud,
                                 EstructuraRPC resultado = {},
                                 std::string codigo = "OK");

    static MensajeRPC deserializar(const CamposMensaje& campos);
    CamposMensaje serializar() const;

    const std::string& id() const noexcept { return id_; }
    const std::string& tipo() const noexcept { return tipo_; }
    const std::string& operacion() const noexcept { return operacion_; }
    const EstructuraRPC& parametros() const noexcept { return parametros_; }
    const EstructuraRPC& resultado() const noexcept { return resultado_; }
    const std::string& codigo() const noexcept { return codigo_; }

private:
    MensajeRPC(std::string id, std::string tipo, std::string operacion,
               EstructuraRPC parametros, EstructuraRPC resultado,
               std::string codigo);

    std::string id_;
    std::string tipo_;
    std::string operacion_;
    EstructuraRPC parametros_;
    EstructuraRPC resultado_;
    std::string codigo_;
};

} // namespace robotrpc
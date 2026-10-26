#include "robotrpc/mensaje.hpp"

#include <cmath>
#include <set>
#include <utility>

namespace robotrpc {
namespace {

bool letra(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

bool digito(char c) {
    return c >= '0' && c <= '9';
}

bool idValido(const std::string& texto) {
    if (texto.empty() || texto.size() > 64) return false;
    for (char c : texto)
        if (!letra(c) && !digito(c) && c != '_' && c != '-') return false;
    return true;
}

bool operacionValida(const std::string& texto) {
    if (texto.empty() || texto.size() > 64 || !letra(texto.front()))
        return false;
    for (char c : texto)
        if (!letra(c) && !digito(c) && c != '_' && c != '-' && c != '.')
            return false;
    return true;
}

bool nombreValido(const std::string& texto) {
    if (texto.empty() || texto.size() > 64 || !letra(texto.front()))
        return false;
    for (char c : texto)
        if (!letra(c) && !digito(c) && c != '_') return false;
    return true;
}

// Valida UTF-8 y los caracteres permitidos por XML 1.0.
bool textoValido(const std::string& texto) {
    for (std::size_t i = 0; i < texto.size();) {
        const auto inicial = static_cast<unsigned char>(texto[i++]);
        unsigned int punto = 0;
        unsigned int restantes = 0;
        unsigned int minimo = 0;

        if (inicial < 0x80) {
            punto = inicial;
        } else if (inicial >= 0xC2 && inicial <= 0xDF) {
            punto = inicial & 0x1F; restantes = 1; minimo = 0x80;
        } else if (inicial >= 0xE0 && inicial <= 0xEF) {
            punto = inicial & 0x0F; restantes = 2; minimo = 0x800;
        } else if (inicial >= 0xF0 && inicial <= 0xF4) {
            punto = inicial & 0x07; restantes = 3; minimo = 0x10000;
        } else {
            return false;
        }

        for (unsigned int j = 0; j < restantes; ++j) {
            if (i >= texto.size()) return false;
            const auto siguiente = static_cast<unsigned char>(texto[i++]);
            if ((siguiente & 0xC0) != 0x80) return false;
            punto = (punto << 6) | (siguiente & 0x3F);
        }

        if (punto < minimo || punto > 0x10FFFF ||
            !((punto >= 0x20 && punto <= 0xD7FF) ||
              (punto >= 0xE000 && punto <= 0xFFFD) ||
              (punto >= 0x10000 && punto <= 0x10FFFF) ||
              punto == 9 || punto == 10 || punto == 13))
            return false;
    }
    return true;
}

void validarEstructura(const EstructuraRPC& estructura);

void validarValor(const ValorRPC& valor) {
    const auto& dato = valor.dato();

    if (const auto* texto = std::get_if<std::string>(&dato)) {
        if (!textoValido(*texto))
            throw MensajeInvalido("Texto invalido para XML 1.0 / UTF-8.");
    } else if (const auto* numero = std::get_if<double>(&dato)) {
        if (!std::isfinite(*numero))
            throw MensajeInvalido("Un double debe ser finito.");
    } else if (const auto* arreglo = std::get_if<ValorRPC::Arreglo>(&dato)) {
        for (const auto& elemento : *arreglo) validarValor(elemento);
    } else if (const auto* estructura = std::get_if<EstructuraRPC>(&dato)) {
        validarEstructura(*estructura);
    }
    // int32_t y bool no necesitan validaciones adicionales.
}

void validarEstructura(const EstructuraRPC& estructura) {
    for (const auto& campo : estructura) {
        if (!nombreValido(campo.first))
            throw MensajeInvalido("Nombre de parametro o resultado invalido: " + campo.first);
        validarValor(campo.second);
    }
}

std::string textoRequerido(const CamposMensaje& campos, const char* nombre) {
    const auto encontrado = campos.find(nombre);
    if (encontrado == campos.end())
        throw MensajeInvalido(std::string("Falta el campo: ") + nombre);

    const auto* texto = std::get_if<std::string>(&encontrado->second.dato());
    if (!texto)
        throw MensajeInvalido(std::string(nombre) + " debe ser string.");
    return *texto;
}

EstructuraRPC estructuraRequerida(const CamposMensaje& campos, const char* nombre) {
    const auto encontrado = campos.find(nombre);
    if (encontrado == campos.end())
        throw MensajeInvalido(std::string("Falta el campo: ") + nombre);

    const auto* estructura =
        std::get_if<EstructuraRPC>(&encontrado->second.dato());
    if (!estructura)
        throw MensajeInvalido(std::string(nombre) + " debe ser una estructura.");
    return *estructura;
}

} // namespace

MensajeRPC::MensajeRPC(std::string id, std::string tipo, std::string operacion,
                       EstructuraRPC parametros, EstructuraRPC resultado,
                       std::string codigo)
    : id_(std::move(id)), tipo_(std::move(tipo)),
      operacion_(std::move(operacion)), parametros_(std::move(parametros)),
      resultado_(std::move(resultado)), codigo_(std::move(codigo)) {

    if (!idValido(id_))
        throw MensajeInvalido("id invalido (1 a 64 caracteres permitidos).");
    if (!operacionValida(operacion_))
        throw MensajeInvalido("operacion invalida (1 a 64 caracteres permitidos).");
    if (tipo_ != "solicitud" && tipo_ != "respuesta")
        throw MensajeInvalido("tipo debe ser solicitud o respuesta.");

    validarEstructura(parametros_);
    validarEstructura(resultado_);

    if (tipo_ == "solicitud") {
        if (!codigo_.empty() || !resultado_.empty())
            throw MensajeInvalido(
                "Solicitud: codigo vacio y resultado vacio obligatorios.");
    } else {
        if (!idValido(codigo_) || !parametros_.empty())
            throw MensajeInvalido(
                "Respuesta: codigo valido y parametros vacios obligatorios.");
    }
}

MensajeRPC MensajeRPC::solicitud(std::string id, std::string operacion,
                                EstructuraRPC parametros) {
    return MensajeRPC(std::move(id), "solicitud", std::move(operacion),
                      std::move(parametros), {}, "");
}

MensajeRPC MensajeRPC::respuestaA(const MensajeRPC& solicitud,
                                 EstructuraRPC resultado, std::string codigo) {
    if (solicitud.tipo() != "solicitud")
        throw MensajeInvalido("Se esperaba una solicitud para responder.");

    return MensajeRPC(solicitud.id(), "respuesta", solicitud.operacion(),
                      {}, std::move(resultado), std::move(codigo));
}

CamposMensaje MensajeRPC::serializar() const {
    return {
        {"id", ValorRPC(id_)},
        {"tipo", ValorRPC(tipo_)},
        {"operacion", ValorRPC(operacion_)},
        {"parametros", ValorRPC(parametros_)},
        {"resultado", ValorRPC(resultado_)},
        {"codigo", ValorRPC(codigo_)}
    };
}

MensajeRPC MensajeRPC::deserializar(const CamposMensaje& campos) {
    const std::set<std::string> esperados = {
        "id", "tipo", "operacion", "parametros", "resultado", "codigo"
    };

    if (campos.size() != esperados.size())
        throw MensajeInvalido("El mensaje debe tener exactamente seis campos.");

    for (const auto& campo : campos)
        if (esperados.count(campo.first) == 0)
            throw MensajeInvalido("Campo de mensaje desconocido: " + campo.first);

    return MensajeRPC(
        textoRequerido(campos, "id"),
        textoRequerido(campos, "tipo"),
        textoRequerido(campos, "operacion"),
        estructuraRequerida(campos, "parametros"),
        estructuraRequerida(campos, "resultado"),
        textoRequerido(campos, "codigo")
    );
}

} // namespace robotrpc

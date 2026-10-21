#include "robotrpc/mensaje.hpp"
#include <algorithm>
#include <set>
#include <utility>

namespace robotrpc {
namespace {
bool identificadorValido(const std::string& valor, std::size_t maximo) {
    return !valor.empty() && valor.size() <= maximo &&
        std::all_of(valor.begin(), valor.end(), [](unsigned char c) {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                   (c >= '0' && c <= '9') || c == '_' || c == '-';
        });
}

// El contrato version 1 conserva texto UTF-8 del plano basico (BMP).
// Validamos también UTF-8 y los caracteres permitidos por XML 1.0.
bool textoValido(const std::string& texto) {
    for (std::size_t i = 0; i < texto.size();) {
        const auto inicial = static_cast<unsigned char>(texto[i++]);
        unsigned int punto = inicial;
        unsigned int restantes = 0;
        unsigned int minimo = 0;
        if (inicial < 0x80) { /* ASCII */ }
        else if (inicial >= 0xC2 && inicial <= 0xDF) {
            punto = inicial & 0x1F; restantes = 1; minimo = 0x80;
        } else if (inicial >= 0xE0 && inicial <= 0xEF) {
            punto = inicial & 0x0F; restantes = 2; minimo = 0x800;
        } else return false;
        while (restantes--) {
            if (i == texto.size()) return false;
            const auto siguiente = static_cast<unsigned char>(texto[i++]);
            if ((siguiente & 0xC0) != 0x80) return false;
            punto = (punto << 6) | (siguiente & 0x3F);
        }
        if (punto < minimo || !((punto >= 0x20 && punto <= 0xD7FF) ||
             (punto >= 0xE000 && punto <= 0xFFFD) || punto == 9 || punto == 10 || punto == 13))
            return false;
    }
    return true;
}
}

MensajeRPC::MensajeRPC(std::string id, std::string tipo, std::string operacion,
                       std::string contenido, std::string codigo)
    : id_(std::move(id)), tipo_(std::move(tipo)), operacion_(std::move(operacion)),
      contenido_(std::move(contenido)), codigo_(std::move(codigo)) {
    if (!identificadorValido(id_, 64))
        throw MensajeInvalido("id: usar entre 1 y 64 letras ASCII, numeros, guiones o guiones bajos.");
    if (!identificadorValido(operacion_, 32))
        throw MensajeInvalido("operacion: identificador invalido (maximo 32 caracteres).");
    if (tipo_ != "solicitud" && tipo_ != "respuesta")
        throw MensajeInvalido("tipo debe ser solicitud o respuesta.");
    if (contenido_.size() > 8192)
        throw MensajeInvalido("contenido supera 8192 bytes UTF-8.");
    if (!textoValido(contenido_))
        throw MensajeInvalido("contenido debe ser texto XML 1.0 UTF-8 del plano basico, sin emojis.");
    if ((tipo_ == "solicitud" && !codigo_.empty()) ||
        (tipo_ == "respuesta" && !identificadorValido(codigo_, 64)))
        throw MensajeInvalido("codigo debe estar vacio en solicitudes y ser un identificador en respuestas.");
}

MensajeRPC MensajeRPC::solicitud(std::string id, std::string operacion,
                                 std::string contenido) {
    return MensajeRPC(std::move(id), "solicitud", std::move(operacion),
                      std::move(contenido), "");
}

MensajeRPC MensajeRPC::respuestaA(const MensajeRPC& solicitud,
                                  std::string contenido, std::string codigo) {
    if (solicitud.tipo() != "solicitud")
        throw MensajeInvalido("Se esperaba una solicitud.");
    return MensajeRPC(solicitud.id(), "respuesta", solicitud.operacion(),
                      std::move(contenido), std::move(codigo));
}

CamposMensaje MensajeRPC::serializar() const {
    return {{"version", "1"}, {"id", id_}, {"tipo", tipo_},
            {"operacion", operacion_}, {"contenido", contenido_}, {"codigo", codigo_}};
}

MensajeRPC MensajeRPC::deserializar(const CamposMensaje& campos) {
    const std::set<std::string> esperados =
        {"version", "id", "tipo", "operacion", "contenido", "codigo"};
    std::set<std::string> recibidos;
    for (const auto& campo : campos) recibidos.insert(campo.first);
    if (recibidos != esperados)
        throw MensajeInvalido("El mensaje debe contener exactamente los seis campos del contrato.");
    if (campos.at("version") != "1")
        throw MensajeInvalido("Version de contrato no soportada; se requiere 1.");
    return MensajeRPC(campos.at("id"), campos.at("tipo"), campos.at("operacion"),
                      campos.at("contenido"), campos.at("codigo"));
}

} // namespace robotrpc

#include "domain/ProgramaTarea.h"

#include <cmath>
#include <locale>
#include <sstream>
#include <stdexcept>

namespace {
bool digito(char c) { return c >= '0' && c <= '9'; }
char mayuscula(char c) { return c >= 'a' && c <= 'z' ? c - 'a' + 'A' : c; }
bool espacio(char c) { return c == ' ' || c == '\t'; }
void saltarEspacios(const std::string& s, std::size_t& p) {
    while (p < s.size() && espacio(s[p])) ++p;
}
TipoOrdenTarea tipoOrden(char grupo, const std::string& numero) {
    if (grupo == 'G') {
        if (numero == "0" || numero == "1") return TipoOrdenTarea::MOVIMIENTO;
        if (numero == "28") return TipoOrdenTarea::ORIGEN;
        if (numero == "90") return TipoOrdenTarea::ABSOLUTO;
        if (numero == "91") return TipoOrdenTarea::RELATIVO;
    } else if (grupo == 'M') {
        if (numero == "3") return TipoOrdenTarea::ACTIVAR_EFECTOR;
        if (numero == "5") return TipoOrdenTarea::DESACTIVAR_EFECTOR;
        if (numero == "17") return TipoOrdenTarea::ACTIVAR_MOTORES;
        if (numero == "18") return TipoOrdenTarea::DESACTIVAR_MOTORES;
        if (numero == "114") return TipoOrdenTarea::ESTADO;
    }
    throw std::invalid_argument("Codigo G/M no admitido para ejecutar.");
}
OrdenTarea interpretar(const std::string& s, std::size_t linea) {
    std::size_t p = 0;
    saltarEspacios(s, p);
    if (p < s.size() && mayuscula(s[p]) == 'N') {
        ++p;
        saltarEspacios(s, p);
        const auto inicio = p;
        while (p < s.size() && digito(s[p])) ++p;
        if (p == inicio) throw std::invalid_argument("Numeracion N sin digitos.");
        saltarEspacios(s, p);
    }
    if (p == s.size()) throw std::invalid_argument("Falta una orden G/M.");
    const char grupo = mayuscula(s[p++]);
    saltarEspacios(s, p);
    const auto inicio = p;
    while (p < s.size() && digito(s[p])) ++p;
    if (p == inicio) throw std::invalid_argument("Falta el numero G/M.");
    std::string numero = s.substr(inicio, p - inicio);
    const auto primero = numero.find_first_not_of('0');
    numero = primero == std::string::npos ? "0" : numero.substr(primero);
    OrdenTarea orden{tipoOrden(grupo, numero), linea, {}, {}, {}, {}};
    while (true) {
        saltarEspacios(s, p);
        if (p == s.size()) break;
        const char eje = mayuscula(s[p++]);
        std::optional<double>* destino = nullptr;
        switch (eje) {
            case 'X': destino = &orden.x; break;
            case 'Y': destino = &orden.y; break;
            case 'Z': destino = &orden.z; break;
            case 'F': destino = &orden.velocidad; break;
            default: throw std::invalid_argument("Argumento no admitido; usar X/Y/Z/F.");
        }
        if (destino->has_value()) throw std::invalid_argument("Argumento repetido.");
        saltarEspacios(s, p);
        const auto comienzo = p;
        if (p < s.size() && (s[p] == '+' || s[p] == '-')) ++p;
        bool hayDigito = false;
        while (p < s.size() && digito(s[p])) { hayDigito = true; ++p; }
        if (p < s.size() && s[p] == '.') {
            ++p;
            while (p < s.size() && digito(s[p])) { hayDigito = true; ++p; }
        }
        if (!hayDigito) throw std::invalid_argument("Se requiere un decimal con punto.");
        std::istringstream entrada(s.substr(comienzo, p - comienzo));
        entrada.imbue(std::locale::classic());
        double valor;
        if (!(entrada >> valor) || !std::isfinite(valor))
            throw std::invalid_argument("Numero fuera de rango.");
        *destino = valor;
    }
    orden.validar();
    return orden;
}
} // namespace

void OrdenTarea::validar() const {
    switch (tipo) {
        case TipoOrdenTarea::MOVIMIENTO:
            if (!x && !y && !z) throw std::invalid_argument("El movimiento requiere algun eje X/Y/Z.");
            break;
        case TipoOrdenTarea::ORIGEN: case TipoOrdenTarea::ABSOLUTO: case TipoOrdenTarea::RELATIVO:
        case TipoOrdenTarea::ACTIVAR_EFECTOR: case TipoOrdenTarea::DESACTIVAR_EFECTOR:
        case TipoOrdenTarea::ACTIVAR_MOTORES: case TipoOrdenTarea::DESACTIVAR_MOTORES:
        case TipoOrdenTarea::ESTADO:
            if (x || y || z || velocidad) throw std::invalid_argument("Esta orden no acepta argumentos.");
            break;
        default: throw std::invalid_argument("Tipo de orden invalido.");
    }
    for (const auto& valor : {x, y, z, velocidad})
        if (valor && !std::isfinite(*valor)) throw std::invalid_argument("Los valores deben ser finitos.");
    if (velocidad && (*velocidad < VELOCIDAD_PREDETERMINADA || *velocidad > VELOCIDAD_MAXIMA))
        throw std::invalid_argument("F debe estar entre 5 y 10 mm/s.");
}

ProgramaTarea::ProgramaTarea(const std::string& contenido) {
    if (contenido.empty() || contenido.size() > 1024 * 1024)
        throw std::invalid_argument("El programa debe tener entre 1 byte y 1 MiB.");
    std::size_t inicio = contenido.compare(0, 3, "\xef\xbb\xbf") == 0 ? 3 : 0;
    std::string visible;
    bool comentario = false, finLinea = false;
    std::size_t linea = 1;
    auto procesarLinea = [&] {
        const auto primero = visible.find_first_not_of(" \t");
        if (primero != std::string::npos &&
            visible.substr(primero, visible.find_last_not_of(" \t") - primero + 1) != "%") {
            try { ordenes_.push_back(interpretar(visible, linea)); }
            catch (const std::invalid_argument& e) {
                throw std::invalid_argument("Linea " + std::to_string(linea) + ": " + e.what());
            }
        }
        visible.clear(); finLinea = false;
    };
    for (std::size_t i = inicio; i < contenido.size(); ++i) {
        char c = contenido[i];
        if (c == '\r' || c == '\n') {
            procesarLinea(); ++linea;
            if (c == '\r' && i + 1 < contenido.size() && contenido[i + 1] == '\n') ++i;
        } else if (finLinea) {
            continue;
        } else if (comentario) {
            if (c == '(') throw std::invalid_argument("No se admiten comentarios anidados.");
            if (c == ')') comentario = false;
        } else if (c == ';') finLinea = true;
        else if (c == '(') { comentario = true; visible += ' '; }
        else if (c == ')') throw std::invalid_argument("Cierre de comentario sin apertura.");
        else visible += c;
    }
    procesarLinea();
    if (comentario) throw std::invalid_argument("Comentario sin cerrar.");
    if (ordenes_.empty()) throw std::invalid_argument("El programa no contiene ordenes.");
}

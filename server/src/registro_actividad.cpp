#include "robotrpc/registro_actividad.hpp"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace robotrpc {
namespace {
namespace fs = std::filesystem;

const char* ENCABEZADO =
    "fecha_hora,tipo,id_solicitud,usuario,nodo_cliente,modulo,operacion,codigo,detalle";

std::string ahoraUtc() {
    const auto t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm utc{};
#ifdef _WIN32
    if (gmtime_s(&utc, &t) != 0) throw std::runtime_error("Error al obtener hora UTC");
#else
    if (gmtime_r(&t, &utc) == nullptr) throw std::runtime_error("Error al obtener hora UTC");
#endif
    std::ostringstream salida;
    salida << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
    return salida.str();
}

// No registrar saltos de linea ni caracteres de control en datos generados.
std::string textoSeguro(std::string texto) {
    for (char& c : texto) {
        const unsigned char u = static_cast<unsigned char>(c);
        if (u < 0x20 || u == 0x7f) c = ' ';
    }
    if (texto.size() > 180) texto.resize(180);
    return texto;
}

std::string escaparCsv(const std::string& texto) {
    if (texto.find_first_of(",\"\r\n") == std::string::npos) return texto;
    std::string salida = "\"";
    for (char c : texto) {
        if (c == '"') salida += "\"\"";
        else salida += c;
    }
    return salida + '"';
}

std::string serializar(const EntradaActividad& r) {
    const std::string campos[] = {r.fecha_hora, r.tipo, r.id_solicitud,
        r.usuario, r.nodo_cliente, r.modulo, r.operacion, r.codigo, r.detalle};
    std::ostringstream salida;
    for (std::size_t i = 0; i < 9; ++i) {
        if (i) salida << ',';
        salida << escaparCsv(campos[i]);
    }
    return salida.str();
}

std::vector<std::string> dividirCsv(const std::string& linea) {
    std::vector<std::string> campos;
    std::string campo;
    bool entreComillas = false;
    for (std::size_t i = 0; i < linea.size(); ++i) {
        const char c = linea[i];
        if (entreComillas) {
            if (c == '"' && i + 1 < linea.size() && linea[i + 1] == '"') {
                campo += '"'; ++i;
            } else if (c == '"') {
                entreComillas = false;
            } else campo += c;
        } else if (c == '"') {
            if (!campo.empty()) throw std::runtime_error("CSV incorrecto: comillas");
            entreComillas = true;
        } else if (c == ',') {
            campos.push_back(std::move(campo)); campo.clear();
        } else campo += c;
    }
    if (entreComillas) throw std::runtime_error("CSV incorrecto: comillas sin cerrar");
    campos.push_back(std::move(campo));
    if (campos.size() != 9) throw std::runtime_error("CSV incorrecto: numero de columnas");
    return campos;
}

EntradaActividad leerEntrada(const std::string& linea) {
    const auto c = dividirCsv(linea);
    return {c[0],c[1],c[2],c[3],c[4],c[5],c[6],c[7],c[8]};
}

bool fechaValida(const std::string& fecha) {
    if (fecha.empty()) return true;
    if (fecha.size() != 10 || fecha[4] != '-' || fecha[7] != '-') return false;
    for (std::size_t i = 0; i < 10; ++i)
        if (i != 4 && i != 7 && (fecha[i] < '0' || fecha[i] > '9')) return false;
    const int anio = std::stoi(fecha.substr(0,4));
    const int mes = std::stoi(fecha.substr(5,2));
    const int dia = std::stoi(fecha.substr(8,2));
    if (anio < 1 || mes < 1 || mes > 12) return false;
    const int dias[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    const bool bisiesto = (anio % 4 == 0 && (anio % 100 != 0 || anio % 400 == 0));
    const int maxDia = dias[mes - 1] + (mes == 2 && bisiesto ? 1 : 0);
    return dia >= 1 && dia <= maxDia;
}

bool coincide(const EntradaActividad& r, const FiltroActividad& f) {
    if (!f.usuario.empty() && r.usuario != f.usuario) return false;
    if (!f.tipo.empty() && r.tipo != f.tipo) return false;
    if (!f.codigo.empty() && r.codigo != f.codigo) return false;
    const std::string dia = r.fecha_hora.substr(0,10);
    if (!f.desde.empty() && dia < f.desde) return false;
    if (!f.hasta.empty() && dia > f.hasta) return false;
    return true;
}

// Lee un CSV de este modulo. Se protege contra un archivo truncado o corrupto.
template <typename Funcion>
void recorrer(const std::string& ruta, Funcion consumir) {
    std::ifstream archivo(ruta, std::ios::binary);
    if (!archivo) throw std::runtime_error("No se pudo abrir el CSV de actividad");
    std::string linea;
    if (!std::getline(archivo, linea)) throw std::runtime_error("CSV de actividad sin encabezado");
    if (!linea.empty() && linea.back() == '\r') linea.pop_back();
    if (linea != ENCABEZADO) throw std::runtime_error("Encabezado del CSV de actividad inesperado");
    while (std::getline(archivo, linea)) {
        if (!linea.empty() && linea.back() == '\r') linea.pop_back();
        if (linea.empty()) continue;
        consumir(leerEntrada(linea));
    }
    if (archivo.bad()) throw std::runtime_error("Error de lectura del CSV de actividad");
}

} // namespace

RegistroActividadCsv::RegistroActividadCsv(std::string rutaCsv)
    : ruta_(std::move(rutaCsv)) {
    if (ruta_.empty()) throw std::invalid_argument("Ruta del log vacia");
    const fs::path ruta(ruta_);
    if (!ruta.parent_path().empty()) fs::create_directories(ruta.parent_path());
    const bool nuevo = !fs::exists(ruta) || fs::file_size(ruta) == 0;
    if (nuevo) {
        std::ofstream archivo(ruta_, std::ios::out | std::ios::app | std::ios::binary);
        if (!archivo) throw std::runtime_error("No se pudo crear el CSV de actividad");
        archivo << ENCABEZADO << '\n';
        archivo.flush();
        if (!archivo) throw std::runtime_error("No se pudo escribir encabezado CSV");
    } else {
        // Detectar un formato incompatible antes de agregar registros.
        std::ifstream archivo(ruta_, std::ios::binary);
        std::string encabezado;
        if (!std::getline(archivo, encabezado))
            throw std::runtime_error("No se pudo leer el encabezado del CSV");
        if (!encabezado.empty() && encabezado.back() == '\r') encabezado.pop_back();
        if (encabezado != ENCABEZADO)
            throw std::runtime_error("El archivo de log existente usa otro formato; no se sobrescribe");
    }
}

void RegistroActividadCsv::anexar(EntradaActividad fila) {
    std::lock_guard<std::mutex> lock(mutex_);
    fila.fecha_hora = ahoraUtc();
    fila.detalle = textoSeguro(std::move(fila.detalle));
    std::ofstream archivo(ruta_, std::ios::binary | std::ios::app);
    if (!archivo) throw std::runtime_error("No se pudo abrir el log para escritura");
    archivo << serializar(fila) << '\n';
    archivo.flush();
    if (!archivo) throw std::runtime_error("No se pudo escribir en el log CSV");
}

void RegistroActividadCsv::registrarPeticion(
    const std::string& id, const std::string& usuarioVerificado,
    const std::string& nodo, const std::string& operacion,
    const std::string& codigo) {
    anexar({"", "PETICION", id, usuarioVerificado, nodo, "ServidorRPC",
            operacion, codigo, "Solicitud procesada; parametros omitidos por seguridad"});
}

void RegistroActividadCsv::registrarEvento(
    const std::string& modulo, const std::string& operacion,
    const std::string& codigo, const std::string& detalle) {
    anexar({"", "EVENTO", "", "", "", modulo, operacion, codigo, detalle});
}

void RegistroActividadCsv::validarFiltro(const FiltroActividad& f) {
    if (!fechaValida(f.desde) || !fechaValida(f.hasta))
        throw std::invalid_argument("Fechas invalidas. Usar AAAA-MM-DD (UTC).");
    if (!f.desde.empty() && !f.hasta.empty() && f.desde > f.hasta)
        throw std::invalid_argument("La fecha desde debe ser anterior o igual a hasta.");
    if (!f.tipo.empty() && f.tipo != "PETICION" && f.tipo != "EVENTO")
        throw std::invalid_argument("tipo debe ser PETICION o EVENTO.");
    if (f.limite < 1 || f.limite > 100)
        throw std::invalid_argument("limite debe estar entre 1 y 100.");
    if (f.offset > 100000000) throw std::invalid_argument("offset demasiado grande.");
    if (f.usuario.size() > 64 || f.codigo.size() > 64)
        throw std::invalid_argument("Filtro demasiado largo.");
}

PaginaActividad RegistroActividadCsv::consultar(const FiltroActividad& f) const {
    validarFiltro(f);
    std::lock_guard<std::mutex> lock(mutex_);
    PaginaActividad pagina;
    recorrer(ruta_, [&](const EntradaActividad& r) {
        if (!coincide(r, f)) return;
        const auto posicion = pagina.total++;
        if (posicion >= f.offset && pagina.registros.size() < f.limite)
            pagina.registros.push_back(r);
    });
    return pagina;
}

std::vector<ResumenUsuario> RegistroActividadCsv::informeUsuarios(const FiltroActividad& filtro) const {
    FiltroActividad f = filtro;
    f.tipo = "PETICION";
    validarFiltro(f);
    std::lock_guard<std::mutex> lock(mutex_);
    std::map<std::string, ResumenUsuario> porUsuario;
    recorrer(ruta_, [&](const EntradaActividad& r) {
        if (r.usuario.empty() || !coincide(r, f)) return;
        auto& resumen = porUsuario[r.usuario];
        resumen.usuario = r.usuario;
        ++resumen.peticiones;
        if (r.codigo == "OK") ++resumen.exitosas;
        else ++resumen.rechazadas;
    });
    std::vector<ResumenUsuario> salida;
    for (auto& par : porUsuario) salida.push_back(std::move(par.second));
    return salida;
}

} // namespace robotrpc

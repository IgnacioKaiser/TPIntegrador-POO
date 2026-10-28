#include "robotrpc/repositorio_tareas.hpp"
#include <algorithm>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <tuple>

namespace robotrpc {
namespace fs = std::filesystem;
namespace {

bool digito(char c) { return c >= '0' && c <= '9'; }
char minuscula(char c) { return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c; }

void validarUsuario(const Usuario& usuario) {
    if (usuario.id <= 0 || !usuario.habilitado ||
        (usuario.rol != Rol::ADMINISTRADOR && usuario.rol != Rol::OPERADOR))
        throw ErrorTarea(TipoErrorTarea::ACCESO_DENEGADO, "Se requiere un usuario habilitado y valido.");
}

std::string normalizarNombre(std::string nombre) {
    if (nombre.empty() || nombre.size() > 64)
        throw ErrorTarea(TipoErrorTarea::VALIDACION, "El nombre debe tener entre 1 y 64 caracteres.");
    std::transform(nombre.begin(), nombre.end(), nombre.begin(), minuscula);
    const auto punto = nombre.rfind('.');
    if (punto == std::string::npos || punto == 0 ||
        (nombre.substr(punto) != ".gcode" && nombre.substr(punto) != ".nc"))
        throw ErrorTarea(TipoErrorTarea::VALIDACION, "Se requiere un nombre con extension .gcode o .nc.");
    const auto base = nombre.substr(0, punto);
    for (char c : base)
        if (!((c >= 'a' && c <= 'z') || digito(c) || c == '_' || c == '-'))
            throw ErrorTarea(TipoErrorTarea::VALIDACION,
                             "El nombre solo admite letras ASCII, numeros, guion y guion bajo; no rutas.");
    // Windows reserva estos nombres aun cuando tengan extension.
    if (base == "con" || base == "prn" || base == "aux" || base == "nul" ||
        (base.size() == 4 && (base.substr(0, 3) == "com" || base.substr(0, 3) == "lpt") &&
         base[3] >= '1' && base[3] <= '9'))
        throw ErrorTarea(TipoErrorTarea::VALIDACION, "El nombre esta reservado en Windows.");
    return nombre;
}

void validarTexto(const std::string& texto) {
    if (texto.empty() || texto.size() > RepositorioTareas::MAX_CONTENIDO)
        throw ErrorTarea(TipoErrorTarea::VALIDACION, "El contenido debe tener entre 1 byte y 1 MiB.");
    // Decodificar solo para comprobar UTF-8 y caracteres admitidos por XML 1.0.
    for (std::size_t i = 0; i < texto.size();) {
        const auto primero = static_cast<unsigned char>(texto[i++]);
        std::uint32_t valor = primero, minimo = 0;
        unsigned int restantes = 0;
        if (primero >= 0xc2 && primero <= 0xdf) { valor &= 0x1f; restantes = 1; minimo = 0x80; }
        else if (primero >= 0xe0 && primero <= 0xef) { valor &= 0x0f; restantes = 2; minimo = 0x800; }
        else if (primero >= 0xf0 && primero <= 0xf4) { valor &= 0x07; restantes = 3; minimo = 0x10000; }
        else if (primero >= 0x80)
            throw ErrorTarea(TipoErrorTarea::VALIDACION, "El contenido no es texto UTF-8 valido.");
        for (unsigned int n = 0; n < restantes; ++n) {
            if (i == texto.size() || (static_cast<unsigned char>(texto[i]) & 0xc0) != 0x80)
                throw ErrorTarea(TipoErrorTarea::VALIDACION, "El contenido no es texto UTF-8 valido.");
            valor = (valor << 6) | (static_cast<unsigned char>(texto[i++]) & 0x3f);
        }
        if (valor < minimo || !(valor == 9 || valor == 10 || valor == 13 ||
            (valor >= 0x20 && valor <= 0xd7ff) || (valor >= 0xe000 && valor <= 0xfffd) ||
            (valor >= 0x10000 && valor <= 0x10ffff)))
            throw ErrorTarea(TipoErrorTarea::VALIDACION, "El contenido tiene caracteres no admitidos en UTF-8/XML.");
    }
}

std::string normalizarContenido(const std::string& original) {
    validarTexto(original);
    std::string contenido;
    // Aceptar BOM de UTF-8 y finales de linea de Windows, Linux y CR aislado.
    std::size_t inicio = original.compare(0, 3, "\xef\xbb\xbf") == 0 ? 3 : 0;
    for (std::size_t i = inicio; i < original.size(); ++i) {
        if (original[i] == '\r') {
            contenido += '\n';
            if (i + 1 < original.size() && original[i + 1] == '\n') ++i;
        } else contenido += original[i];
    }

    bool hayOrden = false, comentario = false;
    std::istringstream entrada(contenido);
    std::string linea;
    while (std::getline(entrada, linea)) {
        std::string visible;
        for (char c : linea) {
            if (comentario) { if (c == ')') comentario = false; }
            else if (c == ';') break;
            else if (c == '(') comentario = true;
            else visible += c;
        }
        const auto inicioLinea = visible.find_first_not_of(" \t");
        if (inicioLinea == std::string::npos) continue;
        const auto finLinea = visible.find_last_not_of(" \t");
        if (visible.substr(inicioLinea, finLinea - inicioLinea + 1) == "%") continue;
        std::size_t p = inicioLinea;
        // Se reconoce solo el prefijo, no se interpretan parametros ni movimientos.
        if (minuscula(visible[p]) == 'n') {
            ++p;
            while (p < visible.size() && (visible[p] == ' ' || visible[p] == '\t')) ++p;
            const auto inicioNumero = p;
            while (p < visible.size() && digito(visible[p])) ++p;
            if (p == inicioNumero)
                throw ErrorTarea(TipoErrorTarea::VALIDACION, "Numero de linea sin digitos.");
        }
        while (p < visible.size() && (visible[p] == ' ' || visible[p] == '\t')) ++p;
        if (p == visible.size() || (minuscula(visible[p]) != 'g' && minuscula(visible[p]) != 'm'))
            throw ErrorTarea(TipoErrorTarea::VALIDACION, "Cada linea de orden debe comenzar con G o M y un numero.");
        ++p;
        while (p < visible.size() && (visible[p] == ' ' || visible[p] == '\t')) ++p;
        if (p == visible.size() || !digito(visible[p]))
            throw ErrorTarea(TipoErrorTarea::VALIDACION, "Cada linea de orden debe comenzar con G o M y un numero.");
        hayOrden = true;
    }
    if (comentario || !hayOrden)
        throw ErrorTarea(TipoErrorTarea::VALIDACION, "Se requiere al menos una orden y comentarios entre parentesis cerrados.");
    return contenido;
}

Tarea crearTarea(int propietario, const std::string& nombre) {
    return {std::to_string(propietario) + ":" + nombre, propietario, nombre};
}

int leerId(const std::string& texto) {
    try {
        int id = std::stoi(texto);
        return id > 0 && std::to_string(id) == texto ? id : 0;
    } catch (const std::invalid_argument&) { return 0; }
      catch (const std::out_of_range&) { return 0; }
}

} // namespace

RepositorioTareas::RepositorioTareas(const fs::path& directorio) {
    if (directorio.empty())
        throw ErrorTarea(TipoErrorTarea::VALIDACION, "Indique el directorio de tareas.");
    try {
        fs::create_directories(directorio);
        // La raiz es configuracion del servidor, nunca una ruta de una solicitud.
        raiz_ = fs::canonical(directorio);
    } catch (const fs::filesystem_error&) {
        throw ErrorTarea(TipoErrorTarea::ALMACENAMIENTO, "No se pudo preparar el directorio de tareas.");
    }
}

void RepositorioTareas::verificarRaiz() const {
    if (fs::is_symlink(raiz_) || !fs::is_directory(raiz_))
        throw ErrorTarea(TipoErrorTarea::ALMACENAMIENTO, "El directorio de tareas no esta disponible.");
}

fs::path RepositorioTareas::carpetaPropietario(int id) const {
    const auto carpeta = raiz_ / std::to_string(id);
    const auto estado = fs::symlink_status(carpeta);
    if (fs::is_symlink(estado) || (fs::exists(estado) && !fs::is_directory(estado)))
        throw ErrorTarea(TipoErrorTarea::ALMACENAMIENTO, "La carpeta del propietario no es un directorio permitido.");
    return carpeta;
}

Tarea RepositorioTareas::guardar(const Usuario& autenticado, const std::string& nombre,
                                const std::string& contenido) {
    validarUsuario(autenticado);
    const auto normalizado = normalizarNombre(nombre);
    const auto texto = normalizarContenido(contenido);
    fs::path temporal;
    bool creado = false;
    auto limpiarTemporal = [&] {
        if (creado) {
            std::error_code ignorado;
            fs::remove(temporal, ignorado); // Solo el temporal de esta solicitud.
        }
    };
    try {
        verificarRaiz();
        const auto carpeta = carpetaPropietario(autenticado.id);
        fs::create_directories(carpeta);
        const auto destino = carpeta / normalizado;
        const auto estado = fs::symlink_status(destino);
        if (fs::is_symlink(estado))
            throw ErrorTarea(TipoErrorTarea::ALMACENAMIENTO, "No se admiten enlaces en las tareas.");
        if (fs::exists(estado))
            throw ErrorTarea(TipoErrorTarea::DUPLICADA, "Ya existe una tarea de este propietario con ese nombre.");
        temporal = carpeta / ("." + normalizado + ".tmp");
        const auto estadoTemporal = fs::symlink_status(temporal);
        if (fs::exists(estadoTemporal) && !fs::is_regular_file(estadoTemporal))
            throw ErrorTarea(TipoErrorTarea::ALMACENAMIENTO, "El archivo temporal no es un archivo regular permitido.");
        std::ofstream archivo(temporal, std::ios::binary | std::ios::trunc);
        if (!archivo)
            throw ErrorTarea(TipoErrorTarea::ALMACENAMIENTO, "No se pudo abrir el archivo temporal.");
        creado = true;
        archivo.write(texto.data(), static_cast<std::streamsize>(texto.size()));
        archivo.close();
        if (!archivo)
            throw ErrorTarea(TipoErrorTarea::ALMACENAMIENTO, "No se pudo completar la escritura de la tarea.");
        // Un solo escritor secuencial por directorio. No se publica contenido parcial.
        fs::rename(temporal, destino);
        creado = false;
        return crearTarea(autenticado.id, normalizado);
    } catch (const fs::filesystem_error&) {
        limpiarTemporal();
        throw ErrorTarea(TipoErrorTarea::ALMACENAMIENTO, "No se pudo almacenar la tarea.");
    } catch (...) {
        limpiarTemporal();
        throw;
    }
}

std::vector<Tarea> RepositorioTareas::listar(const Usuario& autenticado) const {
    validarUsuario(autenticado);
    std::vector<Tarea> tareas;
    try {
        verificarRaiz();
        auto listarCarpeta = [&](int id) {
            const auto carpeta = carpetaPropietario(id);
            if (!fs::exists(carpeta)) return;
            for (const auto& entrada : fs::directory_iterator(carpeta)) {
                if (entrada.is_symlink() || !entrada.is_regular_file()) continue;
                const auto nombre = entrada.path().filename().string();
                try {
                    if (normalizarNombre(nombre) == nombre) tareas.push_back(crearTarea(id, nombre));
                } catch (const ErrorTarea& error) {
                    if (error.tipo() != TipoErrorTarea::VALIDACION) throw;
                    // Ignorar temporales y archivos ajenos al formato administrado.
                }
            }
        };
        if (autenticado.rol == Rol::OPERADOR) listarCarpeta(autenticado.id);
        else {
            for (const auto& entrada : fs::directory_iterator(raiz_)) {
                if (entrada.is_symlink() || !entrada.is_directory()) continue;
                const int id = leerId(entrada.path().filename().string());
                if (id > 0) listarCarpeta(id);
            }
        }
    } catch (const fs::filesystem_error&) {
        throw ErrorTarea(TipoErrorTarea::ALMACENAMIENTO, "No se pudo listar las tareas.");
    }
    std::sort(tareas.begin(), tareas.end(), [](const Tarea& a, const Tarea& b) {
        return std::tie(a.propietario_id, a.nombre) < std::tie(b.propietario_id, b.nombre);
    });
    return tareas;
}

} // namespace robotrpc

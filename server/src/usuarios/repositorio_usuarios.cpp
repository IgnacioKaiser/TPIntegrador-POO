#include "robotrpc/repositorio_usuarios.hpp"
#include <sqlite3.h>
#include <sodium.h>
#include <filesystem>

namespace robotrpc {
namespace {
// Cada sentencia libera el recurso que SQLite entrega, aun si hay excepciones.
class Sentencia {
public:
    Sentencia(sqlite3* db, const char* sql) {
        if (sqlite3_prepare_v2(db, sql, -1, &stmt_, nullptr) != SQLITE_OK)
            throw ErrorUsuarios("No se pudo preparar la consulta de usuarios.");
    }
    ~Sentencia() { sqlite3_finalize(stmt_); }
    Sentencia(const Sentencia&) = delete;
    Sentencia& operator=(const Sentencia&) = delete;
    sqlite3_stmt* get() const { return stmt_; }
    void texto(int posicion, const std::string& valor) {
        if (sqlite3_bind_text(stmt_, posicion, valor.c_str(),
                              static_cast<int>(valor.size()), SQLITE_TRANSIENT) != SQLITE_OK)
            throw ErrorUsuarios("No se pudo asignar el parametro de la consulta.");
    }
    void entero(int posicion, int valor) {
        if (sqlite3_bind_int(stmt_, posicion, valor) != SQLITE_OK)
            throw ErrorUsuarios("No se pudo asignar el parametro de la consulta.");
    }
    int paso() {
        const int resultado = sqlite3_step(stmt_);
        if (resultado == SQLITE_ROW || resultado == SQLITE_DONE) return resultado;
        if (resultado == SQLITE_CONSTRAINT)
            throw ErrorUsuarios("Usuario duplicado o restriccion de usuarios incumplida.");
        throw ErrorUsuarios("No se pudo consultar o guardar en la base de usuarios.");
    }
private:
    sqlite3_stmt* stmt_ = nullptr;
};

std::string texto(sqlite3_stmt* stmt, int columna) {
    return reinterpret_cast<const char*>(sqlite3_column_text(stmt, columna));
}
Usuario leerUsuario(sqlite3_stmt* stmt) {
    return {sqlite3_column_int(stmt, 0), texto(stmt, 1),
            texto(stmt, 2) == "ADMINISTRADOR" ? Rol::ADMINISTRADOR : Rol::OPERADOR,
            sqlite3_column_int(stmt, 3) != 0};
}
void validarNombre(const std::string& nombre) {
    if (nombre.empty() || nombre.size() > 64)
        throw ErrorUsuarios("El usuario debe tener entre 1 y 64 caracteres.");
    for (char c : nombre)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-'))
            throw ErrorUsuarios("El usuario solo admite letras ASCII, numeros, _ y -.");
}
} // namespace

RepositorioUsuarios::RepositorioUsuarios(const std::string& ruta) {
    if (ruta.empty()) throw ErrorUsuarios("La ruta de la base no puede estar vacia.");
    if (sodium_init() < 0) throw ErrorUsuarios("No se pudo inicializar libsodium.");
    const auto carpeta = std::filesystem::path(ruta).parent_path();
    if (!carpeta.empty()) std::filesystem::create_directories(carpeta);
    if (sqlite3_open(ruta.c_str(), &db_) != SQLITE_OK) {
        sqlite3_close(db_);
        db_ = nullptr;
        throw ErrorUsuarios("No se pudo abrir la base de usuarios.");
    }
    try {
        if (sqlite3_busy_timeout(db_, 3000) != SQLITE_OK)
            throw ErrorUsuarios("No se pudo configurar la base de usuarios.");
        ejecutar("CREATE TABLE IF NOT EXISTS usuarios ("
                 "id INTEGER PRIMARY KEY, nombre TEXT NOT NULL UNIQUE, "
                 "clave_hash TEXT NOT NULL, "
                 "rol TEXT NOT NULL CHECK(rol IN ('ADMINISTRADOR','OPERADOR')), "
                 "habilitado INTEGER NOT NULL DEFAULT 1 CHECK(habilitado IN (0,1)), "
                 "CHECK(rol <> 'ADMINISTRADOR' OR habilitado = 1));");
        ejecutar("CREATE UNIQUE INDEX IF NOT EXISTS un_administrador "
                 "ON usuarios(rol) WHERE rol = 'ADMINISTRADOR';");
    } catch (...) {
        sqlite3_close(db_);
        db_ = nullptr;
        throw;
    }
}
RepositorioUsuarios::~RepositorioUsuarios() { sqlite3_close(db_); }
void RepositorioUsuarios::ejecutar(const char* sql) const {
    if (sqlite3_exec(db_, sql, nullptr, nullptr, nullptr) != SQLITE_OK)
        throw ErrorUsuarios("No se pudo ejecutar la operacion de base de datos.");
}
void RepositorioUsuarios::insertar(const std::string& nombre,
                                  const std::string& clave, Rol rol) {
    validarNombre(nombre);
    if (clave.size() < 8 || clave.size() > 256)
        throw ErrorUsuarios("La clave debe tener entre 8 y 256 bytes.");
    char hash[crypto_pwhash_STRBYTES];
    if (crypto_pwhash_str(hash, clave.data(), clave.size(),
                          crypto_pwhash_OPSLIMIT_INTERACTIVE,
                          crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0)
        throw ErrorUsuarios("No se pudo generar la credencial.");
    Sentencia consulta(db_, "INSERT INTO usuarios(nombre,clave_hash,rol) VALUES(?,?,?);");
    consulta.texto(1, nombre);
    consulta.texto(2, hash);
    consulta.texto(3, rol == Rol::ADMINISTRADOR ? "ADMINISTRADOR" : "OPERADOR");
    consulta.paso();
}
void RepositorioUsuarios::inicializarAdministrador(const std::string& nombre,
                                                   const std::string& clave) {
    ejecutar("BEGIN IMMEDIATE;");
    try {
        {
            Sentencia consulta(db_, "SELECT COUNT(*) FROM usuarios;");
            consulta.paso();
            if (sqlite3_column_int(consulta.get(), 0) != 0)
                throw ErrorUsuarios("La base ya fue inicializada. No se modificaron usuarios.");
        }
        insertar(nombre, clave, Rol::ADMINISTRADOR);
        ejecutar("COMMIT;");
    } catch (...) {
        sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
        throw;
    }
}
void RepositorioUsuarios::agregarOperador(const std::string& nombre,
                                         const std::string& clave) {
    Sentencia consulta(db_, "SELECT id FROM usuarios WHERE rol='ADMINISTRADOR';");
    if (consulta.paso() != SQLITE_ROW)
        throw ErrorUsuarios("Primero hay que inicializar el administrador.");
    insertar(nombre, clave, Rol::OPERADOR);
}
std::optional<Usuario> RepositorioUsuarios::buscar(const std::string& nombre) const {
    Sentencia consulta(db_, "SELECT id,nombre,rol,habilitado FROM usuarios WHERE nombre=?;");
    consulta.texto(1, nombre);
    if (consulta.paso() == SQLITE_DONE) return std::nullopt;
    return leerUsuario(consulta.get());
}
std::vector<Usuario> RepositorioUsuarios::listar() const {
    Sentencia consulta(db_, "SELECT id,nombre,rol,habilitado FROM usuarios ORDER BY id;");
    std::vector<Usuario> usuarios;
    while (consulta.paso() == SQLITE_ROW) usuarios.push_back(leerUsuario(consulta.get()));
    return usuarios;
}
bool RepositorioUsuarios::verificarClave(const std::string& nombre,
                                        const std::string& clave) const {
    if (clave.empty() || clave.size() > 256) return false;
    Sentencia consulta(db_, "SELECT clave_hash FROM usuarios WHERE nombre=?;");
    consulta.texto(1, nombre);
    if (consulta.paso() == SQLITE_DONE) return false;
    const auto hash = texto(consulta.get(), 0);
    return crypto_pwhash_str_verify(hash.c_str(), clave.data(), clave.size()) == 0;
}

bool RepositorioUsuarios::cambiarHabilitacionOperador(
    const std::string& nombre, bool habilitado) {
    validarNombre(nombre);
    // El WHERE impide modificar ADMINISTRADOR, incluso si se llama directamente
    // al repositorio sin pasar por ServicioProtegido.
    Sentencia consulta(db_,
        "UPDATE usuarios SET habilitado=? WHERE nombre=? AND rol='OPERADOR';");
    consulta.entero(1, habilitado ? 1 : 0);
    consulta.texto(2, nombre);
    consulta.paso();
    return sqlite3_changes(db_) == 1;
}
} // namespace robotrpc

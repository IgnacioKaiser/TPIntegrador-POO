#include "robotrpc/repositorio_usuarios.hpp"
#include <sqlite3.h>
#include <iostream>
#include <stdexcept>

void exigir(bool condicion, const char* mensaje) {
    if (!condicion) throw std::runtime_error(mensaje);
}

int main(int argc, char* argv[]) {
    if (argc != 2) return 1; // Base temporal unica aportada por el ejecutor Python.
    const std::string ruta = argv[1];
    try {
        using namespace robotrpc;
        {
            RepositorioUsuarios repo(ruta);
            bool rechazado = false;
            try { repo.agregarOperador("pedro", "ClavePrueba1"); }
            catch (const ErrorUsuarios&) { rechazado = true; }
            exigir(rechazado, "No debe haber operadores antes del administrador.");
            repo.inicializarAdministrador("admin", "ClaveAdmin1");
            repo.agregarOperador("pedro", "ClavePedro1");
            exigir(repo.listar().size() == 2, "Deben existir dos usuarios.");
            exigir(repo.buscar("admin")->rol == Rol::ADMINISTRADOR, "Rol incorrecto.");
            exigir(repo.verificarClave("pedro", "ClavePedro1"), "Clave valida rechazada.");
            exigir(!repo.verificarClave("pedro", "otra") &&
                   !repo.verificarClave("inexistente", "ClavePedro1"),
                   "Clave invalida aceptada.");
            rechazado = false;
            try { repo.agregarOperador("pedro", "OtraClave1"); }
            catch (const ErrorUsuarios&) { rechazado = true; }
            exigir(rechazado && repo.verificarClave("pedro", "ClavePedro1"),
                   "Un duplicado no debe reemplazar al usuario.");
            rechazado = false;
            try { repo.inicializarAdministrador("otro_admin", "OtraClave1"); }
            catch (const ErrorUsuarios&) { rechazado = true; }
            exigir(rechazado && repo.listar().size() == 2,
                   "La inicializacion no debe repetirse.");
            rechazado = false;
            try { repo.agregarOperador("corta", "123"); }
            catch (const ErrorUsuarios&) { rechazado = true; }
            exigir(rechazado && !repo.buscar("corta"), "Clave corta aceptada.");
            rechazado = false;
            try { repo.agregarOperador("mal'nombre", "ClavePrueba1"); }
            catch (const ErrorUsuarios&) { rechazado = true; }
            exigir(rechazado, "Nombre invalido aceptado.");

            // US-05: cambio de habilitacion, sin borrar ni modificar claves.
            exigir(repo.buscar("pedro")->habilitado, "El operador nuevo debe estar habilitado.");
            exigir(repo.cambiarHabilitacionOperador("pedro", false),
                   "No se pudo deshabilitar al operador.");
            exigir(!repo.buscar("pedro")->habilitado, "No se guardo la deshabilitacion.");
            exigir(repo.verificarClave("pedro", "ClavePedro1"),
                   "No debe cambiar la clave del operador.");
            exigir(!repo.cambiarHabilitacionOperador("admin", false),
                   "Se permitio deshabilitar al administrador.");
            exigir(repo.buscar("admin")->habilitado, "Administrador alterado.");
            exigir(!repo.cambiarHabilitacionOperador("nadie", false),
                   "Se acepto un operador inexistente.");
        }
        {
            RepositorioUsuarios reabierto(ruta);
            exigir(reabierto.listar().size() == 2 &&
                   reabierto.verificarClave("admin", "ClaveAdmin1"),
                   "Se perdieron usuarios al reabrir.");
            exigir(!reabierto.buscar("pedro")->habilitado,
                   "El estado deshabilitado no persistio.");
            exigir(reabierto.cambiarHabilitacionOperador("pedro", true),
                   "No se pudo volver a habilitar.");
            exigir(reabierto.buscar("pedro")->habilitado,
                   "El operador no quedo habilitado.");
        }
        sqlite3* db = nullptr;
        exigir(sqlite3_open(ruta.c_str(), &db) == SQLITE_OK,
               "No se pudo inspeccionar la base.");
        const int resultado = sqlite3_exec(db,
            "INSERT INTO usuarios(nombre,clave_hash,rol) VALUES('otro','hash','ADMINISTRADOR');",
            nullptr, nullptr, nullptr);
        sqlite3_close(db);
        exigir(resultado == SQLITE_CONSTRAINT,
               "La base debe impedir un segundo administrador.");
        std::cout << "Usuarios: persistencia, contrasenas y habilitacion US-05 OK.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

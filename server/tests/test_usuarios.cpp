#include "robotrpc/repositorio_usuarios.hpp"
#include <sqlite3.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>

void exigir(bool condicion, const char* mensaje) {
    if (!condicion) throw std::runtime_error(mensaje);
}
int main(int argc, char* argv[]) {
    if (argc != 2) return 1; // El ejecutor Python aporta una base temporal unica.
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
                   !repo.verificarClave("inexistente", "ClavePedro1"), "Clave invalida aceptada.");
            rechazado = false;
            try { repo.agregarOperador("pedro", "OtraClave1"); }
            catch (const ErrorUsuarios&) { rechazado = true; }
            exigir(rechazado && repo.verificarClave("pedro", "ClavePedro1"),
                   "Un duplicado no debe reemplazar al usuario.");
            rechazado = false;
            try { repo.inicializarAdministrador("otro_admin", "OtraClave1"); }
            catch (const ErrorUsuarios&) { rechazado = true; }
            exigir(rechazado && repo.listar().size() == 2, "La inicializacion no debe repetirse.");
            rechazado = false;
            try { repo.agregarOperador("corta", "123"); }
            catch (const ErrorUsuarios&) { rechazado = true; }
            exigir(rechazado && !repo.buscar("corta"), "Clave corta aceptada.");
            rechazado = false;
            try { repo.agregarOperador("mal'nombre", "ClavePrueba1"); }
            catch (const ErrorUsuarios&) { rechazado = true; }
            exigir(rechazado, "Nombre invalido aceptado.");
        }
        {
            RepositorioUsuarios reabierto(ruta);
            exigir(reabierto.listar().size() == 2 &&
                   reabierto.verificarClave("admin", "ClaveAdmin1"), "Se perdieron usuarios al reabrir.");
        }
        // Comprobar la restriccion incluso si alguien evita la API de C++.
        sqlite3* db = nullptr;
        exigir(sqlite3_open(ruta.c_str(), &db) == SQLITE_OK, "No se pudo inspeccionar la base.");
        const int resultado = sqlite3_exec(db,
            "INSERT INTO usuarios(nombre,clave_hash,rol) VALUES('otro','hash','ADMINISTRADOR');",
            nullptr, nullptr, nullptr);
        sqlite3_close(db);
        exigir(resultado == SQLITE_CONSTRAINT, "La base debe impedir un segundo administrador.");
        std::cout << "Usuarios: persistencia, credenciales, duplicados y administrador unico OK.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

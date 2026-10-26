#include "robotrpc/repositorio_usuarios.hpp"
#include <iostream>
#include <string>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

namespace {
std::string leer(const char* mensaje) {
    std::cout << mensaje << std::flush;
    std::string valor;
    if (!std::getline(std::cin, valor)) throw robotrpc::ErrorUsuarios("Entrada interrumpida.");
    return valor;
}
std::string leerClave(const char* mensaje) {
    std::cout << mensaje << std::flush;
    // Ocultar la entrada solo cuando se esta leyendo desde una terminal.
#ifdef _WIN32
    HANDLE entrada = GetStdHandle(STD_INPUT_HANDLE);
    DWORD modo = 0;
    const bool terminal = GetConsoleMode(entrada, &modo) != 0;
    if (terminal && !SetConsoleMode(entrada, modo & ~ENABLE_ECHO_INPUT))
        throw robotrpc::ErrorUsuarios("No se pudo ocultar la clave.");
#else
    termios modo{};
    const bool terminal = tcgetattr(STDIN_FILENO, &modo) == 0;
    if (terminal) {
        termios oculto = modo;
        oculto.c_lflag &= ~ECHO;
        if (tcsetattr(STDIN_FILENO, TCSANOW, &oculto) != 0)
            throw robotrpc::ErrorUsuarios("No se pudo ocultar la clave.");
    }
#endif
    std::string clave;
    const bool correcto = static_cast<bool>(std::getline(std::cin, clave));
#ifdef _WIN32
    if (terminal) SetConsoleMode(entrada, modo);
#else
    if (terminal) tcsetattr(STDIN_FILENO, TCSANOW, &modo);
#endif
    std::cout << '\n';
    if (!correcto) throw robotrpc::ErrorUsuarios("Entrada interrumpida.");
    return clave;
}
std::string nuevaClave() {
    const auto clave = leerClave("Nueva clave: ");
    if (clave != leerClave("Repetir clave: "))
        throw robotrpc::ErrorUsuarios("Las claves no coinciden.");
    return clave;
}
}

int main(int argc, char* argv[]) {
    try {
        if ((argc != 2 && argc != 4) ||
            (argc == 4 && std::string(argv[2]) != "--db")) {
            std::cerr << "Uso: usuarios_admin inicializar|agregar|listar [--db ruta]\n";
            return 1;
        }
        const std::string accion = argv[1];
        if (accion != "inicializar" && accion != "agregar" && accion != "listar")
            throw robotrpc::ErrorUsuarios("Accion desconocida.");
        robotrpc::RepositorioUsuarios repositorio(
            argc == 4 ? argv[3] : "data/db/usuarios.sqlite");
        if (accion == "inicializar") {
            const auto nombre = leer("Nombre del administrador inicial: ");
            const auto clave = nuevaClave();
            repositorio.inicializarAdministrador(nombre, clave);
            std::cout << "Administrador creado.\n";
            return 0;
        }

        const auto nombre = leer("Usuario administrador: ");
        const auto clave = leerClave("Clave del administrador: ");
        const auto usuario = repositorio.buscar(nombre);
        if (!usuario || !repositorio.verificarClave(nombre, clave))
            throw robotrpc::ErrorUsuarios("Usuario o clave incorrectos.");
        if (!usuario->habilitado || usuario->rol != robotrpc::Rol::ADMINISTRADOR)
            throw robotrpc::ErrorUsuarios("Se requiere un administrador habilitado.");

        if (accion == "agregar") {
            const auto nuevoNombre = leer("Nombre del nuevo operador: ");
            const auto nueva = nuevaClave();
            repositorio.agregarOperador(nuevoNombre, nueva);
            std::cout << "Operador creado y habilitado.\n";
        } else {
            std::cout << "ID\tUsuario\tRol\tHabilitado\n";
            for (const auto& u : repositorio.listar())
                std::cout << u.id << '\t' << u.nombre << '\t'
                          << (u.rol == robotrpc::Rol::ADMINISTRADOR ? "ADMINISTRADOR" : "OPERADOR")
                          << '\t' << (u.habilitado ? "si" : "no") << '\n';
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}

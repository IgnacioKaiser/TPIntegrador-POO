#include "robotrpc/servicio_protegido.hpp"
#include <iostream>
#include <stdexcept>
#include <string>

static void exigir(bool condicion, const char* mensaje) {
    if (!condicion) throw std::runtime_error(mensaje);
}

int main(int argc, char* argv[]) {
    if (argc != 2) return 1;
    try {
        using namespace robotrpc;
        RepositorioUsuarios repo(argv[1]);
        repo.inicializarAdministrador("admin", "ClaveAdmin1");
        repo.agregarOperador("operador", "ClaveOperador1");
        repo.agregarOperador("pedro", "ClavePedro1");
        Autenticador autenticador(repo);
        ServicioPrueba servicio;
        ServicioProtegido protegido(autenticador, servicio, repo);

        auto solicitud = [](const std::string& actor, const std::string& clave,
                            const std::string& operacion,
                            EstructuraRPC datos = {}) {
            datos["credenciales"] = EstructuraRPC{{"usuario", actor}, {"clave", clave}};
            return MensajeRPC::solicitud("sol-05", operacion, datos);
        };
        auto controlar = [&](const MensajeRPC& s, const std::string& codigo) {
            const auto respuesta = protegido.atender(s);
            exigir(respuesta.codigo() == codigo, "Codigo de respuesta incorrecto.");
            exigir(respuesta.id() == s.id() && respuesta.operacion() == s.operacion(),
                   "Se perdio la correlacion de la respuesta.");
        };
        auto estado = [&](const std::string& nombre) -> bool {
            return repo.buscar(nombre)->habilitado;
        };

        controlar(solicitud("operador", "ClaveOperador1", "ping"), "OK");
        controlar(solicitud("operador", "Incorrecta", "ping"), "NO_AUTENTICADO");
        controlar(solicitud("nadie", "Incorrecta", "ping"), "NO_AUTENTICADO");
        controlar(MensajeRPC::solicitud("sin-cred", "ping"), "NO_AUTENTICADO");
        controlar(MensajeRPC::solicitud("mal", "ping", {{"credenciales", "mal"}}),
                  "VALIDACION");

        // Un operador autenticado no puede modificar a otro operador.
        controlar(solicitud("operador", "ClaveOperador1", "usuario.deshabilitar",
                             {{"usuario", "pedro"}}), "PERMISO_DENEGADO");
        exigir(estado("pedro"), "Operador sin permisos modifico otro usuario.");

        // Tampoco debe poder modificarlo si omite o falsifica las credenciales.
        controlar(solicitud("admin", "Incorrecta", "usuario.deshabilitar",
                             {{"usuario", "pedro"}}), "NO_AUTENTICADO");
        exigir(estado("pedro"), "Una credencial invalida modifico la base.");

        // Administrador: prueba del recorrido completo hasta SQLite.
        controlar(solicitud("admin", "ClaveAdmin1", "usuario.deshabilitar",
                             {{"usuario", "pedro"}}), "OK");
        exigir(!estado("pedro"), "No se deshabilito el operador.");
        controlar(solicitud("pedro", "ClavePedro1", "ping"), "PERMISO_DENEGADO");
        controlar(solicitud("pedro", "ClavePedro1", "eco", {{"texto", "hola"}}),
                  "PERMISO_DENEGADO");

        controlar(solicitud("admin", "ClaveAdmin1", "usuario.habilitar",
                             {{"usuario", "pedro"}}), "OK");
        exigir(estado("pedro"), "No se volvio a habilitar el operador.");
        controlar(solicitud("pedro", "ClavePedro1", "ping"), "OK");

        // No se puede cambiar la habilitacion del administrador inicial.
        controlar(solicitud("admin", "ClaveAdmin1", "usuario.deshabilitar",
                             {{"usuario", "admin"}}), "PERMISO_DENEGADO");
        exigir(estado("admin"), "Se cambio el estado del administrador.");
        controlar(solicitud("admin", "ClaveAdmin1", "usuario.habilitar",
                             {{"usuario", "admin"}}), "PERMISO_DENEGADO");

        controlar(solicitud("admin", "ClaveAdmin1", "usuario.deshabilitar",
                             {{"usuario", "inexistente"}}), "RECURSO_NO_ENCONTRADO");
        controlar(solicitud("admin", "ClaveAdmin1", "usuario.deshabilitar"), "VALIDACION");
        controlar(solicitud("admin", "ClaveAdmin1", "usuario.deshabilitar",
                             {{"usuario", 12}}), "VALIDACION");
        controlar(solicitud("admin", "ClaveAdmin1", "usuario.deshabilitar",
                             {{"usuario", "mala clave"}}), "VALIDACION");
        controlar(solicitud("admin", "ClaveAdmin1", "usuario.deshabilitar",
                             {{"usuario", "pedro"}, {"extra", true}}), "VALIDACION");

        // Una misma solicitud repetida deja el estado final igual: es idempotente.
        controlar(solicitud("admin", "ClaveAdmin1", "usuario.deshabilitar",
                             {{"usuario", "pedro"}}), "OK");
        controlar(solicitud("admin", "ClaveAdmin1", "usuario.deshabilitar",
                             {{"usuario", "pedro"}}), "OK");
        exigir(!estado("pedro"), "La deshabilitacion repetida altero el estado.");

        std::cout << "Autenticacion y US-05: permisos, rechazo y cambios en SQLite OK.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

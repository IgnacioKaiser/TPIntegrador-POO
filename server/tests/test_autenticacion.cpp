#include "robotrpc/servicio_protegido.hpp"
#include <sqlite3.h>
#include <iostream>

void exigir(bool valor, const char* mensaje) {
    if (!valor) throw std::runtime_error(mensaje);
}
// Servicio espia: permite demostrar que un rechazo no ejecuta la operacion.
class ServicioEspia : public robotrpc::ServicioPrueba {
public:
    mutable int ejecuciones = 0;
    robotrpc::MensajeRPC atender(const robotrpc::MensajeRPC& s) const override {
        ++ejecuciones;
        exigir(!s.parametros().count("credenciales"), "Se filtraron las claves al servicio.");
        return robotrpc::MensajeRPC::respuestaA(s, {{"mensaje", "ejecutado"}});
    }
};
void ejecutarSQL(const std::string& ruta, const char* sql) {
    sqlite3* db = nullptr;
    if (sqlite3_open(ruta.c_str(), &db) != SQLITE_OK) {
        sqlite3_close(db);
        throw std::runtime_error("No se pudo abrir la base temporal.");
    }
    const int resultado = sqlite3_exec(db, sql, nullptr, nullptr, nullptr);
    sqlite3_close(db);
    exigir(resultado == SQLITE_OK, "No se pudo preparar el estado de prueba.");
}
int main(int argc, char* argv[]) {
    if (argc != 2) return 1;
    try {
        using namespace robotrpc;
        RepositorioUsuarios repo(argv[1]);
        repo.inicializarAdministrador("admin", "ClaveAdmin1");
        repo.agregarOperador("operador", "ClaveOperador1");
        Autenticador autenticador(repo);
        ServicioEspia espia;
        ServicioProtegido protegido(autenticador, espia);
        auto solicitud = [](const std::string& nombre, const std::string& clave) {
            return MensajeRPC::solicitud("s1", "eco", {
                {"texto", "hola"}, {"credenciales", EstructuraRPC{{"usuario", nombre}, {"clave", clave}}}});
        };
        auto rechazar = [&](const MensajeRPC& s, const std::string& codigo) {
            const int antes = espia.ejecuciones;
            const auto r = protegido.atender(s);
            exigir(r.codigo() == codigo, "Codigo de rechazo incorrecto.");
            exigir(r.id() == s.id(), "Se perdio la correlacion de respuesta.");
            exigir(espia.ejecuciones == antes, "Se ejecuto una peticion rechazada.");
        };
        exigir(protegido.atender(solicitud("operador", "ClaveOperador1")).codigo() == "OK",
               "Credenciales validas rechazadas.");
        exigir(espia.ejecuciones == 1, "La operacion valida no se ejecuto una sola vez.");
        rechazar(solicitud("operador", "Incorrecta"), "NO_AUTENTICADO");
        rechazar(solicitud("nadie", "Incorrecta"), "NO_AUTENTICADO");
        rechazar(MensajeRPC::solicitud("s2", "eco"), "NO_AUTENTICADO");
        rechazar(MensajeRPC::solicitud("s3", "eco", {{"credenciales", "mal"}}), "VALIDACION");
        rechazar(MensajeRPC::solicitud("s4", "eco", {{"credenciales", EstructuraRPC{
            {"usuario", 5}, {"clave", "Incorrecta"}}}}), "VALIDACION");
        // Mismas credenciales, siguiente peticion: debe volver a consultar el estado.
        ejecutarSQL(argv[1], "UPDATE usuarios SET habilitado=0 WHERE nombre='operador';");
        rechazar(solicitud("operador", "ClaveOperador1"), "PERMISO_DENEGADO");
        ejecutarSQL(argv[1], "UPDATE usuarios SET habilitado=1 WHERE nombre='operador';");
        exigir(protegido.atender(solicitud("operador", "ClaveOperador1")).codigo() == "OK",
               "La nueva peticion no refleja la habilitacion actual.");
        ejecutarSQL(argv[1], "ALTER TABLE usuarios RENAME TO usuarios_temporales;");
        rechazar(solicitud("operador", "ClaveOperador1"), "ERROR_INTERNO");
        ejecutarSQL(argv[1], "ALTER TABLE usuarios_temporales RENAME TO usuarios;");
        std::cout << "Autenticacion: validacion por peticion y ausencia de ejecucion ante rechazos OK.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

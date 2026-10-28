#include "robotrpc/registro_actividad.hpp"
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>

using namespace robotrpc;
namespace fs = std::filesystem;

int main() {
    const auto base = fs::temp_directory_path() /
        ("us182021_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto csv = (base / "log.csv").string();
    try {
        {
            RegistroActividadCsv log(csv);
            log.registrarEvento("ServidorRPC", "inicio", "OK", "Servidor abierto");
            log.registrarPeticion("abc", "hector", "127.0.0.1", "ping", "OK");
            log.registrarPeticion("def", "martin", "127.0.0.1", "eco", "VALIDACION");
            log.registrarPeticion("ghi", "", "127.0.0.1", "ping", "NO_AUTENTICADO");
            log.registrarEvento("Robot", "conexion", "ERROR_INTERNO", "Puerto no disponible");
        }
        RegistroActividadCsv log(csv);  // Verifica persistencia entre instancias.
        FiltroActividad todos;
        assert(log.consultar(todos).total == 5);
        FiltroActividad hector; hector.usuario = "hector";
        assert(log.consultar(hector).total == 1);
        FiltroActividad eventos; eventos.tipo = "EVENTO";
        assert(log.consultar(eventos).total == 2);
        FiltroActividad errores; errores.codigo = "VALIDACION";
        assert(log.consultar(errores).total == 1);
        FiltroActividad pagina; pagina.limite = 2; pagina.offset = 1;
        assert(log.consultar(pagina).total == 5);
        assert(log.consultar(pagina).registros.size() == 2);
        auto resumen = log.informeUsuarios(todos);
        assert(resumen.size() == 2);
        assert(resumen[0].usuario == "hector" && resumen[0].exitosas == 1);
        assert(resumen[1].usuario == "martin" && resumen[1].rechazadas == 1);
        FiltroActividad fechas; fechas.desde = "2026-02-31";
        bool fallo = false;
        try { log.consultar(fechas); } catch (const std::invalid_argument&) { fallo = true; }
        assert(fallo);
        std::cout << "US-18/20/21: pruebas CSV, fechas, filtros y persistencia OK\n";
    } catch (...) {
        fs::remove_all(base);
        throw;
    }
    fs::remove_all(base);
}

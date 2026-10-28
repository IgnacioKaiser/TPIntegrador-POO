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
        // US-19: aislamiento y periodo en orden de archivo, aun en el mismo segundo.
        assert(log.actividadPropia("nadie").pagina.total == 0);
        assert(log.actividadPropia("hector").pagina.total == 1);
        assert(log.actividadPropia("martin").errores == 1);
        log.registrarEvento("Robot", "conectar", "OK", "Conexion de prueba");
        assert(log.actividadPropia("hector").pagina.total == 0);
        log.registrarPeticion("uno", "hector", "127.0.0.1", "mover", "VALIDACION");
        log.registrarPeticion("otro", "martin", "127.0.0.1", "ping", "OK");
        log.registrarPeticion("dos", "hector", "127.0.0.1", "estado", "OK");
        log.registrarEvento("Robot", "conectar", "OK", "Ya estaba conectado; no reinicia periodo");
        log.registrarEvento("Robot", "conectar", "PERMISO_DENEGADO", "Rechazo");
        log.registrarEvento("Robot", "desconectar", "OK", "Desconexion");
        const auto propia = log.actividadPropia("hector", 1, 1);
        assert(propia.referencia == "conectar" && !propia.desde.empty());
        assert(!propia.inicio_actividad.empty());
        assert(propia.pagina.total == 2 && propia.errores == 1);
        assert(propia.pagina.registros.size() == 1);
        assert(propia.pagina.registros[0].id_solicitud == "dos");
        assert(log.actividadPropia("hector", 2).pagina.registros.empty());
        log.registrarEvento("Robot", "conectar", "OK", "Reconectar tras desconexion");
        assert(log.actividadPropia("hector").pagina.total == 0);
        assert(log.informeUsuarios(todos)[0].peticiones == 3); // Global sigue acumulativo.
        for (const auto limite : {std::size_t(0), std::size_t(101)}) {
            fallo = false;
            try { log.actividadPropia("hector", 0, limite); }
            catch (const std::invalid_argument&) { fallo = true; }
            assert(fallo);
        }
        fallo = false;
        try { log.actividadPropia("hector", 100000001); }
        catch (const std::invalid_argument&) { fallo = true; }
        assert(fallo);
        fallo = false;
        try { log.actividadPropia(""); }
        catch (const std::invalid_argument&) { fallo = true; }
        assert(fallo);
        log.registrarEvento("ServidorRPC", "inicio", "OK", "Reinicio");
        const auto reinicio = log.actividadPropia("hector");
        assert(reinicio.referencia == "inicio" && reinicio.inicio_actividad.empty());
        assert(reinicio.pagina.total == 0 && reinicio.errores == 0);
        std::cout << "US-19: propiedad, periodo, paginacion, errores y reinicio OK\n";
        std::cout << "US-18/20/21: pruebas CSV, fechas, filtros y persistencia OK\n";
    } catch (...) {
        fs::remove_all(base);
        throw;
    }
    fs::remove_all(base);
}

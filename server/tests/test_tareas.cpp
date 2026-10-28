#include "robotrpc/repositorio_tareas.hpp"
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>

namespace fs = std::filesystem;
using namespace robotrpc;

int comprobaciones = 0;
void exigir(bool condicion, const char* mensaje) {
    ++comprobaciones;
    if (!condicion) throw std::runtime_error(mensaje);
}

void exigirError(TipoErrorTarea tipo, const std::function<void()>& accion) {
    try { accion(); }
    catch (const ErrorTarea& error) {
        exigir(error.tipo() == tipo, "Tipo de error incorrecto.");
        return;
    }
    throw std::runtime_error("La operacion debia rechazarse.");
}

std::string leer(const fs::path& ruta) {
    std::ifstream entrada(ruta, std::ios::binary);
    if (!entrada) throw std::runtime_error("No se pudo leer el archivo de prueba.");
    return {std::istreambuf_iterator<char>(entrada), std::istreambuf_iterator<char>()};
}

void escribir(const fs::path& ruta, const std::string& texto) {
    std::ofstream salida(ruta, std::ios::binary);
    salida << texto;
    salida.close();
    if (!salida) throw std::runtime_error("No se pudo preparar el archivo de prueba.");
}

int main(int argc, char* argv[]) {
    if (argc != 2) return 1;
    try {
        // El ejecutor Python entrega una carpeta temporal, nunca datos del proyecto.
        const fs::path temporal = argv[1];
        const auto raiz = temporal / "tareas";
        const Usuario admin{1, "administrador", Rol::ADMINISTRADOR, true};
        const Usuario ana{2, "ana", Rol::OPERADOR, true};
        const Usuario bruno{3, "bruno", Rol::OPERADOR, true};
        const Usuario deshabilitado{2, "ana", Rol::OPERADOR, false};
        const Usuario invalido{0, "sin_identidad", Rol::ADMINISTRADOR, true};
        const std::string programa = "; tarea de prueba\nG90\nG1X10Y20Z30F7.5\nM3\nM5\n";

        {
            RepositorioTareas repo(raiz);
            exigir(repo.listar(ana).empty(), "Un usuario sin tareas debe obtener lista vacia.");
            const auto tarea = repo.guardar(ana, "Pieza.GCODE", programa);
            exigir(tarea.id == "2:pieza.gcode" && tarea.propietario_id == 2 && tarea.nombre == "pieza.gcode",
                   "Identidad o nombre normalizado incorrectos.");
            exigir(leer(raiz / "2" / "pieza.gcode") == programa, "Se altero el contenido.");
            exigir(!fs::exists(raiz / "2" / ".pieza.gcode.tmp"), "Quedo un temporal publicado.");
            exigirError(TipoErrorTarea::DUPLICADA, [&] { repo.guardar(ana, "PIEZA.gcode", "M18"); });
            exigir(leer(raiz / "2" / "pieza.gcode") == programa, "Un duplicado sobrescribio la tarea.");
            repo.guardar(bruno, "pieza.gcode", "M114");
            repo.guardar(admin, "propia.nc", "G28");
            exigir(repo.listar(ana).size() == 1 && repo.listar(bruno).size() == 1,
                   "Un operador ve tareas de otros propietarios.");
            const auto todas = repo.listar(admin);
            exigir(todas.size() == 3 && todas[0].id == "1:propia.nc" &&
                   todas[1].id == "2:pieza.gcode" && todas[2].id == "3:pieza.gcode",
                   "Listado global u orden incorrecto.");
            for (const auto& usuario : {deshabilitado, invalido}) {
                exigirError(TipoErrorTarea::ACCESO_DENEGADO, [&] { repo.listar(usuario); });
                exigirError(TipoErrorTarea::ACCESO_DENEGADO, [&] { repo.guardar(usuario, "no.nc", "M17"); });
            }
        }
        RepositorioTareas repo(raiz);
        exigir(repo.listar(admin).size() == 3 && leer(raiz / "2" / "pieza.gcode") == programa,
               "Las tareas no persistieron al reabrir el repositorio.");

        // Nombres portables y rutas rechazadas sin crear archivos fuera del almacen.
        for (const auto& nombre : {"", ".gcode", "../escape.nc", "/escape.nc", "C:\\escape.nc",
                                   "sub/tarea.nc", "sub\\tarea.nc", "otra.txt", "a..nc", "a b.nc",
                                   "con.nc", "LPT1.gcode", "PRN.nc", "a:nc", "tarea.nc "})
            exigirError(TipoErrorTarea::VALIDACION, [&] { repo.guardar(ana, nombre, "G28"); });
        exigirError(TipoErrorTarea::VALIDACION, [&] { repo.guardar(ana, u8"niño.nc", "G28"); });
        repo.guardar(ana, std::string(58, 'a') + ".gcode", "G28"); // 64 caracteres.
        exigirError(TipoErrorTarea::VALIDACION, [&] {
            repo.guardar(ana, std::string(59, 'a') + ".gcode", "G28");
        });

        // Formas usadas por el firmware: minusculas, espacios, compacto y decimales.
        // Comentarios, N y % se aceptan para almacenar, no certifican ejecucion.
        const std::string exportado = u8"\xef\xbb\xbf%\r\n; prueba ñ 😀\r\n(inicio\r\n comentario)\r\n"
                                      "n10 g01x-1.5y+2z.5f7.5 ; nota\r"
                                      "G 90\nM 03\r\nG92 X0 Y0 Z0 E0\n\nM114\n%";
        repo.guardar(ana, "exportado.nc", exportado);
        const auto guardado = leer(raiz / "2" / "exportado.nc");
        exigir(guardado.rfind("%\n", 0) == 0 && guardado.find('\r') == std::string::npos,
               "No se normalizaron BOM o finales de linea.");
        exigir(guardado.find(u8"; prueba ñ 😀") != std::string::npos &&
               guardado.find("\n\nM114") != std::string::npos,
               "Se perdieron comentarios UTF-8 o lineas vacias.");
        repo.guardar(ana, "sin_ultima_linea.nc", "m18");
        repo.guardar(ana, "sin_interpretar.nc", "G999 X999999 F-50");
        exigir(fs::exists(raiz / "2" / "sin_interpretar.nc"),
               "El almacen no debe interpretar comandos, movimientos ni velocidades.");

        for (const auto& contenido : {"", " \n\t", "; sin orden", "(G1 X0)\n%", "hola", "G X0",
                                      "N G1", "G1\ntexto arbitrario", "G1 (sin cerrar"})
            exigirError(TipoErrorTarea::VALIDACION, [&] { repo.guardar(ana, "invalida.nc", contenido); });
        for (const auto& bytes : {std::string("G1\0", 3), std::string("G1\x01", 3),
                                  std::string("G1;\xc0\xaf"), std::string("G1;\xed\xa0\x80"),
                                  std::string("G1;\xf0\x9f"), std::string("G1;\xf4\x90\x80\x80"),
                                  std::string("G1;\xef\xbf\xbe")})
            exigirError(TipoErrorTarea::VALIDACION, [&] { repo.guardar(ana, "invalida.nc", bytes); });
        const std::string limite = "G1\n;" + std::string(RepositorioTareas::MAX_CONTENIDO - 4, 'a');
        repo.guardar(ana, "limite.nc", limite);
        exigir(fs::file_size(raiz / "2" / "limite.nc") == RepositorioTareas::MAX_CONTENIDO,
               "Se rechazo o altero el limite de 1 MiB.");
        exigirError(TipoErrorTarea::VALIDACION, [&] { repo.guardar(ana, "grande.nc", limite + "a"); });
        exigir(!fs::exists(raiz / "2" / "invalida.nc") && !fs::exists(raiz / "2" / "grande.nc"),
               "Una entrada invalida creo una tarea.");

        // Temporal de un cierre inesperado: no debe aparecer en el listado.
        escribir(raiz / "2" / ".pendiente.nc.tmp", "incompleto");
        const auto cantidad = repo.listar(ana).size();
        RepositorioTareas reabierto(raiz);
        exigir(reabierto.listar(ana).size() == cantidad, "Se listo un temporal como tarea.");
        reabierto.guardar(ana, "pendiente.nc", "G28");
        exigir(leer(raiz / "2" / "pendiente.nc") == "G28", "No se recupero el temporal anterior.");

        // Fallo real de sistema de archivos sin mocks ni quitar permisos de Windows.
        fs::create_directory(raiz / "2" / ".fallo.nc.tmp");
        exigirError(TipoErrorTarea::ALMACENAMIENTO, [&] { repo.guardar(ana, "fallo.nc", "M17"); });
        exigir(!fs::exists(raiz / "2" / "fallo.nc") &&
               fs::is_directory(raiz / "2" / ".fallo.nc.tmp"),
               "Se publico contenido parcial o se elimino un obstaculo ajeno.");
        fs::remove(raiz / "2" / ".fallo.nc.tmp");
        repo.guardar(ana, "fallo.nc", "M17");
        exigir(leer(raiz / "2" / "fallo.nc") == "M17", "No se pudo guardar despues del fallo.");
        escribir(raiz / "6", "no es una carpeta");
        const Usuario carpetaInvalida{6, "otro", Rol::OPERADOR, true};
        exigirError(TipoErrorTarea::ALMACENAMIENTO, [&] { repo.guardar(carpetaInvalida, "a.nc", "G28"); });
        exigirError(TipoErrorTarea::ALMACENAMIENTO, [&] { RepositorioTareas otro(raiz / "6"); });

        // Estas pruebas se omiten si Windows no concede crear enlaces simbolicos.
        const auto fuera = temporal / "fuera";
        fs::create_directory(fuera);
        escribir(fuera / "secreto.nc", "M18");
        std::error_code errorEnlace;
        fs::create_directory_symlink(fuera, raiz / "4", errorEnlace);
        if (!errorEnlace) {
            const Usuario enlace{4, "enlace", Rol::OPERADOR, true};
            exigirError(TipoErrorTarea::ALMACENAMIENTO, [&] { repo.guardar(enlace, "escapa.nc", "G28"); });
            exigirError(TipoErrorTarea::ALMACENAMIENTO, [&] { repo.listar(enlace); });
            for (const auto& tarea : repo.listar(admin))
                exigir(tarea.propietario_id != 4, "El administrador recorrio un enlace externo.");
            exigir(!fs::exists(fuera / "escapa.nc"), "Se escribio fuera del directorio.");
        } else std::cout << "Enlaces de directorio: omitidos por permisos/plataforma.\n";
        fs::create_symlink(fuera / "secreto.nc", raiz / "2" / "enlace.nc", errorEnlace);
        if (!errorEnlace) {
            exigirError(TipoErrorTarea::ALMACENAMIENTO, [&] { repo.guardar(ana, "enlace.nc", "G28"); });
            for (const auto& tarea : repo.listar(ana))
                exigir(tarea.nombre != "enlace.nc", "El listado incluyo un enlace externo.");
            exigir(leer(fuera / "secreto.nc") == "M18", "Se altero un archivo externo.");
        } else std::cout << "Enlaces de archivo: omitidos por permisos/plataforma.\n";

        std::cout << "Tareas: " << comprobaciones << " comprobaciones correctas.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

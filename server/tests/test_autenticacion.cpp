#include "robotrpc/servicio_protegido.hpp"
#include <iostream>
#include <stdexcept>
#include <string>
#include <filesystem>

// Solo en pruebas: cuenta consultas del robot y simula un estado parcial.
class ServicioEspia : public robotrpc::ServicioRobot {
public:
    explicit ServicioEspia(Robot& robot) : ServicioRobot(robot) {}
    mutable int llamadas = 0;
    bool estadoParcial = false;
    robotrpc::MensajeRPC atender(const robotrpc::MensajeRPC& solicitud) const override {
        ++llamadas;
        if (estadoParcial && solicitud.operacion() == "estado")
            return robotrpc::MensajeRPC::respuestaA(solicitud,
                {{"conectado", true}, {"posicion", robotrpc::EstructuraRPC{
                    {"disponible", false}, {"mensaje", "ruta_interna_de_prueba"}}},
                 {"actividad", robotrpc::EstructuraRPC{}},
                 {"mensaje", "ruta_interna_de_prueba"}}, "ERROR_INTERNO");
        return ServicioRobot::atender(solicitud);
    }
};

static void exigir(bool condicion, const char* mensaje) {
    if (!condicion) throw std::runtime_error(mensaje);
}

int main(int argc, char* argv[]) {
    if (argc != 3) return 1;
    try {
        using namespace robotrpc;
        RepositorioUsuarios repo(argv[1]);
        repo.inicializarAdministrador("admin", "ClaveAdmin1");
        repo.agregarOperador("operador", "ClaveOperador1");
        repo.agregarOperador("pedro", "ClavePedro1");
        Autenticador autenticador(repo);
        Robot robot;
        ServicioEspia servicio(robot);
        RepositorioTareas tareas(argv[2]);
        ServicioProtegido protegido(autenticador, servicio, repo, tareas);

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

        auto verificarActual = [&](const std::string& nombre, const std::string& clave,
                                   const std::string& rol) {
            const auto s = solicitud(nombre, clave, "usuario.actual");
            controlar(s, "OK");
            const auto datos = protegido.atender(s).resultado();
            exigir(datos.size() == 2, "La identidad solo debe devolver usuario y rol.");
            exigir(datos.at("usuario").obtener<std::string>() == nombre,
                   "Se devolvio otra cuenta.");
            exigir(datos.at("rol").obtener<std::string>() == rol, "Rol incorrecto.");
        };
        verificarActual("admin", "ClaveAdmin1", "ADMINISTRADOR");
        verificarActual("operador", "ClaveOperador1", "OPERADOR");
        controlar(solicitud("operador", "Incorrecta", "usuario.actual"), "NO_AUTENTICADO");
        controlar(solicitud("nadie", "Incorrecta", "usuario.actual"), "NO_AUTENTICADO");
        controlar(MensajeRPC::solicitud("sin-cred", "usuario.actual"), "NO_AUTENTICADO");
        controlar(MensajeRPC::solicitud("mal", "usuario.actual", {{"credenciales", "mal"}}),
                  "VALIDACION");
        controlar(solicitud("operador", "ClaveOperador1", "usuario.actual",
                             {{"usuario", "admin"}}), "VALIDACION");
        controlar(solicitud("admin", "ClaveAdmin1", "usuario.actual",
                             {{"rol", "ADMINISTRADOR"}}), "VALIDACION");

        const int llamadasAntes = servicio.llamadas;
        controlar(solicitud("operador", "ClaveOperador1", "tarea.subir",
                             {{"nombre", "pieza.nc"}, {"contenido", "G28\nM3\n"}}), "OK");
        controlar(solicitud("operador", "ClaveOperador1", "tarea.subir",
                             {{"nombre", "PIEZA.NC"}, {"contenido", "M18"}}), "ESTADO_INVALIDO");
        controlar(solicitud("admin", "ClaveAdmin1", "tarea.listar"), "OK");
        controlar(solicitud("operador", "Incorrecta", "tarea.subir",
                             {{"nombre", "rechazada.nc"}, {"contenido", "G28"}}), "NO_AUTENTICADO");
        controlar(solicitud("operador", "ClaveOperador1", "tarea.subir",
                             {{"nombre", "otra.nc"}, {"contenido", "G28"},
                              {"propietario", "admin"}}), "VALIDACION");
        controlar(solicitud("operador", "ClaveOperador1", "tarea.listar",
                             {{"propietario", "admin"}}), "VALIDACION");
        exigir(servicio.llamadas == llamadasAntes, "Subir o listar invoco el servicio del robot.");
        exigir(tareas.listar(*repo.buscar("operador")).size() == 1,
               "Una solicitud rechazada escribio una tarea.");

        controlar(solicitud("operador", "ClaveOperador1", "conectar",
                             {{"puerto", "puerto-inexistente"}}),
                  "PERMISO_DENEGADO");
        controlar(solicitud("operador", "ClaveOperador1", "desconectar"),
                  "PERMISO_DENEGADO");
        controlar(solicitud("operador", "ClaveOperador1", "estado"), "OK");
        controlar(solicitud("operador", "ClaveOperador1", "activar_motores"),
                  "ESTADO_INVALIDO");
        controlar(solicitud("admin", "ClaveAdmin1", "conectar",
                             {{"puerto", "puerto-inexistente"}}),
                  "ERROR_CONEXION");
        controlar(solicitud("admin", "ClaveAdmin1", "desconectar"), "OK");

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
        controlar(solicitud("pedro", "ClavePedro1", "tarea.listar"), "PERMISO_DENEGADO");
        controlar(solicitud("pedro", "ClavePedro1", "tarea.subir",
                             {{"nombre", "rechazada.nc"}, {"contenido", "G28"}}), "PERMISO_DENEGADO");
        controlar(solicitud("pedro", "ClavePedro1", "usuario.actual"), "PERMISO_DENEGADO");
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

        // US-19: una solicitud rechazada no llega a consultar el estado del robot.
        RegistroActividadCsv registro((std::filesystem::path(argv[1]).parent_path() / "actividad.csv").string());
        ServicioProtegido conRegistro(autenticador, servicio, repo, tareas, &registro);
        const int antesInforme = servicio.llamadas;
        for (const auto& s : {
                 solicitud("operador", "Incorrecta", "informe.propio"),
                 solicitud("pedro", "ClavePedro1", "informe.propio"),
                 solicitud("operador", "ClaveOperador1", "informe.propio", {{"usuario", "admin"}}),
                 solicitud("operador", "ClaveOperador1", "informe.propio", {{"limite", 0}})}) {
            exigir(conRegistro.atender(s).codigo() != "OK", "Se acepto una consulta invalida.");
        }
        exigir(servicio.llamadas == antesInforme, "Un rechazo consulto el robot.");
        const auto propia = conRegistro.atender(solicitud("operador", "ClaveOperador1", "informe.propio"));
        exigir(propia.codigo() == "OK", "Fallo la consulta propia valida.");
        exigir(servicio.llamadas == antesInforme + 1, "No reutilizo una unica consulta de estado.");
        exigir(propia.resultado().at("usuario").obtener<std::string>() == "operador",
               "La consulta propia devolvio otra identidad.");
        servicio.estadoParcial = true;
        const auto parcial = conRegistro.atender(solicitud("operador", "ClaveOperador1", "informe.propio"));
        exigir(parcial.codigo() == "OK", "Un estado parcial oculto el informe.");
        exigir(parcial.resultado().at("codigo_estado").obtener<std::string>() == "ERROR_INTERNO",
               "Se oculto el fallo de estado.");
        const auto& datosEstado = parcial.resultado().at("estado").obtener<EstructuraRPC>();
        exigir(datosEstado.at("conectado").obtener<bool>(), "Se perdio el estado parcial conocido.");
        exigir(datosEstado.at("mensaje").obtener<std::string>().find("ruta_interna") == std::string::npos,
               "El informe expuso detalles internos de un error.");

        std::cout << "Autenticacion y US-05: permisos, rechazo y cambios en SQLite OK.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

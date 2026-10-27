#include "robotrpc/servidor.hpp"
#include "robotrpc/adaptador_xmlrpc.hpp"
#include "XmlRpc.h"
#include "XmlRpcSocket.h"

#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif
#include <csignal>
#include <iostream>
#include <string>

namespace robotrpc {
namespace {

volatile std::sig_atomic_t detener = 0;

void solicitarCierre(int) {
    detener = 1;
}

// Se mantiene la escucha local del prototipo original.
class ServidorLocal final : public XmlRpc::XmlRpcServer {
public:
    void abrir(unsigned short puerto) {

        const auto descriptor = XmlRpc::XmlRpcSocket::socket();

        if (descriptor == XmlRpc::InvalidSocket)
            throw ErrorServidor(
                "No se pudo crear el socket.");

        setfd(descriptor);

        sockaddr_in direccion{};
        direccion.sin_family = AF_INET;
        direccion.sin_port = htons(puerto);
        direccion.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

        // En Windows SO_REUSEADDR permitiria compartir el puerto con otro proceso.
#ifdef _WIN32
        const int exclusivo = 1;
        const bool opcionCorrecta = ::setsockopt(
            descriptor, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
            reinterpret_cast<const char*>(&exclusivo), sizeof(exclusivo)) == 0;
#else
        const bool opcionCorrecta = XmlRpc::XmlRpcSocket::setReuseAddr(descriptor);
#endif

        if (!XmlRpc::XmlRpcSocket::setNonBlocking(descriptor) ||
            !opcionCorrecta ||
            ::bind(
                descriptor,
                reinterpret_cast<const sockaddr*>(&direccion),
                sizeof(direccion)
            ) != 0 ||
            !XmlRpc::XmlRpcSocket::listen(descriptor, 16)) {

            const auto detalle =
                XmlRpc::XmlRpcSocket::getErrorMsg();

            close();

            throw ErrorServidor(
                "No se pudo escuchar en 127.0.0.1:" +
                std::to_string(puerto) + ": " + detalle
            );
        }

        _disp.addSource(
            this,
            XmlRpc::XmlRpcDispatch::ReadableEvent
        );
    }
};

class MetodoIntercambiar final
    : public XmlRpc::XmlRpcServerMethod {

public:
    MetodoIntercambiar(
        XmlRpc::XmlRpcServer& servidor,
        const ServicioRobot& servicio
    )
        : XmlRpcServerMethod(
            "robot.intercambiar", &servidor),
          servicio_(servicio) {}

    std::string help() override {
        return "Recibe un MensajeRPC de seis campos y devuelve su respuesta.";
    }

    void execute(
        XmlRpc::XmlRpcValue& argumentos,
        XmlRpc::XmlRpcValue& resultado
    ) override {

        try {
            // XML-RPC entrega los argumentos del metodo
            // dentro de un array.
            if (
                argumentos.getType() !=
                    XmlRpc::XmlRpcValue::TypeArray ||
                argumentos.size() != 1
            ) {
                throw MensajeInvalido(
                    "Se requiere exactamente un mensaje.");
            }

            const MensajeRPC solicitud =
                desdeXmlRpc(argumentos[0]);

            const MensajeRPC respuesta =
                servicio_.atender(solicitud);

            resultado = haciaXmlRpc(respuesta);

            std::cout
                << "id=" << solicitud.id()
                << " operacion=" << solicitud.operacion()
                << " codigo=" << respuesta.codigo()
                << std::endl;

        } catch (const MensajeInvalido& error) {

            // Error de formato externo: XML-RPC Fault.
            throw XmlRpc::XmlRpcException(
                error.what(), -32602);

        } catch (const std::exception& error) {

            std::cerr << "Error interno: "
                      << error.what() << std::endl;

            throw XmlRpc::XmlRpcException(
                "Error interno del servidor.", -32603);
        }
    }

private:
    const ServicioRobot& servicio_;
};

} // namespace

ServidorXmlRpc::ServidorXmlRpc(unsigned short puerto)
    : puerto_(puerto), robot_(), servicio_(robot_) {

    if (puerto == 0)
        throw ErrorServidor(
            "El puerto no puede ser cero.");
}

void ServidorXmlRpc::ejecutar() {

    ServidorLocal servidor;

    MetodoIntercambiar metodo(servidor, servicio_);

    servidor.enableIntrospection(true);
    XmlRpc::setVerbosity(0);

    // Evita redondear los doubles a seis decimales.
    XmlRpc::XmlRpcValue::setDoubleFormat("%.17g");

    servidor.abrir(puerto_);

    detener = 0;

    if (std::signal(SIGINT, solicitarCierre) == SIG_ERR ||
        std::signal(SIGTERM, solicitarCierre) == SIG_ERR)
        throw ErrorServidor("No se pudieron configurar las senales de cierre.");

#ifndef _WIN32
    // Linux puede generar SIGPIPE si un cliente cierra su conexion.
    if (std::signal(SIGPIPE, SIG_IGN) == SIG_ERR)
        throw ErrorServidor("No se pudo configurar SIGPIPE.");
#endif

    std::cout
        << "Servidor XML-RPC\n"
        << "Direccion: http://127.0.0.1:"
        << puerto_ << "/RPC2\n"
        << "Operaciones: ping, eco y control serie del robot. Cerrar con Ctrl+C."
        << std::endl;

    while (!detener)
        servidor.work(0.1);

    servidor.shutdown();

    std::cout << "Servidor cerrado." << std::endl;
}

} // namespace robotrpc
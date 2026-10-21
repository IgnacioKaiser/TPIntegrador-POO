#include "robotrpc/servidor.hpp"
#include "XmlRpc.h"
#include "XmlRpcSocket.h"
#include <arpa/inet.h>
#include <array>
#include <csignal>
#include <iostream>

namespace robotrpc {
namespace {
volatile std::sig_atomic_t detener = 0;
void solicitarCierre(int) { detener = 1; }

// XmlRpc++ escucha por defecto en todas las interfaces. Este adaptador
// conserva la biblioteca original y limita el prototipo a esta computadora.
class ServidorLocal final : public XmlRpc::XmlRpcServer {
public:
    void abrir(unsigned short puerto) {
        const int descriptor = XmlRpc::XmlRpcSocket::socket();
        if (descriptor < 0) throw ErrorServidor("No se pudo crear el socket.");
        setfd(descriptor);
        sockaddr_in direccion{};
        direccion.sin_family = AF_INET;
        direccion.sin_port = htons(puerto);
        direccion.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (!XmlRpc::XmlRpcSocket::setNonBlocking(descriptor) ||
            !XmlRpc::XmlRpcSocket::setReuseAddr(descriptor) ||
            ::bind(descriptor, reinterpret_cast<const sockaddr*>(&direccion), sizeof(direccion)) != 0 ||
            !XmlRpc::XmlRpcSocket::listen(descriptor, 16)) {
            const auto detalle = XmlRpc::XmlRpcSocket::getErrorMsg();
            close();
            throw ErrorServidor("No se pudo escuchar en 127.0.0.1:" +
                std::to_string(puerto) + ": " + detalle);
        }
        _disp.addSource(this, XmlRpc::XmlRpcDispatch::ReadableEvent);
    }
};

class MetodoIntercambiar final : public XmlRpc::XmlRpcServerMethod {
public:
    MetodoIntercambiar(XmlRpc::XmlRpcServer& servidor, const ServicioPrueba& servicio)
        : XmlRpcServerMethod("robot.intercambiar", &servidor), servicio_(servicio) {}

    std::string help() override { return "Recibe y devuelve un MensajeRPC version 1."; }

    void execute(XmlRpc::XmlRpcValue& parametros, XmlRpc::XmlRpcValue& resultado) override {
        try {
            using Valor = XmlRpc::XmlRpcValue;
            if (parametros.getType() != Valor::TypeArray || parametros.size() != 1)
                throw MensajeInvalido("Se requiere exactamente un mensaje.");
            auto& entrada = parametros[0];
            const std::array<const char*, 6> nombres{
                "version", "id", "tipo", "operacion", "contenido", "codigo"};
            if (entrada.getType() != Valor::TypeStruct || entrada.size() != 6)
                throw MensajeInvalido("El mensaje debe ser una estructura de seis campos.");
            CamposMensaje campos;
            for (const auto* nombre : nombres) {
                if (!entrada.hasMember(nombre) || entrada[nombre].getType() != Valor::TypeString)
                    throw MensajeInvalido("Falta un campo o no es string.");
                campos.emplace(nombre, static_cast<std::string&>(entrada[nombre]));
            }
            const auto solicitud = MensajeRPC::deserializar(campos);
            const auto respuesta = servicio_.atender(solicitud);
            resultado.clear();
            for (const auto& campo : respuesta.serializar())
                resultado[campo.first] = Valor(campo.second);
            std::cout << "id=" << solicitud.id() << " operacion=" << solicitud.operacion()
                      << " codigo=" << respuesta.codigo() << std::endl;
        } catch (const MensajeInvalido& error) {
            throw XmlRpc::XmlRpcException(error.what(), -32602);
        } catch (const std::exception& error) {
            std::cerr << "Error interno: " << error.what() << std::endl;
            throw XmlRpc::XmlRpcException("Error interno del servidor.", -32603);
        }
    }
private:
    const ServicioPrueba& servicio_;
};
}

ServidorXmlRpc::ServidorXmlRpc(unsigned short puerto) : puerto_(puerto) {
    if (puerto == 0) throw ErrorServidor("El puerto no puede ser cero.");
}

void ServidorXmlRpc::ejecutar() {
    ServidorLocal servidor;
    MetodoIntercambiar metodo(servidor, servicio_);
    servidor.enableIntrospection(true);
    XmlRpc::setVerbosity(0);
    servidor.abrir(puerto_);
    detener = 0;
    struct sigaction accion{};
    accion.sa_handler = solicitarCierre;
    sigemptyset(&accion.sa_mask);
    if (sigaction(SIGINT, &accion, nullptr) != 0 || sigaction(SIGTERM, &accion, nullptr) != 0)
        throw ErrorServidor("No se pudieron configurar las senales de cierre.");
    struct sigaction ignorar{};
    ignorar.sa_handler = SIG_IGN;
    sigemptyset(&ignorar.sa_mask);
    if (sigaction(SIGPIPE, &ignorar, nullptr) != 0)
        throw ErrorServidor("No se pudo configurar SIGPIPE.");
    std::cout << "Servidor v0.2: XmlRpc++ del profesor\n"
              << "Direccion: http://127.0.0.1:" << puerto_ << "/RPC2\n"
              << "Operaciones: ping, eco. Cerrar con Ctrl+C." << std::endl;
    // Multiplexa conexiones; los metodos se ejecutan de a uno.
    while (!detener) servidor.work(0.1);
    servidor.shutdown();
    std::cout << "Servidor cerrado." << std::endl;
}
} // namespace robotrpc

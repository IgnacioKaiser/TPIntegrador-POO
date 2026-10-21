#include "ServerModel.h"

#include "XmlRpcException.h"
#include "XmlRpcServer.h"
#include "XmlRpcValue.h"

#include <string>

namespace {
class RpcServiceException : public XmlRpc::XmlRpcException {
public:
    explicit RpcServiceException(const std::string& message)
        : XmlRpc::XmlRpcException(message) {}
};

void requireArray(const XmlRpc::XmlRpcValue& params, const std::string& method) {
    if (params.getType() != XmlRpc::XmlRpcValue::TypeArray) {
        throw RpcServiceException(method + " espera una lista de argumentos");
    }
}
}

ServerTestMethod::ServerTestMethod(XmlRpc::XmlRpcServer* server)
    : XmlRpcServerMethod("ServerTest", server) {}

void ServerTestMethod::execute(XmlRpc::XmlRpcValue& params,
                               XmlRpc::XmlRpcValue& result) {
    if (params.getType() != XmlRpc::XmlRpcValue::TypeInvalid) {
        requireArray(params, name());
    }
    if (params.getType() == XmlRpc::XmlRpcValue::TypeArray && params.size() != 0) {
        throw RpcServiceException("ServerTest no acepta argumentos");
    }

    result = "Servidor del integrador disponible";
}

std::string ServerTestMethod::help() {
    return "Verifica que el servidor este activo; no recibe argumentos";
}

EchoMethod::EchoMethod(XmlRpc::XmlRpcServer* server)
    : XmlRpcServerMethod("Eco", server) {}

void EchoMethod::execute(XmlRpc::XmlRpcValue& params,
                         XmlRpc::XmlRpcValue& result) {
    requireArray(params, name());
    if (params.size() != 1 ||
        params[0].getType() != XmlRpc::XmlRpcValue::TypeString) {
        throw RpcServiceException("Eco espera exactamente una cadena");
    }

    result = static_cast<std::string>(params[0]);
}

std::string EchoMethod::help() {
    return "Devuelve la cadena recibida";
}

ServerModel::ServerModel(XmlRpc::XmlRpcServer& server)
    : serverTest_(&server), echo_(&server) {}

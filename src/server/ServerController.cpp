#include "ServerController.h"

#include "ApplicationException.h"
#include "ServerModel.h"
#include "XmlRpc.h"

#include <cerrno>
#include <cctype>
#include <cstdlib>
#include <string>

namespace {
int parsePort(const std::string& value) {
    if (value.empty()) {
        throw ApplicationException("El puerto debe ser un entero entre 1 y 65535.");
    }

    for (std::string::const_iterator character = value.begin();
         character != value.end(); ++character) {
        if (!std::isdigit(static_cast<unsigned char>(*character))) {
            throw ApplicationException("El puerto debe ser un entero entre 1 y 65535.");
        }
    }

    errno = 0;
    char* end = 0;
    const long port = std::strtol(value.c_str(), &end, 10);
    if (errno == ERANGE || *end != '\0' || port < 1 || port > 65535) {
        throw ApplicationException("El puerto debe ser un entero entre 1 y 65535.");
    }
    return static_cast<int>(port);
}
}

int ServerController::run(int argc, char* argv[]) {
    if (argc != 2) {
        throw ApplicationException("Uso: servidor PUERTO");
    }

    const int port = parsePort(argv[1]);
    XmlRpc::XmlRpcServer server;
    ServerModel model(server);
    server.enableIntrospection(true);

    if (!server.bindAndListen(port)) {
        throw ApplicationException("No se pudo iniciar el servidor en el puerto " +
                                   std::to_string(port) + ".");
    }

    XmlRpc::setVerbosity(0);
    server.work(-1.0);
    return 0;
}

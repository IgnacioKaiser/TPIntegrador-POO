#include "robotrpc/servidor.hpp"
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    try {
        unsigned long puerto = 8080;
        if (argc == 2 && std::string(argv[1]) == "--help") {
            std::cout << "Uso: robot_server [--port 8080]\n";
            return 0;
        }
        if (argc != 1) {
            if (argc != 3 || std::string(argv[1]) != "--port")
                throw std::invalid_argument("Uso: robot_server [--port 8080]");
            const std::string argumento = argv[2];
            std::size_t leidos = 0;
            puerto = std::stoul(argumento, &leidos);
            if (leidos != argumento.size() || puerto < 1 || puerto > 65535)
                throw std::invalid_argument("El puerto debe estar entre 1 y 65535.");
        }
        robotrpc::ServidorXmlRpc servidor(static_cast<unsigned short>(puerto));
        servidor.ejecutar();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "No se pudo iniciar/continuar el servidor: " << error.what() << '\n';
        return 1;
    }
}

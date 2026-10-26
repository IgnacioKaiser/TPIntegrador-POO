#include "robotrpc/servidor.hpp"
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    try {
        unsigned long puerto = 8080;
        std::string ruta = "data/db/usuarios.sqlite";
        const char* uso = "Uso: robot_server [--port 8080] [--db ruta]";
        if (argc == 2 && std::string(argv[1]) == "--help") {
            std::cout << uso << '\n';
            return 0;
        }
        for (int i = 1; i < argc; i += 2) {
            if (i + 1 >= argc) throw std::invalid_argument(uso);
            const std::string opcion = argv[i];
            const std::string argumento = argv[i + 1];
            if (opcion == "--db") ruta = argumento;
            else if (opcion == "--port") {
                std::size_t leidos = 0;
                puerto = std::stoul(argumento, &leidos);
                if (leidos != argumento.size() || puerto < 1 || puerto > 65535)
                    throw std::invalid_argument("El puerto debe estar entre 1 y 65535.");
            } else throw std::invalid_argument(uso);
        }
        robotrpc::ServidorXmlRpc servidor(static_cast<unsigned short>(puerto), ruta);
        servidor.ejecutar();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "No se pudo iniciar/continuar el servidor: " << error.what() << '\n';
        return 1;
    }
}

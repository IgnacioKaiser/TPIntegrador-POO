#include "ApplicationException.h"
#include "ServerController.h"

#include <exception>
#include <iostream>

int main(int argc, char* argv[]) {
    try {
        ServerController controller;
        return controller.run(argc, argv);
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}

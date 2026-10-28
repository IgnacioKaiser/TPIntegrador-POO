// Ejecutable exclusivo de pruebas. Siempre inyecta un canal sustituido; no abre serie.
#include "robotrpc/servidor.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>

class CanalPrueba final : public CanalSerie {
public:
    CanalPrueba(std::filesystem::path carpeta,std::string caso) : carpeta_(std::move(carpeta)),caso_(std::move(caso)) {}
    bool conectar(const std::string&) override { conectado_=true; return true; }
    void desconectar() noexcept override { conectado_=false; }
    bool estaConectado() const noexcept override { return conectado_; }
    std::string intercambiar(const std::string& comando,unsigned int) override {
        // La traza solo existe en el directorio temporal de la prueba.
        { std::ofstream traza(carpeta_/"trafico.txt",std::ios::app); traza<<comando<<'\n'; }
        if(comando=="M3\r" || comando=="M5\r") {
            ++orden_;
            if(caso_=="bloqueada" && orden_==1) {
                { std::ofstream marca(carpeta_/"bloqueada"); marca<<"esperando"; }
                const auto limite=std::chrono::steady_clock::now()+std::chrono::seconds(15);
                while(!std::filesystem::exists(carpeta_/"liberar")) {
                    if(std::chrono::steady_clock::now()>=limite) throw ErrorSerie("No se libero el sustituto.");
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                }
            }
            if((caso_=="error2" && orden_==2) || (caso_=="error3" && orden_==3))
                return "ERROR: fallo programado\nOK\n";
            if(caso_=="timeout3" && orden_==3) { conectado_=false; throw ErrorSerie("Timeout programado"); }
        }
        if(comando=="M17\r") motores_=true;
        if(comando=="M18\r") motores_=false;
        if(comando=="G1 X1.000000 Y170.000000 Z120.000000 F5.000000\r") movido_=true;
        if(comando=="G28\r") { movido_=false; return "INFO: HOMING COMPLETE\nOK\n"; }
        if(comando=="M114\r")
            return std::string("INFO: CURRENT POSITION: [X:")+(movido_?"1":"0")+
                " Y:170 Z:120 E:0]\nINFO: MOTORS "+(motores_?"ENABLED":"DISABLED")+"\nOK\n";
        return "OK\n";
    }
private:
    std::filesystem::path carpeta_;
    std::string caso_;
    bool conectado_=false,motores_=false,movido_=false;
    int orden_=0;
};

int main(int argc,char** argv) {
    if(argc!=5) return 1;
    try {
        const auto carpeta=std::filesystem::path(argv[2]);
        CanalPrueba canal(carpeta,argv[4]);
        robotrpc::ServidorXmlRpc servidor(static_cast<unsigned short>(std::stoi(argv[3])),argv[1],
                                          (carpeta/"tareas").string(),&canal);
        servidor.ejecutar();
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}

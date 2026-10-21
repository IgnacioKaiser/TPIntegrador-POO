/* server.cpp : codigo base para el TP sobre servidor XMLRPC
*/

#include "XmlRpc.h"
using namespace XmlRpc;

#include <iostream>
#include <stdlib.h>
using namespace std;

// Sin argumentos, el resultado es "Hi, soy el servidor RPC !!".
class ServerTest : public XmlRpcServerMethod
{
public:
  ServerTest(XmlRpcServer* S) : XmlRpcServerMethod("ServerTest", S) {}

  void execute(XmlRpcValue& params, XmlRpcValue& result)
  {
    result = "Hi, soy el servidor RPC !!";
  }

  std::string help() { return std::string("Respondo quien soy cuando no hay argumentos"); }
};


// Con un argumento, el resultado es "Hola, " + argumento + argumento
class Eco : public XmlRpcServerMethod
{
public:
  Eco(XmlRpcServer* S) : XmlRpcServerMethod("Eco", S) {}

  void execute(XmlRpcValue& params, XmlRpcValue& result)
  {
    std::string resultString = "Hola, ";
    resultString += std::string(params[0]);
    resultString += std::string(" ");
    resultString += std::string(params[0]);
    result = resultString;
  }

  std::string help() { return std::string("Diga algo y recibira un saludo"); }
};


// Con un numero variable de argumentos, todos dobles, el resultado es la suma
class Sumar : public XmlRpcServerMethod
{
public:
  Sumar(XmlRpcServer* S) : XmlRpcServerMethod("Sumar", S) {}

  void execute(XmlRpcValue& params, XmlRpcValue& result)
  {
    int nArgs = params.size();
    double sum = 0.0;
    for (int i=0; i<nArgs; ++i)
      sum += double(params[i]);
    result = sum;
  }

  std::string help() { return std::string("Indique varios numeros reales separados por espacio"); }
};

int main(int argc, char* argv[])
{
  if (argc != 2) {
    std::cerr << "Uso: miserver N_Port\n";
    return 1;
  }
  int port = atoi(argv[1]);

  // S es el servidor
  XmlRpcServer S;

  // Registro de metodos en el servidor
  // mediante el uso del constructor heredado.
  // Cada clase modela un metodo, implementado en execute().
  // Cada clase admite una ayuda, en help().
  ServerTest serverTest(&S);
  Eco eco(&S);
  Sumar sumar(&S);

  XmlRpc::setVerbosity(5);

  // Se crea un socket de servidor sobre el puerto indicado
  S.bindAndListen(port);

  // Enable introspection
  S.enableIntrospection(true);

  // A la escucha de requerimientos
  S.work(-1.0);

  return 0;
}

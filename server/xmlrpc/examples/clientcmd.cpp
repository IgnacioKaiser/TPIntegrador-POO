/* clientcmd.cpp : codigo fuente para TP de un cliente XMLRPC de prueba basica.
   Uso: clientcmd Host Port [otros_argumentos]
*/
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <cctype>
using namespace std;

#include "XmlRpc.h"
using namespace XmlRpc;

// Se puede recibir por linea de argumentos una cadena vacia,
// una cadena alfanumerica, un conjunto de numeros reales
// 2 cadenas siendo la primera la palabra help


// Verifica si la cadena es un entero valido
bool esEntero(const string& s) {
    if (s.empty()) return false;

    size_t i = 0;
    if (s[0] == '-' || s[0] == '+') i = 1; // permitir signo

    if (i == s.size()) return false; // solo un signo no es válido

    for (; i < s.size(); ++i) {
        if (!isdigit(static_cast<unsigned char>(s[i]))) return false;
    }
    return true;
}

// Verifica si la cadena es una dirección IPv4 válida
bool esIP(const string& s) {
    stringstream ss(s);
    string segmento;
    int count = 0;

    while (getline(ss, segmento, '.')) {
        if (!esEntero(segmento)) return false;

        int valor = stoi(segmento);
        if (valor < 0 || valor > 255) return false;

        count++;
    }

    return (count == 4);
}

// Verificar si es alfanumerica (sin espacios, solo letras o digitos)
bool esAlfanumerica(const string& s) {
    if (s.empty()) return false;
    for (char c : s) {
        if (!isalnum(static_cast<unsigned char>(c))) return false;
    }
    return true;
}

// Verificar si una cadena es un numero real
bool esReal(const string& s) {
    try {
        size_t pos;
        stod(s, &pos);       // convierte a double
        return pos == s.size(); // true si se consumio toda la cadena
    }
    catch (...) {
        return false; // excepcion => no es un numero valido
    }
}

// Verificar si todos los tokens son numeros reales
bool sonNumerosReales(const vector<string>& tokens, XmlRpcValue& numbers) {
  for (size_t i = 3; i < tokens.size(); ++i) {
      if(!esReal(tokens[i])) return false;
      numbers[i-3]=stod(tokens[i]);
  }
  return true;
}

int main(int argc, char* argv[])
{
  std::vector<std::string> args(argv, argv + argc);
  int port;
  const char* ip; // string

  if (argc < 3) {
    std::cerr << "Modo de Uso: clientcmd25 IP_HOST N_PORT otros_argumentos\n";
    std::cerr << "o bien: clientcmd25 IP_HOST N_PORT help nombre_servicio\n";
    return -1;
  }

  for (size_t i = 0; i < args.size(); ++i) {
    std::cout << "Arg " << i << ": " << args[i] << "\n";
  }

  if(esIP(args[1])){
    ip = args[1].c_str();
  }
  else {
    std::cerr << "No corresponde a un IP valido\n";
    return -1;
  }

  if(esEntero(args[2])){
    port = atoi(args[2].c_str());
  }
  else {
    std::cerr << "El port debe ser un entero\n";
    return -1;
  }

  XmlRpcClient c(ip, port);

  XmlRpcValue noArgs, result;
  XmlRpcValue oneArg;
  XmlRpcValue numbers;

  if(args.size()==3){
        cout << "Caso 1: Cadena vacia\n";
        std::cout << "********************** Llamada al metodo ServerTest **********************" << std::endl ;
        if (c.execute("ServerTest", noArgs, result))
          std::cout << result << "\n\n";
        else
          std::cout << "Error en la llamada a 'ServerTest'\n\n";

        return 0;
  }

  if(args.size()==4 && esAlfanumerica(args[3])) {
        cout << "Caso 2: Una cadena alfanumerica sin espacios '" << args[3] << "'\n";
        std::cout << "******************* Llamada al metodo Eco *******************" << std::endl ;
        oneArg[0] = args[3];
        if (c.execute("Eco", oneArg, result))
          std::cout << result << "\n\n";
        else
          std::cout << "Error en la llamada a 'Eco'\n\n";

        return 0;
    }

    if (args.size()>=4 && sonNumerosReales(args, numbers)) {
        cout << "Caso 3: Conjunto de números reales [";
        for (size_t i = 3; i < args.size(); ++i) {
            cout << args[i] << (i + 1 < args.size() ? ", " : "");
        }
        cout << "]\n";
        std::cout << "valor 0 is " << numbers[0] << std::endl;
        std::cout << "valor 1 is " << numbers[1] << std::endl;
        std::cout << "****************** Llamada al metodo Sumar *******************" << std::endl ;


        if (c.execute("Sumar", numbers, result))
          std::cout << "Suma = " << double(result) << "\n\n";
        else
          std::cout << "Error en la llamada a 'Sumar'\n\n";

        return 0;
    }

    if (args.size()==5 && args[3] == "help") {
        cout << "Caso 4: Solicitud de Help para el servicio '" << args[4] << "'\n";
      std::cout << "****** Llamada a help ******" << std::endl ;
        oneArg[0] = args[4];
        if (c.execute("system.methodHelp", oneArg, result))
          std::cout << "Ayuda para el metodo '" << args[4] << "':\n" << result << "\n\n";
        else
          std::cout << "Error en la llamada a 'methodHelp'\n\n";

        return 0;
    }

    // Ningún caso coincide
    cout << "Entrada no reconocida.\n";
    return 0;
}

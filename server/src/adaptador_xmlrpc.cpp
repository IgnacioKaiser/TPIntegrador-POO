#include "robotrpc/adaptador_xmlrpc.hpp"

#include <cstdint>
#include <string>
#include <variant>

namespace robotrpc {
namespace {

using Xml = XmlRpc::XmlRpcValue;

// La version de XmlRpc++ incluida en el TP no publica un iterador
// de miembros de struct. Esta vista usa su interfaz protegida,
// sin modificar el codigo de la biblioteca ni el modelo de dominio.
class VistaEstructura final : public Xml {
public:
    explicit VistaEstructura(const Xml& valor) : Xml(valor) {}

    const ValueStruct& miembros() const {
        if (getType() != TypeStruct)
            throw MensajeInvalido("Se esperaba un struct XML-RPC.");
        return *_value.asStruct;
    }
};

// Una instancia XmlRpcValue() es INVALID, no un struct vacio.
// Usamos el parser de la propia biblioteca para crear ese valor.
Xml estructuraVacia() {
    const std::string xml = "<value><struct></struct></value>";
    int posicion = 0;
    Xml valor;

    if (!valor.fromXml(xml, &posicion) ||
        valor.getType() != Xml::TypeStruct)
        throw std::runtime_error(
            "No se pudo crear el struct XML-RPC vacio.");

    return valor;
}

ValorRPC aDominio(const Xml& valor, unsigned profundidad) {
    if (profundidad > 32)
        throw MensajeInvalido(
            "Estructura XML-RPC demasiado profunda.");

    switch (valor.getType()) {
        case Xml::TypeString: {
            Xml copia(valor);
            return ValorRPC(
                static_cast<std::string&>(copia));
        }

        case Xml::TypeInt: {
            Xml copia(valor);
            return ValorRPC(
                static_cast<std::int32_t>(
                    static_cast<int&>(copia)));
        }

        case Xml::TypeDouble: {
            Xml copia(valor);
            return ValorRPC(
                static_cast<double&>(copia));
        }

        case Xml::TypeBoolean: {
            Xml copia(valor);
            return ValorRPC(
                static_cast<bool&>(copia));
        }

        case Xml::TypeArray: {
            ValorRPC::Arreglo elementos;

            for (int i = 0; i < valor.size(); ++i)
                elementos.push_back(
                    aDominio(valor[i], profundidad + 1));

            return ValorRPC(std::move(elementos));
        }

        case Xml::TypeStruct: {
            VistaEstructura vista(valor);
            EstructuraRPC campos;

            for (const auto& campo : vista.miembros())
                campos.emplace(
                    campo.first,
                    aDominio(campo.second, profundidad + 1));

            return ValorRPC(std::move(campos));
        }

        default:
            throw MensajeInvalido(
                "Tipo XML-RPC no admitido por el contrato.");
    }
}

Xml aXmlRpc(const ValorRPC& valor, unsigned profundidad) {
    if (profundidad > 32)
        throw MensajeInvalido(
            "Estructura demasiado profunda.");

    const auto& dato = valor.dato();

    if (const auto* texto = std::get_if<std::string>(&dato))
        return Xml(*texto);

    if (const auto* entero = std::get_if<std::int32_t>(&dato))
        return Xml(static_cast<int>(*entero));

    if (const auto* real = std::get_if<double>(&dato))
        return Xml(*real);

    if (const auto* booleano = std::get_if<bool>(&dato))
        return Xml(*booleano);

    if (const auto* arreglo =
            std::get_if<ValorRPC::Arreglo>(&dato)) {

        Xml salida;
        salida.setSize(static_cast<int>(arreglo->size()));

        for (std::size_t i = 0; i < arreglo->size(); ++i)
            salida[static_cast<int>(i)] =
                aXmlRpc((*arreglo)[i], profundidad + 1);

        return salida;
    }

    const auto& campos = std::get<EstructuraRPC>(dato);

    Xml salida = estructuraVacia();

    for (const auto& campo : campos)
        salida[campo.first] =
            aXmlRpc(campo.second, profundidad + 1);

    return salida;
}

} // namespace

MensajeRPC desdeXmlRpc(const Xml& valor) {
    const ValorRPC neutro = aDominio(valor, 0);

    const auto* campos =
        std::get_if<EstructuraRPC>(&neutro.dato());

    if (!campos)
        throw MensajeInvalido(
            "El mensaje debe ser un struct XML-RPC.");

    return MensajeRPC::deserializar(*campos);
}

Xml haciaXmlRpc(const MensajeRPC& mensaje) {
    return aXmlRpc(
        ValorRPC(mensaje.serializar()), 0);
}

} // namespace robotrpc
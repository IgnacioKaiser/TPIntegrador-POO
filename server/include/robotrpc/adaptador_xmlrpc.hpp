#pragma once

#include "robotrpc/mensaje.hpp"
#include "XmlRpcValue.h"

namespace robotrpc {

// Unico punto de conversion entre la biblioteca XML-RPC y el modelo neutral.
MensajeRPC desdeXmlRpc(const XmlRpc::XmlRpcValue& valor);

XmlRpc::XmlRpcValue haciaXmlRpc(const MensajeRPC& mensaje);

} // namespace robotrpc
#ifndef SERVER_MODEL_H
#define SERVER_MODEL_H

#include "XmlRpcServerMethod.h"

#include <string>

namespace XmlRpc {
class XmlRpcServer;
class XmlRpcValue;
}

class ServerTestMethod : public XmlRpc::XmlRpcServerMethod {
public:
    explicit ServerTestMethod(XmlRpc::XmlRpcServer* server);
    void execute(XmlRpc::XmlRpcValue& params, XmlRpc::XmlRpcValue& result);
    std::string help();
};

class EchoMethod : public XmlRpc::XmlRpcServerMethod {
public:
    explicit EchoMethod(XmlRpc::XmlRpcServer* server);
    void execute(XmlRpc::XmlRpcValue& params, XmlRpc::XmlRpcValue& result);
    std::string help();
};

class ServerModel {
public:
    explicit ServerModel(XmlRpc::XmlRpcServer& server);

private:
    ServerTestMethod serverTest_;
    EchoMethod echo_;
};

#endif

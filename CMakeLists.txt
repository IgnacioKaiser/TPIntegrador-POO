cmake_minimum_required(VERSION 3.10)
project(TPIntegradorServer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(XMLRPC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/lib/xmlrpc")
set(XMLRPC_SOURCES
    "${XMLRPC_DIR}/XmlRpcClient.cpp"
    "${XMLRPC_DIR}/XmlRpcDispatch.cpp"
    "${XMLRPC_DIR}/XmlRpcServer.cpp"
    "${XMLRPC_DIR}/XmlRpcServerConnection.cpp"
    "${XMLRPC_DIR}/XmlRpcServerMethod.cpp"
    "${XMLRPC_DIR}/XmlRpcSocket.cpp"
    "${XMLRPC_DIR}/XmlRpcSource.cpp"
    "${XMLRPC_DIR}/XmlRpcUtil.cpp"
    "${XMLRPC_DIR}/XmlRpcValue.cpp")

add_library(xmlrpc STATIC ${XMLRPC_SOURCES})
target_include_directories(xmlrpc PUBLIC "${XMLRPC_DIR}")
if(WIN32)
    target_compile_definitions(xmlrpc PRIVATE _WINDOWS)
    target_link_libraries(xmlrpc PUBLIC ws2_32)
endif()

add_executable(servidor
    src/server/main.cpp
    src/server/ServerController.cpp
    src/server/ServerModel.cpp)
target_include_directories(servidor PRIVATE inc)
target_link_libraries(servidor PRIVATE xmlrpc)

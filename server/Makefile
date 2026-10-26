CXX ?= g++
AR ?= ar
CXXFLAGS ?= -std=c++17 -O2 -g -Wall -Wextra -Wpedantic
CPPFLAGS += -Iinclude -Ixmlrpc -Iinclude/tp2_logger

ifeq ($(OS),Windows_NT)
SHELL := cmd.exe
.SHELLFLAGS := /C
PYTHON ?= python
EXE := .exe
ejecutar_prueba = .\$(subst /,\,$(DOMAIN_TEST))
CPPFLAGS += -D_WINDOWS -DNOMINMAX
LDLIBS += -lws2_32
crear_directorio = if not exist "$(subst /,\,$(dir $@))" mkdir "$(subst /,\,$(dir $@))"
limpiar = if exist build rmdir /S /Q build
prueba_python = set "PYTHONPATH=../client" && $(PYTHON)
else
PYTHON ?= python3
EXE :=
ejecutar_prueba = ./$(DOMAIN_TEST)
crear_directorio = mkdir -p "$(dir $@)"
limpiar = rm -rf build
prueba_python = PYTHONPATH=../client $(PYTHON)
endif

RPC_SOURCES := $(wildcard xmlrpc/XmlRpc*.cpp)
LOG_SOURCES := $(wildcard src/tp2_logger/*.cpp)
APP_SOURCES := $(wildcard src/*.cpp)
USER_OBJECTS := build/src/usuarios/repositorio_usuarios.o
USER_LIBS := -lsqlite3 -lsodium
USER_TOOL := build/usuarios_admin$(EXE)
USER_TEST := build/test_usuarios$(EXE)
RPC_OBJECTS := $(patsubst %.cpp,build/%.o,$(RPC_SOURCES))
LOG_OBJECTS := $(patsubst %.cpp,build/%.o,$(LOG_SOURCES))
APP_OBJECTS := $(patsubst %.cpp,build/%.o,$(APP_SOURCES))
DOMAIN_OBJECTS := build/src/mensaje.o build/src/servicio.o
RPC_LIB := build/libxmlrpcpp.a
LOG_LIB := build/libtp2_logger.a
SERVER := build/robot_server$(EXE)
DOMAIN_TEST := build/test_domain$(EXE)
DEPS := $(RPC_OBJECTS:.o=.d) $(LOG_OBJECTS:.o=.d) $(APP_OBJECTS:.o=.d) build/tests/test_domain.d

.PHONY: all libraries examples test test-unit test-usuarios test-autenticacion usuarios clean
all: $(SERVER) libraries $(USER_TOOL)
libraries: $(RPC_LIB) $(LOG_LIB)

$(RPC_LIB): $(RPC_OBJECTS)
	$(AR) rcs $@ $^
$(LOG_LIB): $(LOG_OBJECTS)
	$(AR) rcs $@ $^
$(SERVER): $(APP_OBJECTS) $(RPC_LIB) $(USER_OBJECTS) build/src/usuarios/autenticador.o build/src/usuarios/servicio_protegido.o
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS) $(LDLIBS) $(USER_LIBS)
build/%.o: %.cpp
	$(crear_directorio)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@
$(DOMAIN_TEST): build/tests/test_domain.o $(DOMAIN_OBJECTS)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS) $(LDLIBS)

examples: build/examples/server$(EXE) build/examples/client$(EXE) build/examples/clientcmd$(EXE)
build/examples/%$(EXE): xmlrpc/examples/%.cpp $(RPC_LIB)
	$(crear_directorio)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -o $@ $(LDFLAGS) $(LDLIBS)

test-unit: $(DOMAIN_TEST)
	$(ejecutar_prueba)
	$(prueba_python) -m unittest discover -s ../client/tests -v
test: all test-unit test-usuarios test-autenticacion
	$(prueba_python) -m unittest discover -s tests -p "test_integration.py" -v
usuarios: $(USER_TOOL)
$(USER_TOOL): build/tools/usuarios_admin.o $(USER_OBJECTS)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS) $(USER_LIBS)
$(USER_TEST): build/tests/test_usuarios.o $(USER_OBJECTS)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS) $(USER_LIBS)
test-usuarios: $(USER_TOOL) $(USER_TEST)
	$(PYTHON) tests/ejecutar_usuarios.py
build/test_autenticacion$(EXE): build/tests/test_autenticacion.o $(USER_OBJECTS) build/src/usuarios/autenticador.o build/src/usuarios/servicio_protegido.o $(DOMAIN_OBJECTS)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS) $(USER_LIBS)
test-autenticacion: build/test_autenticacion$(EXE)
	$(PYTHON) tests/ejecutar_autenticacion.py
clean:
	$(limpiar)
-include build/src/usuarios/autenticador.d build/src/usuarios/servicio_protegido.d build/tests/test_autenticacion.d $(DEPS) $(USER_OBJECTS:.o=.d) build/tools/usuarios_admin.d build/tests/test_usuarios.d

CXX ?= g++
AR ?= ar
CXXFLAGS ?= -std=c++17 -O2 -g -Wall -Wextra -Wpedantic
CPPFLAGS += -Iinclude -Ixmlrpc -Iinclude/tp2_logger

ifeq ($(OS),Windows_NT)
EXE := .exe
CPPFLAGS += -D_WINDOWS -DNOMINMAX
LDLIBS += -lws2_32
ifneq ($(MSYSTEM),)
SHELL := bash
.SHELLFLAGS := -c
ejecutar_prueba = ./$(DOMAIN_TEST)
argumento_ruta = "$1"
crear_directorio = mkdir -p "$(dir $@)"
limpiar = rm -rf build
PYTHON ?= python
prueba_python = PYTHONPATH=../client $(PYTHON)
else
SHELL := cmd.exe
.SHELLFLAGS := /C
PYTHON ?= python
ejecutar_prueba = .\$(subst /,\,$(DOMAIN_TEST))
argumento_ruta = "$(subst /,\,$1)"
crear_directorio = if not exist "$(subst /,\,$(dir $@))" mkdir "$(subst /,\,$(dir $@))"
limpiar = if exist build rmdir /S /Q build
prueba_python = set "PYTHONPATH=..\client" && $(PYTHON)
endif
else
PYTHON ?= python3
EXE :=
ejecutar_prueba = ./$(DOMAIN_TEST)
crear_directorio = mkdir -p "$(dir $@)"
limpiar = rm -rf build
prueba_python = PYTHONPATH=../client $(PYTHON)
argumento_ruta = "$1"
endif

RPC_SOURCES := $(wildcard xmlrpc/XmlRpc*.cpp)
LOG_SOURCES := $(wildcard src/tp2_logger/*.cpp)
USER_OBJECTS := build/src/usuarios/repositorio_usuarios.o
USER_LIBS := -lsqlite3 -lsodium
USER_TOOL := build/usuarios_admin$(EXE)
USER_TEST := build/test_usuarios$(EXE)
TAREAS_OBJECTS := build/src/tareas/repositorio_tareas.o
TAREAS_TEST := build/test_tareas$(EXE)
APP_SOURCES := $(wildcard src/*.cpp) $(wildcard src/domain/*.cpp)
RPC_OBJECTS := $(patsubst %.cpp,build/%.o,$(RPC_SOURCES))
LOG_OBJECTS := $(patsubst %.cpp,build/%.o,$(LOG_SOURCES))
APP_OBJECTS := $(patsubst %.cpp,build/%.o,$(APP_SOURCES))
CONTROL_OBJECTS := build/src/domain/Robot.o build/src/domain/CanalSerie.o build/src/domain/ProgramaTarea.o
DOMAIN_OBJECTS := build/src/mensaje.o build/src/servicio.o $(CONTROL_OBJECTS)
RPC_LIB := build/libxmlrpcpp.a
LOG_LIB := build/libtp2_logger.a
SERVER := build/robot_server$(EXE)
DOMAIN_TEST := build/test_domain$(EXE)
USER_DEPS := build/src/usuarios/autenticador.d build/src/usuarios/servicio_protegido.d \
	build/tests/test_autenticacion.d $(USER_OBJECTS:.o=.d) \
	build/tools/usuarios_admin.d build/tests/test_usuarios.d
DEPS := $(RPC_OBJECTS:.o=.d) $(LOG_OBJECTS:.o=.d) $(APP_OBJECTS:.o=.d) \
	build/tests/test_domain.d $(USER_DEPS) $(TAREAS_OBJECTS:.o=.d) build/tests/test_tareas.d \
	build/tests/test_ejecucion_tareas.d build/tests/test_registro_actividad.d

.PHONY: all libraries examples test test-unit test-usuarios test-autenticacion test-tareas test-registro test-ejecucion usuarios clean
all: $(SERVER) libraries $(USER_TOOL)
libraries: $(RPC_LIB) $(LOG_LIB)

$(RPC_LIB): $(RPC_OBJECTS)
	$(AR) rcs $(call argumento_ruta,$@) $(foreach archivo,$^,$(call argumento_ruta,$(archivo)))
$(LOG_LIB): $(LOG_OBJECTS)
	$(AR) rcs $(call argumento_ruta,$@) $(foreach archivo,$^,$(call argumento_ruta,$(archivo)))
$(SERVER): $(APP_OBJECTS) $(RPC_LIB) $(USER_OBJECTS) $(TAREAS_OBJECTS) build/src/usuarios/autenticador.o build/src/usuarios/servicio_protegido.o
	$(CXX) $(CXXFLAGS) $(foreach archivo,$^,$(call argumento_ruta,$(archivo))) -o $(call argumento_ruta,$@) $(LDFLAGS) $(LDLIBS) $(USER_LIBS)
build/%.o: %.cpp
	$(crear_directorio)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -MT "$@" -c "$<" -o "$@"
$(DOMAIN_TEST): build/tests/test_domain.o $(DOMAIN_OBJECTS)
	$(CXX) $(CXXFLAGS) $(foreach archivo,$^,$(call argumento_ruta,$(archivo))) -o $(call argumento_ruta,$@) $(LDFLAGS) $(LDLIBS)

examples: build/examples/server$(EXE) build/examples/client$(EXE) build/examples/clientcmd$(EXE)
build/examples/%$(EXE): xmlrpc/examples/%.cpp $(RPC_LIB)
	$(crear_directorio)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(foreach archivo,$^,$(call argumento_ruta,$(archivo))) -o $(call argumento_ruta,$@) $(LDFLAGS) $(LDLIBS)

test-unit: $(DOMAIN_TEST)
	$(ejecutar_prueba)
	$(prueba_python) -m unittest discover -s $(call argumento_ruta,../client/tests) -v
test: all test-unit test-usuarios test-autenticacion test-tareas test-registro test-ejecucion
	$(prueba_python) -m unittest discover -s tests -p "test_integration.py" -v
usuarios: $(USER_TOOL)
$(USER_TOOL): build/tools/usuarios_admin.o $(USER_OBJECTS)
	$(CXX) $(CXXFLAGS) $(foreach archivo,$^,$(call argumento_ruta,$(archivo))) -o $(call argumento_ruta,$@) $(LDFLAGS) $(USER_LIBS)
$(USER_TEST): build/tests/test_usuarios.o $(USER_OBJECTS)
	$(CXX) $(CXXFLAGS) $(foreach archivo,$^,$(call argumento_ruta,$(archivo))) -o $(call argumento_ruta,$@) $(LDFLAGS) $(USER_LIBS)
test-usuarios: $(USER_TOOL) $(USER_TEST)
	$(prueba_python) $(call argumento_ruta,tests/ejecutar_usuarios.py)
build/test_autenticacion$(EXE): build/tests/test_autenticacion.o $(USER_OBJECTS) $(TAREAS_OBJECTS) build/src/registro_actividad.o build/src/usuarios/autenticador.o build/src/usuarios/servicio_protegido.o $(DOMAIN_OBJECTS)
	$(CXX) $(CXXFLAGS) $(foreach archivo,$^,$(call argumento_ruta,$(archivo))) -o $(call argumento_ruta,$@) $(LDFLAGS) $(USER_LIBS)
test-autenticacion: build/test_autenticacion$(EXE)
	$(prueba_python) $(call argumento_ruta,tests/ejecutar_autenticacion.py)
$(TAREAS_TEST): build/tests/test_tareas.o $(TAREAS_OBJECTS)
	$(CXX) $(CXXFLAGS) $(foreach archivo,$^,$(call argumento_ruta,$(archivo))) -o $(call argumento_ruta,$@) $(LDFLAGS)
test-tareas: $(TAREAS_TEST)
	$(prueba_python) $(call argumento_ruta,tests/ejecutar_tareas.py)
REGISTRO_TEST := build/test_registro_actividad$(EXE)
$(REGISTRO_TEST): build/tests/test_registro_actividad.o build/src/registro_actividad.o
	$(CXX) $(CXXFLAGS) $(foreach archivo,$^,$(call argumento_ruta,$(archivo))) -o $(call argumento_ruta,$@) $(LDFLAGS)
test-registro: $(REGISTRO_TEST)
	$(if $(filter Windows_NT,$(OS)),$(if $(MSYSTEM),./$(REGISTRO_TEST),.\$(subst /,\,$(REGISTRO_TEST))),./$(REGISTRO_TEST))

EJECUCION_TEST := build/test_ejecucion_tareas$(EXE)
$(EJECUCION_TEST): build/tests/test_ejecucion_tareas.o $(TAREAS_OBJECTS) $(CONTROL_OBJECTS)
	$(CXX) $(CXXFLAGS) $(foreach archivo,$^,$(call argumento_ruta,$(archivo))) -o $(call argumento_ruta,$@) $(LDFLAGS)
test-ejecucion: $(EJECUCION_TEST)
	$(prueba_python) $(call argumento_ruta,tests/ejecutar_ejecucion_tareas.py)
clean:
	$(limpiar)
-include $(DEPS)

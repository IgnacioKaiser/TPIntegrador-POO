CXX ?= g++
AR ?= ar
PYTHON ?= python3
CXXFLAGS ?= -std=c++17 -O2 -g -Wall -Wextra -Wpedantic
CPPFLAGS += -Iinclude -Ixmlrpc -Iinclude/tp2_logger
RPC_SOURCES := $(wildcard xmlrpc/XmlRpc*.cpp)
LOG_SOURCES := $(wildcard src/tp2_logger/*.cpp)
APP_SOURCES := $(wildcard src/*.cpp)
RPC_OBJECTS := $(patsubst %.cpp,build/%.o,$(RPC_SOURCES))
LOG_OBJECTS := $(patsubst %.cpp,build/%.o,$(LOG_SOURCES))
APP_OBJECTS := $(patsubst %.cpp,build/%.o,$(APP_SOURCES))
DOMAIN_OBJECTS := build/src/mensaje.o build/src/servicio.o
RPC_LIB := build/libxmlrpcpp.a
LOG_LIB := build/libtp2_logger.a
DEPS := $(RPC_OBJECTS:.o=.d) $(LOG_OBJECTS:.o=.d) $(APP_OBJECTS:.o=.d) build/tests/test_domain.d

.PHONY: all libraries examples test test-unit clean
all: build/robot_server libraries
libraries: $(RPC_LIB) $(LOG_LIB)

$(RPC_LIB): $(RPC_OBJECTS)
	$(AR) rcs $@ $^
$(LOG_LIB): $(LOG_OBJECTS)
	$(AR) rcs $@ $^
build/robot_server: $(APP_OBJECTS) $(RPC_LIB)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS) $(LDLIBS)
build/%.o: %.cpp
	mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@
build/test_domain: build/tests/test_domain.o $(DOMAIN_OBJECTS)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS) $(LDLIBS)

examples: build/examples/server build/examples/client build/examples/clientcmd
build/examples/%: xmlrpc/examples/%.cpp $(RPC_LIB)
	mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -o $@ $(LDFLAGS) $(LDLIBS)

test-unit: build/test_domain
	./build/test_domain
	PYTHONPATH=../client $(PYTHON) -m unittest discover -s ../client/tests -v
test: all test-unit
	PYTHONPATH=../client $(PYTHON) -m unittest discover -s tests -p 'test_integration.py' -v
clean:
	rm -rf build
-include $(DEPS)

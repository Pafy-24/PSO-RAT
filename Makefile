CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra
LDFLAGS = -lsfml-network -lsfml-system

SERVER_SRC = RAT-Server/src/*.cpp
CLIENT_SRC = RAT-Client/src/*.cpp

all: build_make/rat_server build_make/rat_client

build_make/libutils.so:
	$(MAKE) -C Utils

build_make/rat_server: $(SERVER_SRC) build_make/libutils.so
	@mkdir -p build_make
	$(CXX) $(CXXFLAGS) -IUtils/include -IRAT-Server/include -o $@ $(SERVER_SRC) -Lbuild_make -lutils $(LDFLAGS) -Wl,-rpath,'$$ORIGIN'

build_make/rat_client: $(CLIENT_SRC) build_make/libutils.so
	@mkdir -p build_make
	$(CXX) $(CXXFLAGS) -IUtils/include -IRAT-Client/include -o $@ $(CLIENT_SRC) -Lbuild_make -lutils $(LDFLAGS) -Wl,-rpath,'$$ORIGIN'

clean:
	$(MAKE) -C Utils clean || true
	rm -rf build_make

.PHONY: all clean

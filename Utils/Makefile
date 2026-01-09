CXX = g++
CXXFLAGS = -std=c++17 -O2 -fPIC -Iinclude -Wall -Wextra
LDFLAGS = -shared -lsfml-network -lsfml-system

OBJ_DIR = ../build_make/obj_utils
OUT = ../build_make/libutils.so

SRCS = src/TCPSocket.cpp src/UDPSocket.cpp
OBJS = $(OBJ_DIR)/TCPSocket.o $(OBJ_DIR)/UDPSocket.o

all: $(OUT)

$(OUT): $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^

$(OBJ_DIR)/%.o: src/%.cpp
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR) $(OUT)

.PHONY: all clean

CC := g++
CFLAGS := -std=c++17 -O2 -Wall -Wextra
INCLUDES := -Iinclude -I../Utils/include
SRC := src/main.cpp src/ServerController.cpp src/ServerManager.cpp src/ServerPingController.cpp src/ServerCommandController.cpp src/ServerLogController.cpp
OUT := ../build_make/rat_server

all: $(OUT)


LIBS := -L../build_make -lutils -lsfml-network -lsfml-system -Wl,-rpath,'$$ORIGIN'

$(OUT): $(SRC)
	@mkdir -p ../build_make
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^ $(LIBS)

clean:
	rm -f $(OUT)

.PHONY: all clean

CC := g++
CFLAGS := -std=c++17 -O2 -Wall -Wextra
INCLUDES := -Iinclude -I../Utils/include
SRC := src/main.cpp src/ServerController.cpp src/ServerManager.cpp src/ServerPingController.cpp src/ServerCommandController.cpp src/ServerLogController.cpp
OUT := ../build_make/rat_server

# standalone log tail program
LOG_SCRIPT_SRC := src/LogScriptMain.cpp
LOG_SCRIPT_OUT := ../build_make/log_script

all: $(OUT)


LIBS := -L../build_make -lutils -lsfml-network -lsfml-system -Wl,-rpath,'$$ORIGIN'

$(OUT): $(SRC)
	@mkdir -p ../build_make
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^ $(LIBS)

$(LOG_SCRIPT_OUT): $(LOG_SCRIPT_SRC)
	@mkdir -p ../build_make
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^ $(LIBS)

# convenience mapping for top-level Makefile: allow calling build_make/log_script from repo root
build_make/log_script: $(LOG_SCRIPT_OUT)
	@true

clean:
	rm -f $(OUT) $(LOG_SCRIPT_OUT)

.PHONY: all clean

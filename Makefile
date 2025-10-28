PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
BUILD_DIR := build_make

CC := g++
CFLAGS := -std=c++17 -O2 -Wall -Wextra

SERVER_DIR := RAT-Server
CLIENT_DIR := RAT-Client

all: $(BUILD_DIR)/libutils.so $(BUILD_DIR)/rat_server $(BUILD_DIR)/rat_client $(BUILD_DIR)/log_script

SRV_SRCS := $(wildcard $(SERVER_DIR)/src/*.cpp)
CLI_SRCS := $(wildcard $(CLIENT_DIR)/src/*.cpp)

$(BUILD_DIR)/rat_server: $(SRV_SRCS) | $(BUILD_DIR) $(BUILD_DIR)/libutils.so
	$(CC) $(CFLAGS) -IUtils/include -I$(SERVER_DIR)/include -o $@ $^ -L$(BUILD_DIR) -lutils -lsfml-network -lsfml-system -Wl,-rpath,'$$ORIGIN'

$(BUILD_DIR)/rat_client: $(CLI_SRCS) | $(BUILD_DIR) $(BUILD_DIR)/libutils.so
	$(CC) $(CFLAGS) -IUtils/include -I$(CLIENT_DIR)/include -o $@ $^ -L$(BUILD_DIR) -lutils -lsfml-network -lsfml-system -Wl,-rpath,'$$ORIGIN'

$(BUILD_DIR)/libutils.so:
	$(MAKE) -C Utils

$(BUILD_DIR)/log_script:
	$(MAKE) -C $(SERVER_DIR) $(BUILD_DIR)/log_script

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	$(MAKE) -C Utils clean || true
	rm -rf $(BUILD_DIR)

.PHONY: all clean

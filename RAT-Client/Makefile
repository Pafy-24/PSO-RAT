CC := g++
CFLAGS := -std=c++17 -O2 -Wall -Wextra
SRC := src/main.cpp
OUT := ../build_make/rat_client

all: $(OUT)

$(OUT): $(SRC)
	@mkdir -p ../build_make
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f $(OUT)

.PHONY: all clean

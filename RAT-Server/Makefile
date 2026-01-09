CXX = g++
CXXFLAGS = -std=c++17 -O2
INCLUDES = -Iinclude -I../Utils/include
LDFLAGS = -L../build_make -lutils -lsfml-network -lsfml-system -Wl,-rpath,'$$ORIGIN'

OUT = ../build_make/rat_server
SRCS = src/*.cpp

all: $(OUT)

$(OUT): $(SRCS)
	@mkdir -p ../build_make
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(OUT)

.PHONY: all clean

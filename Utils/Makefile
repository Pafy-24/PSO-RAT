SRCDIR := src
INCDIR := include
OBJDIR := ../build_make/obj_utils
OUT := ../build_make/libutils.so

CC := g++
CFLAGS := -std=c++17 -O2 -fPIC -I$(INCDIR) -Wall -Wextra
LDFLAGS := -shared

SRCS := $(wildcard $(SRCDIR)/*.cpp)
OBJS := $(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/%.o,$(SRCS))

all: $(OUT)

test: $(OUT)
	$(CC) -std=c++17 -Iinclude -o ../build_make/udp_test test/udp_test.cpp -L../build_make -lutils -lsfml-network -lsfml-system

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OUT): $(OBJS) | $(OBJDIR)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) -lsfml-network -lsfml-system

$(OBJDIR):
	mkdir -p $(OBJDIR)

clean:
	rm -rf $(OBJDIR) $(OUT)
	rm -f ../build_make/udp_test

.PHONY: all clean

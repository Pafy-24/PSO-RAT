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

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OUT): $(OBJS) | $(OBJDIR)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) -lsfml-network -lsfml-system

$(OBJDIR):
	mkdir -p $(OBJDIR)

clean:
	rm -rf $(OBJDIR) $(OUT)

.PHONY: all clean

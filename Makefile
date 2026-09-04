CC       ?= cc
CXX      ?= c++
AR       ?= ar
CFLAGS   ?= -std=c99 -O2 -Wall -Wextra -pedantic
CXXFLAGS ?= -std=c++11 -O2 -Wall -Wextra -pedantic
CPPFLAGS += -Iinclude

CXX_AVAILABLE := $(shell command -v $(firstword $(CXX)) 2>/dev/null)

LIB     = libbt.a
OBJ     = src/bt.o

all: $(LIB) patrol test_bt

$(LIB): $(OBJ)
	$(AR) rcs $@ $^

src/bt.o: src/bt.c include/bt.h
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

patrol: examples/patrol.c $(LIB)
	$(CC) $(CFLAGS) $(CPPFLAGS) $< $(LIB) -o $@

test_bt: test/test_bt.c $(LIB)
	$(CC) $(CFLAGS) $(CPPFLAGS) $< $(LIB) -o $@

check: test_bt cpp-smoke
	./test_bt

ifneq ($(CXX_AVAILABLE),)
cpp-smoke: test_cpp_linkage
	./test_cpp_linkage

test_cpp_linkage: test/test_cpp_linkage.cpp $(LIB)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $< $(LIB) -o $@
else
cpp-smoke:
	@echo "SKIP: no C++ compiler"
endif

run: patrol
	./patrol

clean:
	rm -f $(OBJ) $(LIB) patrol test_bt test_cpp_linkage

.PHONY: all check cpp-smoke run clean

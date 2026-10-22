CC       ?= cc
CXX      ?= c++
AR       ?= ar
CFLAGS   ?= -std=c99 -O2 -Wall -Wextra -pedantic
CXXFLAGS ?= -std=c++11 -O2 -Wall -Wextra -pedantic
CPPFLAGS += -Iinclude

CXX_AVAILABLE := $(shell command -v $(firstword $(CXX)) 2>/dev/null)

LIB     = libfsm.a
OBJ     = src/fsm.o

all: $(LIB) turnstile test_fsm

$(LIB): $(OBJ)
	$(AR) rcs $@ $^

src/fsm.o: src/fsm.c include/fsm.h
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

turnstile: examples/turnstile.c $(LIB)
	$(CC) $(CFLAGS) $(CPPFLAGS) $< $(LIB) -o $@

test_fsm: test/test_fsm.c $(LIB)
	$(CC) $(CFLAGS) $(CPPFLAGS) $< $(LIB) -o $@

check: test_fsm cpp-smoke
	./test_fsm

ifneq ($(CXX_AVAILABLE),)
cpp-smoke: test_cpp_linkage
	./test_cpp_linkage

test_cpp_linkage: test/test_cpp_linkage.cpp $(LIB)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $< $(LIB) -o $@
else
cpp-smoke:
	@echo "SKIP: no C++ compiler"
endif

run: turnstile
	./turnstile

clean:
	rm -f $(OBJ) $(LIB) turnstile test_fsm test_cpp_linkage

.PHONY: all check cpp-smoke run clean

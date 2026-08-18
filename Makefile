CC      ?= cc
AR      ?= ar
CFLAGS  ?= -std=c99 -O2 -Wall -Wextra -pedantic
CPPFLAGS += -Iinclude

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

check: test_fsm
	./test_fsm

run: turnstile
	./turnstile

clean:
	rm -f $(OBJ) $(LIB) turnstile test_fsm

.PHONY: all check run clean

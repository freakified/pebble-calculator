# Host-side unit tests for the calculator engine.
#
#   make test    compile the engine natively and run the suite
#   make clean   remove the host test build
#
# This does NOT build the Pebble app (use `pebble build` for that). It compiles
# only the pure-C engine/math/format sources against a tiny <pebble.h> shim
# (test/shim) so the calculation logic can be tested off-device.

CC      ?= cc
CFLAGS  := -std=c11 -Wall -Wextra -Itest/shim -Isrc/c
OUT     := build-test/run

ENGINE_SRC := \
	src/c/calc_engine.c \
	src/c/calc_engine_std.c \
	src/c/calc_engine_rpn.c \
	src/c/calc_engine_sci.c \
	src/c/calc_math.c \
	src/c/calc_format.c

TEST_SRC := test/test_engine.c

.PHONY: test clean

test: $(OUT)
	@./$(OUT)

$(OUT): $(ENGINE_SRC) $(TEST_SRC) test/test_harness.h test/shim/pebble.h
	@mkdir -p build-test
	$(CC) $(CFLAGS) $(ENGINE_SRC) $(TEST_SRC) -o $(OUT)

clean:
	rm -rf build-test

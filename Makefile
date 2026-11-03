CXX      := g++
WARNINGS := -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion -Wshadow -Wold-style-cast -Wcast-align -Wnull-dereference -Wformat=2
CXXFLAGS := -std=c++20 -O3 -march=native $(WARNINGS) -Iinclude
SANFLAGS := -std=c++20 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer $(WARNINGS) -Iinclude

BUILD_DIR := build
LIB_SRC   := src/order_book.cpp
HEADERS   := $(wildcard include/acme/*.hpp)

.PHONY: all run test bench sanitize clean

all: $(BUILD_DIR)/acme_demo $(BUILD_DIR)/acme_tests $(BUILD_DIR)/acme_bench

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/acme_demo: src/main.cpp $(LIB_SRC) $(HEADERS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) src/main.cpp $(LIB_SRC) -o $@ -pthread

$(BUILD_DIR)/acme_tests: tests/test_order_book.cpp $(LIB_SRC) $(HEADERS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) tests/test_order_book.cpp $(LIB_SRC) -o $@ -pthread

$(BUILD_DIR)/acme_bench: benchmarks/bench_order_book.cpp $(LIB_SRC) $(HEADERS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) benchmarks/bench_order_book.cpp $(LIB_SRC) -o $@ -pthread

run: $(BUILD_DIR)/acme_demo
	./$(BUILD_DIR)/acme_demo

test: $(BUILD_DIR)/acme_tests
	./$(BUILD_DIR)/acme_tests

bench: $(BUILD_DIR)/acme_bench
	./$(BUILD_DIR)/acme_bench

sanitize: | $(BUILD_DIR)
	$(CXX) $(SANFLAGS) tests/test_order_book.cpp $(LIB_SRC) -o $(BUILD_DIR)/acme_tests_asan -pthread
	./$(BUILD_DIR)/acme_tests_asan

clean:
	rm -rf $(BUILD_DIR)

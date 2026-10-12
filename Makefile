BUILD_DIR ?= build
CMAKE ?= cmake

.PHONY: all configure run test clean help

all: configure
	$(CMAKE) --build $(BUILD_DIR) --parallel

configure:
	$(CMAKE) -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Release -DGRIDFALL_FETCH_RAYLIB=ON -DGRIDFALL_BUILD_TESTS=ON

run: all
	./run-gridfall-wsl.sh

test: configure
	$(CMAKE) --build $(BUILD_DIR) --parallel
	ctest --test-dir $(BUILD_DIR) --output-on-failure

clean:
	rm -rf $(BUILD_DIR)

help:
	@echo "make       Configure and build Gridfall"
	@echo "make run   Build, then launch the game"
	@echo "make test  Build and run headless domain tests"
	@echo "make clean Remove generated build files"

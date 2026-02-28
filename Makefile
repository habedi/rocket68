####################################################################################################
## Variables
####################################################################################################
CC       := gcc # Change to `clang` if needed
AR       := ar

# Set BUILD_TYPE=release for an optimized build
BUILD_TYPE ?= debug
ifeq ($(BUILD_TYPE),release)
    CFLAGS += -O3
else
    CFLAGS += -g -O0
endif

# Common build flags
CFLAGS   += -Wall -Wextra -pedantic -std=c11 -Iinclude -fPIC
LDFLAGS  :=
LIBS     :=

# Directories
SRC_DIR   := src
INC_DIR   := include
TEST_DIR  := test
BIN_DIR   := bin
LIB_DIR   := lib
TARGET_DIR:= obj
DOC_DIR   := docs
COVERAGE_BIN_DIR := bin_coverage
COVERAGE_TARGET_DIR := obj_coverage

TEST_BINARY   := $(BIN_DIR)/test_main
BCD_TEST_BINARY := $(BIN_DIR)/test_bcd
COVERAGE_TEST_BINARY := $(COVERAGE_BIN_DIR)/test_main
SRC_FILES     := $(wildcard $(SRC_DIR)/*.c) $(wildcard $(SRC_DIR)/m68k/*.c)
OBJ_FILES     := $(patsubst $(SRC_DIR)/%.c, $(TARGET_DIR)/%.o, $(SRC_FILES))
DEP_FILES     := $(OBJ_FILES:.o=.d)
TEST_FILES    := $(wildcard $(TEST_DIR)/*.c)
# Unit test files exclude test_json.c (has its own main) and test_bcd_verifier.c (standalone verifier)
UNIT_TEST_FILES := $(filter-out $(TEST_DIR)/test_json.c $(TEST_DIR)/test_bcd_verifier.c, $(TEST_FILES))
COVERAGE_OBJ_FILES := $(patsubst $(SRC_DIR)/%.c, $(COVERAGE_TARGET_DIR)/%.o, $(SRC_FILES))
COVERAGE_DEP_FILES := $(COVERAGE_OBJ_FILES:.o=.d)
STATIC_LIB    := $(LIB_DIR)/librocket68.a
SHARED_LIB    := $(LIB_DIR)/librocket68.so

# Adjust PATH if necessary (append /snap/bin if not present)
PATH := $(if $(findstring /snap/bin,$(PATH)),$(PATH),/snap/bin:$(PATH))

SHELL := /bin/bash
.SHELLFLAGS := -e -o pipefail -c

# Create directories if they don't exist
$(BIN_DIR) $(TARGET_DIR) $(LIB_DIR) $(COVERAGE_BIN_DIR) $(COVERAGE_TARGET_DIR):
	mkdir -p $@

####################################################################################################
## C Targets
####################################################################################################

.DEFAULT_GOAL := help

.PHONY: help
help: ## Show the help messages for all targets
	@grep -E '^[a-zA-Z0-9_-]+:.*?## ' Makefile | awk 'BEGIN {FS = ":.*?## "}; {printf "  %-10s %s\n", $$1, $$2}'

.PHONY: all
all: static shared ## Build static and shared library versions of Rocket 68
	@echo "Building all targets..."

# Build object files with dependency generation
$(TARGET_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(COVERAGE_TARGET_DIR)/%.o: CFLAGS += -fprofile-arcs -ftest-coverage -fprofile-prefix-map=$(PWD)=.
$(COVERAGE_TARGET_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: test
test: test-unit test-json ## Run unit tests plus JSON compatibility suite
	@echo "All core test suites passed."

.PHONY: test-unit
test-unit: $(TEST_BINARY) ## Run unit tests (excluding JSON/BCD/Musashi, etc.)
	@echo "Running tests..."
	./$(TEST_BINARY)

$(TEST_BINARY): $(UNIT_TEST_FILES) $(OBJ_FILES) | $(BIN_DIR)
	@echo "Building test binary..."
	$(CC) $(CFLAGS) $(UNIT_TEST_FILES) $(OBJ_FILES) -o $@

$(COVERAGE_TEST_BINARY): CFLAGS += -fprofile-arcs -ftest-coverage -fprofile-prefix-map=$(PWD)=.
$(COVERAGE_TEST_BINARY): LDFLAGS += -lgcov
$(COVERAGE_TEST_BINARY): $(UNIT_TEST_FILES) $(COVERAGE_OBJ_FILES) | $(COVERAGE_BIN_DIR)
	@echo "Building coverage test binary..."
	$(CC) $(CFLAGS) $(UNIT_TEST_FILES) $(COVERAGE_OBJ_FILES) -o $@ $(LDFLAGS)

.PHONY: test-json
test-json: $(BIN_DIR)/test_json ## Run JSON test suite
	@if [ ! -d external/m68000-json-tests/v1 ]; then \
		echo "JSON tests missing. Run 'git submodule update --init --recursive'"; \
		exit 1; \
	fi
	./$(BIN_DIR)/test_json external/m68000-json-tests/v1

$(BIN_DIR)/test_json: $(TEST_DIR)/test_json.c $(OBJ_FILES) | $(BIN_DIR)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $^ -o $@

$(BCD_TEST_BINARY): $(TEST_DIR)/test_bcd_verifier.c $(OBJ_FILES) | $(BIN_DIR)
	@echo "Building BCD test binary..."
	$(CC) $(CFLAGS) $^ -o $@

.PHONY: test-musashi
test-musashi: ## Run Musashi's test suite
	@if [ ! -d external/musashi ]; then \
		echo "Musashi submodule missing. Run 'git submodule update --init --recursive'"; \
		exit 0; \
	fi
	@tmp_log=$$(mktemp); \
	$(MAKE) --no-print-directory -C external/musashi test > "$$tmp_log" 2>&1; \
	status=$$?; \
	grep -v "warning:" "$$tmp_log" || true; \
	rm -f "$$tmp_log"; \
	exit $$status

.PHONY: test-bcd
test-bcd: ## Run BCD test suite
	@if [ ! -d external/bcd-test-rom ]; then \
		echo "BCD test ROM submodule missing. Run 'git submodule update --init --recursive'"; \
		exit 0; \
	fi
	@mkdir -p external/bcd-test-rom/data
	@g++ -O2 -std=c++11 -o external/bcd-test-rom/bcd-gen \
		external/bcd-test-rom/bcd-gen.cc \
		external/bcd-test-rom/bcd-emul.cc
	@cd external/bcd-test-rom && ./bcd-gen
	@$(MAKE) --no-print-directory $(BCD_TEST_BINARY)
	@BCD_TABLE_PATH=external/bcd-test-rom/data/bcd-table.bin ./$(BCD_TEST_BINARY)

.PHONY: test-memory
test-memory: $(TEST_BINARY) $(BIN_DIR)/test_json $(BCD_TEST_BINARY) ## Run the tests with Valgrind to check for memory leaks
	@echo "Running tests with Valgrind..."
	valgrind --error-exitcode=1 --leak-check=full --suppressions=valgrind.supp ./$(TEST_BINARY)
	@if [ -d external/m68000-json-tests/v1 ]; then \
		valgrind --error-exitcode=1 --leak-check=full --suppressions=valgrind.supp ./$(BIN_DIR)/test_json external/m68000-json-tests/v1 || exit 1; \
	fi
	@if [ -f external/bcd-test-rom/data/bcd-table.bin ]; then \
		env BCD_TABLE_PATH=external/bcd-test-rom/data/bcd-table.bin valgrind --error-exitcode=1 --leak-check=full --suppressions=valgrind.supp ./$(BCD_TEST_BINARY) || exit 1; \
	fi

.PHONY: test-ubsan
test-ubsan: CFLAGS += -fsanitize=undefined
test-ubsan: LDFLAGS += -fsanitize=undefined
test-ubsan: clean $(TEST_BINARY) $(BIN_DIR)/benchmark_rocket68 $(BIN_DIR)/test_json ## Run `undefined behavior sanitizer` tests
	@echo "Running UBSan tests..."
	UBSAN_OPTIONS=print_stacktrace=1 ./$(TEST_BINARY)
	UBSAN_OPTIONS=print_stacktrace=1 ./$(BIN_DIR)/benchmark_rocket68
	@if [ -d external/m68000-json-tests/v1 ]; then \
		UBSAN_OPTIONS=print_stacktrace=1 ./$(BIN_DIR)/test_json external/m68000-json-tests/v1; \
	fi

.PHONY: test-asan
test-asan: CFLAGS += -fsanitize=address
test-asan: LDFLAGS += -fsanitize=address
test-asan: clean $(TEST_BINARY) $(BIN_DIR)/benchmark_rocket68 $(BIN_DIR)/test_json ## Run `address sanitizer` tests
	@echo "Running ASan tests..."
	ASAN_OPTIONS=detect_leaks=1 ./$(TEST_BINARY)
	ASAN_OPTIONS=detect_leaks=1 ./$(BIN_DIR)/benchmark_rocket68
	@if [ -d external/m68000-json-tests/v1 ]; then \
		ASAN_OPTIONS=detect_leaks=1 ./$(BIN_DIR)/test_json external/m68000-json-tests/v1; \
	fi

.PHONY: static
static: $(STATIC_LIB) ## Build static library version of Rocket 68
	@echo "Static library built at $(STATIC_LIB)"

$(STATIC_LIB): $(OBJ_FILES) | $(LIB_DIR)
	$(AR) rcs $@ $^

.PHONY: shared
shared: $(SHARED_LIB) ## Build shared library version of Rocket 68
	@echo "Shared library built at $(SHARED_LIB)"

$(SHARED_LIB): $(OBJ_FILES) | $(LIB_DIR)
	$(CC) -shared -o $@ $^

.PHONY: format
format: ## Format the C files
	@echo "Formatting code..."
	clang-format -i $(SRC_FILES) $(wildcard $(INC_DIR)/*.h) $(TEST_FILES)

.PHONY: lint
lint: ## Run linter checks
	cppcheck --enable=all --inconclusive --quiet --std=c11 -I$(INC_DIR) --suppress=missingIncludeSystem $(SRC_DIR) $(INC_DIR) $(TEST_DIR)
	@if command -v clang-tidy &> /dev/null; then \
		clang-tidy $(SRC_FILES) -- $(CFLAGS); \
	else \
		echo "clang-tidy not found. Skipping."; \
	fi

.PHONY: docs
docs: ## Generate project documentation (Doxygen and MkDocs)
	@echo "Generating documentation..."
	mkdocs build
	doxygen Doxyfile

.PHONY: docs-serve
docs-serve: docs ## Serve Vq MkDocs locally
	@echo "Serving documentation locally..."
	python -m http.server --directory ./site

.PHONY: coverage
coverage: $(COVERAGE_TEST_BINARY) ## Generate code coverage report
	@echo "Running coverage tests..."
	./$(COVERAGE_TEST_BINARY)
	@echo "Generating code coverage report..."
	gcov -o $(COVERAGE_TARGET_DIR) $(wildcard $(SRC_DIR)/*.c)
	gcov -o $(COVERAGE_TARGET_DIR)/m68k $(wildcard $(SRC_DIR)/m68k/*.c)

.PHONY: bench
bench: $(BIN_DIR)/benchmark_rocket68 $(BIN_DIR)/benchmark_musashi ## Run benchmarks comparing Rocket68 to Musashi
	@echo "--- Rocket68 Benchmark ---"
	@./$(BIN_DIR)/benchmark_rocket68
	@echo "--- Musashi Benchmark ---"
	@./$(BIN_DIR)/benchmark_musashi

$(BIN_DIR)/benchmark_rocket68: benches/benchmark_rocket68.c static | $(BIN_DIR)
	@echo "Building Rocket68 benchmark..."
	@$(CC) $(CFLAGS) -O3 -I$(INC_DIR) benches/benchmark_rocket68.c $(STATIC_LIB) -o $(BIN_DIR)/benchmark_rocket68

$(BIN_DIR)/benchmark_musashi: benches/benchmark_musashi.c | $(BIN_DIR)
	@echo "Building Musashi benchmark..."
	@if [ ! -d external/musashi ]; then \
		echo "Musashi submodule missing. Run 'git submodule update --init --recursive'"; \
		exit 1; \
	fi
	@$(MAKE) --no-print-directory -C external/musashi
	@$(CC) -O3 -Iexternal/musashi $(CFLAGS) benches/benchmark_musashi.c external/musashi/m68kcpu.o external/musashi/m68kops.o external/musashi/m68kdasm.o external/musashi/softfloat/softfloat.o -lm -o $(BIN_DIR)/benchmark_musashi

.PHONY: zig-test
zig-test: ## Run tests using Zig build system
	@echo "Running tests with zig build..."
	zig build test

.PHONY: zig-bench
zig-bench: ## Run benchmarks using Zig build system
	@echo "Running benchmarks with zig build..."
	zig build bench -Doptimize=ReleaseFast

.PHONY: clean
clean: ## Remove all build artifacts
	@echo "Cleaning up..."
	rm -rf $(BIN_DIR) $(TARGET_DIR) $(LIB_DIR) $(COVERAGE_BIN_DIR) $(COVERAGE_TARGET_DIR) *.gcno *.gcda *.gcov *.out *.o *.a *.so *.d
	rm -rf site Doxyfile.bak zig-out .zig-cache

.PHONY: clean-coverage
clean-coverage: ## Remove coverage build artifacts only
	@echo "Cleaning coverage artifacts..."
	rm -rf $(COVERAGE_BIN_DIR) $(COVERAGE_TARGET_DIR) *.gcno *.gcda *.gcov

.PHONY: install-deps
install-deps: ## Install system and development dependencies (for Debian-based OSes)
	@echo "Installing system dependencies..."
	sudo apt-get update
	sudo apt-get install -y gcc g++ clang clang-format clang-tidy doxygen cppcheck valgrind gdb python3-pip snap
	pip install uv
	sudo snap install --classic --beta zig

.PHONY: nix-shell
nix-shell: ## Enter the Nix development environment
	@echo "Entering Nix development shell..."
	nix develop

.PHONY: nix-build
nix-build: ## Build the project using Nix
	@echo "Building project with Nix..."
	nix build

.PHONY: setup-hooks
setup-hooks: ## Install Git hooks (pre-commit and pre-push)
	@echo "Installing Git hooks..."
	@pre-commit install --hook-type pre-commit
	@pre-commit install --hook-type pre-push
	@pre-commit install-hooks

.PHONY: test-hooks
test-hooks: ## Run Git hooks on all files manually
	@echo "Running Git hooks..."
	@pre-commit run --all-files

# Include dependency files, if they exist.
-include $(DEP_FILES)
-include $(COVERAGE_DEP_FILES)

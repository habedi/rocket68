####################################################################################################
## Variables
####################################################################################################
# Compiler and archiver
CC       := gcc # Change to `clang` if needed
AR       := ar

# Build configuration: set BUILD_TYPE=release for an optimized build.
BUILD_TYPE ?= debug
ifeq ($(BUILD_TYPE),release)
    CFLAGS += -O2
else
    CFLAGS += -g -O0
endif

# Common flags, including automatic dependency generation (-MMD -MP)
CFLAGS   += -Wall -Wextra -pedantic -std=c11 -fPIC -Iinclude -MMD -MP
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

# Names and Files
BINARY_NAME   := main
BINARY        := $(BIN_DIR)/$(BINARY_NAME)
TEST_BINARY   := $(BIN_DIR)/test_$(BINARY_NAME)
BCD_TEST_BINARY := $(BIN_DIR)/test_bcd
SRC_FILES     := $(wildcard $(SRC_DIR)/*.c) $(wildcard $(SRC_DIR)/m68k/*.c)
OBJ_FILES     := $(patsubst $(SRC_DIR)/%.c, $(TARGET_DIR)/%.o, $(SRC_FILES))
DEP_FILES     := $(OBJ_FILES:.o=.d)
TEST_FILES    := $(wildcard $(TEST_DIR)/*.c)
# Unit test files exclude test_json.c which has its own main()
UNIT_TEST_FILES := $(filter-out $(TEST_DIR)/test_json.c, $(TEST_FILES))
STATIC_LIB    := $(LIB_DIR)/libproject.a
SHARED_LIB    := $(LIB_DIR)/libproject.so

# Adjust PATH if necessary (append /snap/bin if not present)
PATH := $(if $(findstring /snap/bin,$(PATH)),$(PATH),/snap/bin:$(PATH))

SHELL := /bin/bash
.SHELLFLAGS := -e -o pipefail -c

# Create directories as needed
$(BIN_DIR) $(TARGET_DIR) $(LIB_DIR):
	mkdir -p $@

####################################################################################################
## C Targets
####################################################################################################

.DEFAULT_GOAL := help

.PHONY: help
help: ## Show the help messages for all targets
	@grep -E '^[a-zA-Z0-9_-]+:.*?## ' Makefile | awk 'BEGIN {FS = ":.*?## "}; {printf "  %-10s %s\n", $$1, $$2}'

.PHONY: all
all: build static shared ## Build everything
	@echo "Building all targets..."

.PHONY: build
build: $(BINARY) ## Build the main binary
	@echo "Main binary built at $(BINARY)"

$(BINARY): $(OBJ_FILES) | $(BIN_DIR)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) $(LIBS)

# Build object files with dependency generation
$(TARGET_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: rebuild
rebuild: clean all ## Clean and rebuild everything
	@echo "Rebuilding all targets..."

.PHONY: run
run: build ## Run the main binary
	@echo "Running main binary..."
	./$(BINARY)

.PHONY: test
test: $(TEST_BINARY) ## Run the tests
	@echo "Running tests..."
	./$(TEST_BINARY)

$(TEST_BINARY): $(UNIT_TEST_FILES) $(filter-out $(TARGET_DIR)/main.o, $(OBJ_FILES)) | $(BIN_DIR)
	@echo "Building test binary..."
	$(CC) $(CFLAGS) $(UNIT_TEST_FILES) $(filter-out $(TARGET_DIR)/main.o, $(OBJ_FILES)) -o $@

.PHONY: test-json
test-json: $(BIN_DIR)/test_json ## Run the JSON test
	./$(BIN_DIR)/test_json external/m68000-json-tests/v1

$(BIN_DIR)/test_json: $(TEST_DIR)/test_json.c $(filter-out $(TARGET_DIR)/main.o, $(OBJ_FILES)) | $(BIN_DIR)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $^ -o $@

$(BCD_TEST_BINARY): $(TEST_DIR)/test_bcd_verifier.c $(filter-out $(TARGET_DIR)/main.o, $(OBJ_FILES)) | $(BIN_DIR)
	@echo "Building BCD test binary..."
	$(CC) $(CFLAGS) $^ -o $@

.PHONY: test-valgrind
test-valgrind: $(TEST_BINARY) ## Run tests with Valgrind using suppressions
	@echo "Running tests with Valgrind..."
	valgrind --leak-check=full --suppressions=valgrind.supp ./$(TEST_BINARY)

.PHONY: static
static: $(STATIC_LIB) ## Build static library
	@echo "Static library built at $(STATIC_LIB)"

$(STATIC_LIB): $(OBJ_FILES) | $(LIB_DIR)
	$(AR) rcs $@ $^

.PHONY: shared
shared: $(SHARED_LIB) ## Build shared library
	@echo "Shared library built at $(SHARED_LIB)"

$(SHARED_LIB): $(OBJ_FILES) | $(LIB_DIR)
	$(CC) -shared -o $@ $^

.PHONY: install
install: all ## Install binary, headers, and libs (to /usr/local)
	@echo "Installing..."
	install -d /usr/local/bin /usr/local/include /usr/local/lib
	install -m 0755 $(BINARY) /usr/local/bin/
	cp -r $(INC_DIR)/*.h /usr/local/include/
	install -m 0644 $(STATIC_LIB) /usr/local/lib/
	install -m 0755 $(SHARED_LIB) /usr/local/lib/

.PHONY: uninstall
uninstall: ## Uninstall everything (from /usr/local)
	@echo "Uninstalling..."
	rm -f /usr/local/bin/$(BINARY_NAME)
	rm -f /usr/local/include/*.h
	rm -f /usr/local/lib/libproject.a
	rm -f /usr/local/lib/libproject.so

.PHONY: format
format: ## Format code with clang-format (requires a .clang-format file)
	@echo "Formatting code..."
	clang-format -i $(SRC_FILES) $(wildcard $(INC_DIR)/*.h) $(TEST_FILES)

.PHONY: lint
lint: ## Run cppcheck and clang-tidy
	cppcheck --enable=all --inconclusive --quiet --std=c11 -I$(INC_DIR) --suppress=missingIncludeSystem $(SRC_DIR) $(INC_DIR) $(TEST_DIR)
	@if command -v clang-tidy &> /dev/null; then \
		clang-tidy $(SRC_FILES) -- $(CFLAGS); \
	else \
		echo "clang-tidy not found. Skipping."; \
	fi

.PHONY: docs
docs: ## Generate documentation with Doxygen
	@echo "Generating documentation..."
	doxygen Doxyfile

.PHONY: coverage
coverage: CFLAGS += -fprofile-arcs -ftest-coverage -fprofile-prefix-map=$(PWD)=.
coverage: LDFLAGS += -lgcov
coverage: test ## Generate code coverage report
	@echo "Generating code coverage report..."
	gcov -o $(TARGET_DIR) $(filter-out $(SRC_DIR)/main.c, $(SRC_FILES))

.PHONY: clean
clean: ## Remove all build artifacts
	@echo "Cleaning up..."
	rm -rf $(BIN_DIR) $(TARGET_DIR) $(LIB_DIR) *.gcno *.gcda *.gcov *.out *.o *.a *.so *.d
	rm -rf $(DOC_DIR)/html $(DOC_DIR)/latex Doxyfile.bak

.PHONY: install-deps
install-deps: ## Install system and development dependencies (for Debian-based OSes)
	@echo "Installing system dependencies..."
	sudo apt-get update
	sudo apt-get install -y gcc clang clang-format clang-tidy doxygen cppcheck valgrind gdb

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

.PHONY: test-musashi
test-musashi: ## Run Musashi's own test suite (optional submodule)
	@if [ ! -d external/musashi ]; then \
		echo "Musashi submodule missing. Run 'git submodule update --init --recursive'"; \
		exit 0; \
	fi
	@$(MAKE) --no-print-directory -C external/musashi test 2>&1 | grep -v "warning:"

.PHONY: test-bcd
test-bcd: ## Generate Flamewing BCD table and run BCD verifier
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

# Include dependency files, if they exist.
-include $(DEP_FILES)

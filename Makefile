PROJECT := PureC_zlib
CC ?= gcc
CSTD := -std=c11
WARN := -Wall -Wextra -Wpedantic
OPT ?= -O3
CFLAGS ?= $(CSTD) $(WARN) $(OPT)
LDFLAGS ?=
LDLIBS ?=

BUILD_DIR := obj
BIN_DIR := bin
APP := $(BIN_DIR)/purec_zlib
TEST_APP := $(BIN_DIR)/test_roundtrip

INCLUDES := -Iinclude

SRCS := \
	app/main.c \
	modules/module_common/pczlib_common.c \
	modules/module_common/pczlib_fileio.c \
	modules/module_compute/pcz_compute_scalar.c \
	modules/module_compute/pcz_compute_x86.c \
	modules/module_compute/pcz_compute_arm.c \
	modules/module_compute/pcz_compute_opencl.c \
	modules/module_compute/pcz_compute_select.c \
	modules/module_deflate/pcz_deflate.c \
	modules/module_zlib/pczlib_api.c

LIB_SRCS := $(filter-out app/main.c,$(SRCS))
OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(SRCS))
TEST_OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(LIB_SRCS) tests/test_roundtrip.c)
DEPS := $(OBJS:.o=.d) $(TEST_OBJS:.o=.d)

BACKEND ?= auto
LEVEL ?= 6
INPUT ?= examples/sample.txt
OUTPUT ?= bin/sample.z
ZINPUT ?= bin/sample.z
ZOUTPUT ?= bin/sample.out
ROUNDTRIP_INPUT ?= examples/sample.txt

.PHONY: all help build clean test quickstart ci \
	compress decompress roundtrip info \
	compress_scalar compress_avx2 compress_avx512 compress_neon compress_opencl \
	decompress_scalar decompress_avx2 decompress_avx512 decompress_neon decompress_opencl

all: build

help:
	@echo "$(PROJECT) targets"
	@echo "  make build"
	@echo "  make test"
	@echo "  make quickstart"
	@echo "  make compress INPUT=... OUTPUT=... [BACKEND=auto|scalar|avx2|avx512|neon|opencl] [LEVEL=0..9]"
	@echo "  make decompress ZINPUT=... ZOUTPUT=... [BACKEND=...]"
	@echo "  make roundtrip ROUNDTRIP_INPUT=... [BACKEND=...] [LEVEL=...]"
	@echo "  make info ZINPUT=..."
	@echo "  make ci"
	@echo ""
	@echo "Backend shortcuts:"
	@echo "  make compress_scalar|compress_avx2|compress_avx512|compress_neon|compress_opencl"
	@echo "  make decompress_scalar|decompress_avx2|decompress_avx512|decompress_neon|decompress_opencl"

build: $(APP)

$(APP): $(OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(OBJS) $(LDFLAGS) $(LDLIBS) -o $@

$(TEST_APP): $(TEST_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(TEST_OBJS) $(LDFLAGS) $(LDLIBS) -o $@

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -MMD -MP -c $< -o $@

test: $(TEST_APP)
	$(TEST_APP)

compress: $(APP)
	$(APP) compress --input $(INPUT) --output $(OUTPUT) --backend $(BACKEND) --level $(LEVEL)

decompress: $(APP)
	$(APP) decompress --input $(ZINPUT) --output $(ZOUTPUT) --backend $(BACKEND)

roundtrip: $(APP)
	$(APP) roundtrip --input $(ROUNDTRIP_INPUT) --backend $(BACKEND) --level $(LEVEL)

info: $(APP)
	$(APP) info --input $(ZINPUT)

compress_scalar:
	$(MAKE) compress BACKEND=scalar

compress_avx2:
	$(MAKE) compress BACKEND=avx2

compress_avx512:
	$(MAKE) compress BACKEND=avx512

compress_neon:
	$(MAKE) compress BACKEND=neon

compress_opencl:
	$(MAKE) compress BACKEND=opencl

decompress_scalar:
	$(MAKE) decompress BACKEND=scalar

decompress_avx2:
	$(MAKE) decompress BACKEND=avx2

decompress_avx512:
	$(MAKE) decompress BACKEND=avx512

decompress_neon:
	$(MAKE) decompress BACKEND=neon

decompress_opencl:
	$(MAKE) decompress BACKEND=opencl

quickstart: build
	$(MAKE) compress INPUT=examples/sample.txt OUTPUT=bin/quickstart.z BACKEND=auto LEVEL=6
	$(MAKE) decompress ZINPUT=bin/quickstart.z ZOUTPUT=bin/quickstart.out BACKEND=auto
	@echo "quickstart done: bin/quickstart.z and bin/quickstart.out"

ci:
	$(MAKE) clean
	$(MAKE) build
	$(MAKE) test

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

-include $(DEPS)

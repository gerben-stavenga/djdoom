CC := gcc
CFLAGS := -O2 -m32 -fomit-frame-pointer -Wno-attributes -Wpedantic -Wno-unused-result -DAPPVER_EXEDEF=DM19F2

DBGFLAGS := -g
COBJFLAGS := $(CFLAGS) -c
AS := nasm

SRC_DIR := src
BUILD_DIR := build


# Find all .c files
SRC := $(wildcard $(SRC_DIR)/*.c)
DOS_SRC := $(wildcard $(SRC_DIR)/dos/*.c)
LINUX_SRC := $(wildcard $(SRC_DIR)/linux/*.c)

OBJ := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRC)) $(BUILD_DIR)/planar.o
DOS_OBJ := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(DOS_SRC)) $(BUILD_DIR)/dos/a_mv_mix.o
LINUX_OBJ := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(LINUX_SRC))

$(BUILD_DIR)/doom.exe: $(OBJ) $(DOS_OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

$(BUILD_DIR)/linux-doom: $(OBJ) $(LINUX_SRC)
	$(CC) -o $@ $^ $(CFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)/%.dir
	$(CC) $(COBJFLAGS) -o $@ $<

$(BUILD_DIR)/%.s: $(SRC_DIR)/%.c | $(BUILD_DIR)/%.dir
	$(CC) $(COBJFLAGS) -S -o $@ $<

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.asm | $(BUILD_DIR)/%.dir
	$(AS) $< -f coff -o $@

%.dir:
	mkdir -p $(dir $@)

# phony rules
.PHONY: all
all: $(BUILD_DIR)/doom.exe

.PHONY: debug
debug: $(TARGET_DEBUG)

.PHONY: clean
clean:
	@rm -rf $(BUILD_DIR)
	@rm -rf $(DBG_PATH)

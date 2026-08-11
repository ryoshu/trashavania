CC65_PREFIX := /opt/homebrew/opt/cc65
CA65        := ca65
CC65        := cc65
LD65        := ld65

CFLAGS      := -t nes -Oi --add-source
ASFLAGS     := -t nes

SRC_DIR     := src
BUILD_DIR   := build
CFG         := $(SRC_DIR)/nes-uxrom.cfg
NESLIB      := $(CC65_PREFIX)/share/cc65/lib/nes.lib

TARGET      := $(BUILD_DIR)/trashavania.nes
MAP         := $(BUILD_DIR)/trashavania.map

C_SOURCES   := $(SRC_DIR)/main.c $(SRC_DIR)/render.c $(SRC_DIR)/player.c \
               $(SRC_DIR)/entities.c $(SRC_DIR)/boss.c $(SRC_DIR)/audio.c \
               $(SRC_DIR)/assets.c $(SRC_DIR)/ppu.c $(SRC_DIR)/pad.c
ASM_SOURCES := $(SRC_DIR)/crt0.s

OBJECTS     := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(C_SOURCES)) \
               $(patsubst $(SRC_DIR)/%.s,$(BUILD_DIR)/%.o,$(ASM_SOURCES))

.PHONY: all clean run run-fceux assets

all: $(TARGET)

# Regenerate src/assets.c/.h + PNG previews from tools/genassets.py
$(SRC_DIR)/assets.c $(SRC_DIR)/assets.h: tools/genassets.py | $(BUILD_DIR)
	python3 tools/genassets.py

assets: $(SRC_DIR)/assets.c

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.s: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC65) $(CFLAGS) $< -o $@

$(BUILD_DIR)/%.o: $(BUILD_DIR)/%.s
	$(CA65) $(ASFLAGS) $< -o $@

$(BUILD_DIR)/crt0.o: $(SRC_DIR)/crt0.s | $(BUILD_DIR)
	$(CA65) $(ASFLAGS) $< -o $@

$(TARGET): $(OBJECTS) $(CFG)
	$(LD65) -C $(CFG) -m $(MAP) -o $(TARGET) $(OBJECTS) $(NESLIB)

clean:
	rm -rf $(BUILD_DIR)

# Launch the built ROM in Mesen (accurate emulator, sprite/timing diagnostics)
run: $(TARGET)
	open -a "$(CURDIR)/tools/Mesen2/Mesen.app" --args "$(CURDIR)/$(TARGET)"

# Launch in FCEUX instead (CLI-friendly / scriptable)
run-fceux: $(TARGET)
	fceux $(TARGET)

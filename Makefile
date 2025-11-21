# Makefile pour l'émulateur NES

CC = gcc
CFLAGS = -Wall -O2 -Iinclude `sdl2-config --cflags`
LDFLAGS = `sdl2-config --libs`

# Répertoires
SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin

# Fichiers
SOURCES = $(SRC_DIR)/main.c $(SRC_DIR)/cpu.c $(SRC_DIR)/ppu.c $(SRC_DIR)/cartridge.c $(SRC_DIR)/mapper0.c
OBJECTS = $(SOURCES:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
TARGET = $(BIN_DIR)/nes

# Règle par défaut
all: directories $(TARGET)

# Créer les répertoires nécessaires
directories:
	@mkdir -p $(OBJ_DIR)
	@mkdir -p $(BIN_DIR)

# Compilation de l'exécutable
$(TARGET): $(OBJECTS)
	@echo "🔗 Linking $(TARGET)..."
	@$(CC) $(OBJECTS) -o $(TARGET) $(LDFLAGS)
	@echo "✅ Build successful!"

# Compilation des fichiers objets
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@echo "🔨 Compiling $<..."
	@$(CC) $(CFLAGS) -c $< -o $@

# Nettoyage
clean:
	@echo "🧹 Cleaning..."
	@rm -rf $(OBJ_DIR) $(BIN_DIR)
	@echo "✅ Cleaned!"

# Rebuild complet
rebuild: clean all

# Exécuter
run: all
	@echo "🎮 Running emulator..."
	@$(TARGET)

# Tester avec une ROM
test: all
	@if [ -f "roms/test.nes" ]; then \
		echo "🎮 Testing with roms/test.nes..."; \
		$(TARGET) roms/test.nes; \
	else \
		echo "❌ No test ROM found at roms/test.nes"; \
		echo "   Place a .nes file there or run: make run ROM=path/to/game.nes"; \
	fi

# Aide
help:
	@echo "NES Emulator - Makefile Commands:"
	@echo "  make           - Build the emulator"
	@echo "  make clean     - Remove build files"
	@echo "  make rebuild   - Clean and rebuild"
	@echo "  make run       - Build and run (needs ROM argument)"
	@echo "  make test      - Run with test ROM"
	@echo ""
	@echo "Usage:"
	@echo "  ./bin/nes_emulator <rom_file.nes>"

.PHONY: all clean rebuild run test help directories
# Compiler et flags
CC = gcc
CFLAGS = -Wall -O2 -Iinclude

# Sources et cible
SRC = src/main.c src/cpu.c
TARGET = nes

# Règle par défaut
all: $(TARGET)

# Compilation du programme
$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

# Nettoyage
clean:
	rm -f $(TARGET)
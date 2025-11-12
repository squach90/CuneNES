CC = gcc
CFLAGS = -Wall -O2 -Iinclude

SRC = src/main.c src/cpu.c
TARGET = nes

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)
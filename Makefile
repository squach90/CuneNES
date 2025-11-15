CC = gcc
CFLAGS = -Wall -O2 -Iinclude `sdl2-config --cflags`
LDFLAGS = `sdl2-config --libs`

SRC = src/main.c src/cpu.c
TARGET = nes

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)

clean:
	rm -f $(TARGET)

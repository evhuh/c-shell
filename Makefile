CC = gcc
CFLAGS = -Wall -Wextra -std=c11
SRC = src/main.c src/utils.c
BIN = build/shell

.PHONY: build clean run

build:
	@mkdir -p build
	$(CC) $(CFLAGS) $(SRC) -o $(BIN)

clean:
	rm -rf build

run: build
	./$(BIN)

CC = gcc
CFLAGS = -Wall -Wextra -std=c11
SRC = src/main.c src/utils.c src/completion.c src/jobs.c src/history.c src/pipeline.c -lreadline
BIN = build/shell

.PHONY: build clean run

build:
	@mkdir -p build
	$(CC) $(CFLAGS) $(SRC) -o $(BIN)

clean:
	rm -rf build

run: build
	./$(BIN)

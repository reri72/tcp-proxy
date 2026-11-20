CC = gcc
CFLAGS = -Wall -Wextra -O2 -Iinclude
BIN_DIR = bin
SRC = \
		src/main.c

TARGET = $(BIN_DIR)/proxy

all: $(TARGET)

$(TARGET): $(SRC) 
		mkdir -p $(BIN_DIR)
		$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -rf $(BIN_DIR)

.PHONY: all clean
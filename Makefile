CC = gcc
CFLAGS = -Wall -Wextra -O2 -Iinclude

BIN_DIR = bin

TEST_DIR = test
TEST_SCRIPT = $(TEST_DIR)/build.sh

SRC = \
		src/net.c \
		src/event_loop.c \
		src/main.c

TARGET = $(BIN_DIR)/proxy

all: proxy test-bin

proxy: $(TARGET)

$(TARGET): $(SRC) 
		mkdir -p $(BIN_DIR)
		$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

test-bin: $(TEST_SCRIPT)
			bash $(TEST_SCRIPT)

clean:
	rm -rf $(BIN_DIR) $(TEST_DIR)/build $(TEST_DIR)/tmp

.PHONY: all proxy test-bin clean
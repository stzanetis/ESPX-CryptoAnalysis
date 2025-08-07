# Host Compiler
CC = gcc
TARGET_NAME = espx_crypto_pc

CFLAGS = -Wall -Wextra -Wpedantic -O2 -flto -D_GNU_SOURCE -pthread -Iinclude
LDFLAGS = -lwebsockets -lssl -lcrypto -lm -lpthread -latomic

SRC = src/main.c src/websocket/websocket.c src/logger/logger.c \
	src/processor/processor.c
OBJ = $(patsubst src/%.c,obj/%.o,$(SRC))

all: dirs host

dirs:
	@mkdir -p bin obj/websocket obj/logger obj/processor

host: dirs $(OBJ)
	$(CC) $(CFLAGS) -o bin/$(TARGET_NAME) $(OBJ) $(LDFLAGS)

obj/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf obj bin logs test/assets

.PHONY: all dirs host clean
# Host Compiler
CC = gcc
TARGET_NAME = espx_crypto_pc

CFLAGS = -Wall -Wextra -Wpedantic -O2 -flto -D_GNU_SOURCE -D_POSIX_C_SOURCE=199309L -pthread -Iinclude
LDFLAGS = -lwebsockets -lssl -lcrypto -lcjson -lm -lpthread -latomic

SRC = src/main.c src/websocket/websocket.c src/logger/logger.c \
	src/processor/processor.c src/utils/utils.c src/calculate/moving_avg.c src/calculate/correlation.c
OBJ = $(patsubst src/%.c,obj/%.o,$(SRC))

all: dirs host

dirs:
	@mkdir -p bin obj/websocket obj/logger obj/processor obj/utils obj/calculate \
		logs/transactions data/mavg data/corr

host: dirs $(OBJ)
	$(CC) $(CFLAGS) -o bin/$(TARGET_NAME) $(OBJ) $(LDFLAGS)

obj/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf obj bin logs data

.PHONY: all dirs host clean
# Host Compiler
CC = gcc
TARGET_NAME = espx_crypto_pc
CFLAGS = -Wall -Wextra -Wpedantic -O2 -flto -D_GNU_SOURCE -D_POSIX_C_SOURCE=199309L -pthread -Iinclude
LDFLAGS = -lwebsockets -lssl -lcrypto -lcjson -lm -lpthread -latomic

# Raspberry Compiler
CC_PI = arm-none-linux-gnueabihf-gcc
TARGET_NAME_PI = espx_crypto_pi
SYSROOT = sysroot_rpi
CFLAGS_PI := -Wall -Wextra -Wpedantic -O2 -flto -D_GNU_SOURCE -D_POSIX_C_SOURCE=199309L \
			 -pthread -Iinclude -marm -mcpu=arm1176jzf-s -mfpu=vfp --sysroot=$(SYSROOT) \
			 -B$(SYSROOT)/usr/lib/arm-linux-gnueabihf/ \
             -isystem $(SYSROOT)/usr/include \
			 -isystem $(SYSROOT)/usr/include/arm-linux-gnueabihf
LDFLAGS_PI := --sysroot=$(SYSROOT) -static \
			  -lwebsockets -lssl -lcrypto -lcjson -lm -lpthread -latomic \
			  -L$(SYSROOT)/lib -L$(SYSROOT)/usr/lib/arm-linux-gnueabihf \

SRC = src/main.c src/websocket/websocket.c src/logger/logger.c src/processor/processor.c src/utils/utils.c src/calculate/moving_avg.c src/calculate/correlation.c
OBJ = $(patsubst src/%.c,obj/pc/%.o,$(SRC))
OBJ_PI = $(patsubst src/%.c,obj/pi/%.o,$(SRC))

all: dirs host

dirs:
	@mkdir -p bin obj/websocket obj/logger obj/processor obj/utils obj/calculate logs/transactions data/mavg data/corr

host: dirs $(OBJ)
	$(CC) $(CFLAGS) -o bin/$(TARGET_NAME) $(OBJ) $(LDFLAGS)

pi: dirs $(OBJ)
	$(CC_PI) $(CFLAGS_PI) -o bin/$(TARGET_NAME_PI) $(OBJ_PI) $(LDFLAGS_PI)

obj/pc/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

obj/pi/%.o: src/%.c
	$(CC_PI) $(CFLAGS_PI) -c $< -o $@

clean:
	rm -rf obj bin logs data

.PHONY: all dirs host pi clean
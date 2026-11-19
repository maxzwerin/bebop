CC = gcc

CFLAGS = -O3 -Wall -Wextra -Iinclude

RAYLIB_CFLAGS := $(shell pkg-config --cflags raylib)
RAYLIB_LIBS   := $(shell pkg-config --libs raylib)

ENGINE_SRC = \
    movegen.c \
    utils.c \
    zobrist.c

GAME_SRC = \
    game.c \
    pgn.c

BOT_SRC = \
    bot_registry.c \
    bebop.c

UCI_SRC = \
    uci.c

MAIN_SRC = src/main.c

ALL_SRC = $(ENGINE_SRC) $(GAME_SRC) $(BOT_SRC) $(UCI_SRC)

.PHONY: all clean perft uci

uci: $(ALL_SRC) $(MAIN_SRC) | $(BUILD)
	$(CC) $(CFLAGS) -pthread $^ -o $@ $(LDFLAGS) -pthread

perft: $(ENGINE_SRC) perft.c | $(BUILD)
	$(CC) $(CFLAGS) $^ -o $(BUILD)/perft $(LDFLAGS) -lm

clean:
	rm -rf $(BUILD)

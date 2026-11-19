CC = gcc

CFLAGS = -O3 -Wall -Wextra -Iinclude
LDFLAGS =

RAYLIB_CFLAGS := $(shell pkg-config --cflags raylib)
RAYLIB_LIBS   := $(shell pkg-config --libs raylib)

CFLAGS += $(RAYLIB_CFLAGS) -pthread
LDLIBS += $(RAYLIB_LIBS) -pthread

BUILD = build

ENGINE_SRC = \
    src/engine/movegen.c \
    src/engine/utils.c \
    src/engine/zobrist.c

GAME_SRC = \
    src/game/game.c \
    src/game/pgn.c

GUI_SRC = \
    src/gui/gui.c

BOT_SRC = \
    src/bots/bot_registry.c \
    $(wildcard bots/*.c)

MAIN_SRC = src/main.c

ALL_SRC = $(ENGINE_SRC) $(GAME_SRC) $(GUI_SRC) $(BOT_SRC)

.PHONY: all clean perft

all: $(BUILD)/chessmatch

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/chessmatch: $(ALL_SRC) $(MAIN_SRC) | $(BUILD)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) $(LDLIBS)

perft: $(ENGINE_SRC) perft.c | $(BUILD)
	$(CC) $(CFLAGS) $^ -o $(BUILD)/perft $(LDFLAGS) -lm

clean:
	rm -rf $(BUILD)

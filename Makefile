CC      = gcc
CFLAGS  = -O2 -Wall -Wextra -std=gnu11
LDLIBS  = -lm

CORE_SRC = utils.c movegen.c zobrist.c bot_registry.c game.c main.c
BOT_SRC  = $(wildcard bots/*.c)

SRC = $(CORE_SRC) $(BOT_SRC)
OBJ = $(SRC:.c=.o)

BIN = chessmatch

.PHONY: all clean list

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ) $(LDLIBS)
	rm -f $(OBJ)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

list: $(BIN)
	./$(BIN) --list

clean:
	rm -f $(OBJ) $(BIN)

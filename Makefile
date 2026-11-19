CC ?= cc

TARGET := bebop

CPPFLAGS += -Isrc
CFLAGS ?= -O3 -std=c11 -Wall -Wextra
LDFLAGS ?=
LDLIBS += -pthread -lm

SOURCES := \
	src/main.c \
	src/uci.c \
	src/search.c \
	src/movegen.c \
	src/board.c \
	src/zobrist.c \
	src/perft.c
HEADERS := $(wildcard src/*.h)

.PHONY: all uci perft test mate-test clean

all: $(TARGET)
	@rm -f *.o *.d src/*.o src/*.d

$(TARGET): $(SOURCES) $(HEADERS)
	@rm -f *.o *.d src/*.o src/*.d
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(SOURCES) $(LDLIBS)
	@rm -f *.o *.d src/*.o src/*.d

uci: all
	./$(TARGET)

perft: all
	./tests/perft.sh

mate-test: all
	./tests/mate-in-1.sh

test: perft mate-test

clean:
	rm -f $(TARGET) *.o *.d src/*.o src/*.d

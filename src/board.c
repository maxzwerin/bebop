#include "board.h"

#include <stdlib.h>

void boardReset(Board *board) {
    board->whiteBB  = 0ULL;
    board->blackBB  = 0ULL;
    board->pawnBB   = 0ULL;
    board->knightBB = 0ULL;
    board->bishopBB = 0ULL;
    board->rookBB   = 0ULL;
    board->queenBB  = 0ULL;
    board->kingBB   = 0ULL;

    board->enp = -1;
    board->castle = 0;

    board->turn = 1;
    board->halfmove = 0;
    board->fullmove = 1;
}

void boardSetFen(Board *board, const char *fen) {
    boardReset(board);

    int rank = 7;
    int file = 0;

    while (*fen && *fen != ' ') {
        char c = *fen++;

        if (c == '/') {
            rank--;
            file = 0;
            continue;
        }

        if (c >= '1' && c <= '8') {
            file += c - '0';
            continue;
        }

        int square = rank * 8 + file;
        U64 mask = 1ULL << square;

        switch (c) {
            case 'P': board->pawnBB   |= mask; board->whiteBB |= mask; break;
            case 'N': board->knightBB |= mask; board->whiteBB |= mask; break;
            case 'B': board->bishopBB |= mask; board->whiteBB |= mask; break;
            case 'R': board->rookBB   |= mask; board->whiteBB |= mask; break;
            case 'Q': board->queenBB  |= mask; board->whiteBB |= mask; break;
            case 'K': board->kingBB   |= mask; board->whiteBB |= mask; break;

            case 'p': board->pawnBB   |= mask; board->blackBB |= mask; break;
            case 'n': board->knightBB |= mask; board->blackBB |= mask; break;
            case 'b': board->bishopBB |= mask; board->blackBB |= mask; break;
            case 'r': board->rookBB   |= mask; board->blackBB |= mask; break;
            case 'q': board->queenBB  |= mask; board->blackBB |= mask; break;
            case 'k': board->kingBB   |= mask; board->blackBB |= mask; break;
        }

        file++;
    }

    fen++;

    board->turn = (*fen == 'w') ? 1 : 0;
    fen += 2;

    board->castle = 0;
    if (*fen != '-') {
        while (*fen && *fen != ' ') {
            switch (*fen++) {
                case 'K': board->castle |= K; break;
                case 'Q': board->castle |= Q; break;
                case 'k': board->castle |= k; break;
                case 'q': board->castle |= q; break;
            }
        }
    } else {
        fen++;
    }
    fen++;

    if (*fen != '-') {
        int epFile = fen[0] - 'a';
        int epRank = fen[1] - '1';
        board->enp = epRank * 8 + epFile;
        fen += 2;
    } else {
        board->enp = -1;
        fen++;
    }

    if (*fen == ' ') fen++;
    board->halfmove = atoi(fen);

    while (*fen && *fen != ' ') fen++;

    if (*fen == ' ') fen++;
    board->fullmove = atoi(fen);
}

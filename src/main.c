#include "typedefs.h"
#include "uci.h"

static int pieceTypeAt(const Board *board, int square) {
    U64 bit = BIT(square);
    if (board->pawnBB   & bit) return PAWN;
    if (board->knightBB & bit) return KNIGHT;
    if (board->bishopBB & bit) return BISHOP;
    if (board->rookBB   & bit) return ROOK;
    if (board->queenBB  & bit) return QUEEN;
    if (board->kingBB   & bit) return KING;
    return -1;
}

static char promotionSuffix(int flag) {
    if (!IS_PROMO(flag)) return '\0';

    switch (PROMO_PT(flag)) {
        case QUEEN:  return 'q';
        case ROOK:   return 'r';
        case BISHOP: return 'b';
        case KNIGHT: return 'n';
        default:     return '\0';
    }
}

static void moveToUci(Move move, char out[6]) {
    int from = EXTRACT_FROM(move);
    int to = EXTRACT_TO(move);
    int flag = EXTRACT_FLAG(move);

    out[0] = (char)('a' + (from & 7));
    out[1] = (char)('1' + (from >> 3));
    out[2] = (char)('a' + (to & 7));
    out[3] = (char)('1' + (to >> 3));

    char suffix = promotionSuffix(flag);
    if (suffix) {
        out[4] = suffix;
        out[5] = '\0';
    } else {
        out[4] = '\0';
    }
}

int main(int argc, char **argv) {
    return runUci();
}

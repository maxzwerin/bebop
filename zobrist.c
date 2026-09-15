#include "zobrist.h"

/* Zobrist hashing, used only for threefold-repetition detection.
 * Piece indexing follows enum PIECES in typedefs.h (0-11: PAWN_B..KING_W). */
static U64 ZOBRIST_PIECES[12][64];
static U64 ZOBRIST_CASTLE[16];
static U64 ZOBRIST_ENP[8]; /* by file */
static U64 ZOBRIST_TURN;

static U64 zobristRandom(void) {
    /* splitmix64 */
    static U64 x = 0x243F6A8885A308D3ULL;
    x += 0x9E3779B97F4A7C15ULL;
    U64 z = x;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

void initZobrist(void) {
    for (int p = 0; p < 12; p++)
        for (int sq = 0; sq < 64; sq++)
            ZOBRIST_PIECES[p][sq] = zobristRandom();

    for (int i = 0; i < 16; i++) ZOBRIST_CASTLE[i] = zobristRandom();
    for (int i = 0; i < 8; i++) ZOBRIST_ENP[i] = zobristRandom();
    ZOBRIST_TURN = zobristRandom();
}

U64 zobristHash(Board *b) {
    U64 h = 0ULL;

    U64 *bbs[6] = { &b->pawnBB, &b->knightBB, &b->bishopBB, &b->rookBB, &b->queenBB, &b->kingBB };

    for (int p = 0; p < 6; p++) {
        U64 white = *bbs[p] & b->whiteBB;
        U64 black = *bbs[p] & b->blackBB;

        while (white) { int sq = POP_LSB(white); h ^= ZOBRIST_PIECES[p + 6][sq]; }
        while (black) { int sq = POP_LSB(black); h ^= ZOBRIST_PIECES[p][sq]; }
    }

    h ^= ZOBRIST_CASTLE[b->castle & 0xF];
    if (b->enp >= 0) h ^= ZOBRIST_ENP[b->enp % 8];
    if (b->turn) h ^= ZOBRIST_TURN;

    return h;
}

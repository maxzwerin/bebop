#include "movegen.h"

U64 KNIGHT_REF[64];
U64 KING_REF[64];
U64 ROOK_REF[64];
U64 BISHOP_REF[64];
U64 PAWN_ATTACKS[2][64];
static U64 BISHOP_ATTACKS[64][512];
static U64 ROOK_ATTACKS[64][4096];

static int ROOK_RELEVANT_BITS[64] = {
    12, 11, 11, 11, 11, 11, 11, 12,
    11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11,
    12, 11, 11, 11, 11, 11, 11, 12
};
static int BISHOP_RELEVANT_BITS[64] = {
    6, 5, 5, 5, 5, 5, 5, 6,
    5, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 7, 7, 7, 7, 5, 5,
    5, 5, 7, 9, 9, 7, 5, 5,
    5, 5, 7, 9, 9, 7, 5, 5,
    5, 5, 7, 7, 7, 7, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 5,
    6, 5, 5, 5, 5, 5, 5, 6
};

static U64 ROOK_MAGICS[64] = {
    0xa8002c000108020ULL,  0x6c00049b0002001ULL,  0x100200010090040ULL,
    0x2480041000800801ULL, 0x280028004000800ULL,  0x900410008040022ULL,
    0x280020001001080ULL,  0x2880002041000080ULL, 0xa000800080400034ULL,
    0x4808020004000ULL,    0x2290802004801000ULL, 0x411000d00100020ULL,
    0x402800800040080ULL,  0xb000401004208ULL,    0x2409000100040200ULL,
    0x1002100004082ULL,    0x22878001e24000ULL,   0x1090810021004010ULL,
    0x801030040200012ULL,  0x500808008001000ULL,  0xa08018014000880ULL,
    0x8000808004000200ULL, 0x201008080010200ULL,  0x801020000441091ULL,
    0x800080204005ULL,     0x1040200040100048ULL, 0x120200402082ULL,
    0xd14880480100080ULL,  0x12040280080080ULL,   0x100040080020080ULL,
    0x9020010080800200ULL, 0x813241200148449ULL,  0x491604001800080ULL,
    0x100401000402001ULL,  0x4820010021001040ULL, 0x400402202000812ULL,
    0x209009005000802ULL,  0x810800601800400ULL,  0x4301083214000150ULL,
    0x204026458e001401ULL, 0x40204000808000ULL,   0x8001008040010020ULL,
    0x8410820820420010ULL, 0x1003001000090020ULL, 0x804040008008080ULL,
    0x12000810020004ULL,   0x1000100200040208ULL, 0x430000a044020001ULL,
    0x280009023410300ULL,  0xe0100040002240ULL,   0x200100401700ULL,
    0x2244100408008080ULL, 0x8000400801980ULL,    0x2000810040200ULL,
    0x8010100228810400ULL, 0x2000009044210200ULL, 0x4080008040102101ULL,
    0x40002080411d01ULL,   0x2005524060000901ULL, 0x502001008400422ULL,
    0x489a000810200402ULL, 0x1004400080a13ULL,    0x4000011008020084ULL,
    0x26002114058042ULL,
};

static U64 BISHOP_MAGICS[64] = {
    0x89a1121896040240ULL, 0x2004844802002010ULL, 0x2068080051921000ULL,
    0x62880a0220200808ULL, 0x4042004000000ULL,    0x100822020200011ULL,
    0xc00444222012000aULL, 0x28808801216001ULL,   0x400492088408100ULL,
    0x201c401040c0084ULL,  0x840800910a0010ULL,   0x82080240060ULL,
    0x2000840504006000ULL, 0x30010c4108405004ULL, 0x1008005410080802ULL,
    0x8144042209100900ULL, 0x208081020014400ULL,  0x4800201208ca00ULL,
    0xf18140408012008ULL,  0x1004002802102001ULL, 0x841000820080811ULL,
    0x40200200a42008ULL,   0x800054042000ULL,     0x88010400410c9000ULL,
    0x520040470104290ULL,  0x1004040051500081ULL, 0x2002081833080021ULL,
    0x400c00c010142ULL,    0x941408200c002000ULL, 0x658810000806011ULL,
    0x188071040440a00ULL,  0x4800404002011c00ULL, 0x104442040404200ULL,
    0x511080202091021ULL,  0x4022401120400ULL,    0x80c0040400080120ULL,
    0x8040010040820802ULL, 0x480810700020090ULL,  0x102008e00040242ULL,
    0x809005202050100ULL,  0x8002024220104080ULL, 0x431008804142000ULL,
    0x19001802081400ULL,   0x200014208040080ULL,  0x3308082008200100ULL,
    0x41010500040c020ULL,  0x4012020c04210308ULL, 0x208220a202004080ULL,
    0x111040120082000ULL,  0x6803040141280a00ULL, 0x2101004202410000ULL,
    0x8200000041108022ULL, 0x21082088000ULL,      0x2410204010040ULL,
    0x40100400809000ULL,   0x822088220820214ULL,  0x40808090012004ULL,
    0x910224040218c9ULL,   0x402814422015008ULL,  0x90014004842410ULL,
    0x1000042304105ULL,    0x10008830412a00ULL,   0x2520081090008908ULL,
    0x40102000a0a60140ULL,
};

static U64 occupancyMask(int index, int bits, U64 attackMask) {
    U64 occ = 0ULL;

    for (int i = 0; i < bits; i++) {
        int square = LSB(attackMask);
        attackMask &= attackMask - 1;

        if (index & (1 << i)) occ |= BIT(square);
    }

    return occ;
}

static U64 rayAttacks(int square, int dr, int df, U64 block) {
    U64 attacks = 0ULL;

    int r = square / 8;
    int f = square % 8;

    r += dr; f += df;

    while (r >= 0 && r <= 7 && f >= 0 && f <= 7) {
        int sq = r * 8 + f;
        U64 bit = BIT(sq);

        attacks |= bit;
        if (block & bit) break;

        r += dr; f += df;
    }

    return attacks;
}

static U64 bishopAttacksOnTheFly(int square, U64 block) {
    return rayAttacks(square,  1,  1, block) | rayAttacks(square,  1, -1, block) | 
           rayAttacks(square, -1,  1, block) | rayAttacks(square, -1, -1, block);
}

static U64 rookAttacksOnTheFly(int square, U64 block) {
    return rayAttacks(square,  1,  0, block) | rayAttacks(square, -1,  0, block) |
           rayAttacks(square,  0,  1, block) | rayAttacks(square,  0, -1, block);
}

static U64 bishopMovement(int square) {
    int f, r;
    int tf = square % 8;
    int tr = square / 8;

    U64 movement = 0ULL;
    for (r = tr + 1, f = tf + 1; r <= 6 && f <= 6; r++, f++) movement |= BIT(r * 8 + f);
    for (r = tr + 1, f = tf - 1; r <= 6 && f >= 1; r++, f--) movement |= BIT(r * 8 + f);
    for (r = tr - 1, f = tf + 1; r >= 1 && f <= 6; r--, f++) movement |= BIT(r * 8 + f);
    for (r = tr - 1, f = tf - 1; r >= 1 && f >= 1; r--, f--) movement |= BIT(r * 8 + f);

    return movement;
}

static U64 rookMovement(int square) {
    int f, r;
    int tf = square % 8;
    int tr = square / 8;
    U64 movement = 0ULL;

    for (r = tr + 1; r <= 6; r++) movement |= BIT(r * 8 + tf);
    for (r = tr - 1; r >= 1; r--) movement |= BIT(r * 8 + tf);
    for (f = tf + 1; f <= 6; f++) movement |= BIT(tr * 8 + f);
    for (f = tf - 1; f >= 1; f--) movement |= BIT(tr * 8 + f);

    return movement;
}

static void initSliders() {
    for (int square = 0; square < 64; square++) {
        BISHOP_REF[square] = bishopMovement(square);
        ROOK_REF[square] = rookMovement(square);

        U64 bishopMask = BISHOP_REF[square];
        U64 rookMask = ROOK_REF[square];
        int bishopRelevantBits = BISHOP_RELEVANT_BITS[square];
        int rookRelevantBits = ROOK_RELEVANT_BITS[square];
        int bishopOccupancyVariations = 1 << bishopRelevantBits;
        int rookOccupancyVariations = 1 << rookRelevantBits;

        for (int i = 0; i < bishopOccupancyVariations; i++) {
            U64 occ = occupancyMask(i, bishopRelevantBits, bishopMask);
            U64 magic_id = occ * BISHOP_MAGICS[square] >> (64 - bishopRelevantBits);
            BISHOP_ATTACKS[square][magic_id] = bishopAttacksOnTheFly(square, occ);
        }

        for (int i = 0; i < rookOccupancyVariations; i++) {
            U64 occ = occupancyMask(i, rookRelevantBits, rookMask);
            U64 magic_id = occ * ROOK_MAGICS[square] >> (64 - rookRelevantBits);
            ROOK_ATTACKS[square][magic_id] = rookAttacksOnTheFly(square, occ);
        }
    }
}

static void initPawnAttacks() {
    for (int sq = 0; sq < 64; sq++) {
        U64 bb = BIT(sq);

        PAWN_ATTACKS[1][sq] = ((bb << 7) & ~FILE_H) | ((bb << 9) & ~FILE_A);
        PAWN_ATTACKS[0][sq] = ((bb >> 7) & ~FILE_A) | ((bb >> 9) & ~FILE_H);
    }
}

U64 getBishopAttacks(int square, U64 occupancy) {
    occupancy &= BISHOP_REF[square];
    occupancy *=  BISHOP_MAGICS[square];
    occupancy >>= 64 - BISHOP_RELEVANT_BITS[square];
    return BISHOP_ATTACKS[square][occupancy];
}

U64 getRookAttacks(int square, U64 occ) {
    occ &= ROOK_REF[square];
    occ *=  ROOK_MAGICS[square];
    occ >>= 64 - ROOK_RELEVANT_BITS[square];
    return ROOK_ATTACKS[square][occ];
}

U64 getKnightAttacks(int sq) {
    U64 knight = BIT(sq);
    U64 FILE_AB = FILE_A | FILE_B;
    U64 FILE_GH = FILE_G | FILE_H;
    U64 attacks = ((knight >> 15) & ~FILE_A)  | ((knight << 15) & ~FILE_H)  |
                  ((knight << 10) & ~FILE_AB) | ((knight >> 10) & ~FILE_GH) |
                  ((knight << 17) & ~FILE_A)  | ((knight >> 17) & ~FILE_H)  |
                  ((knight << 6)  & ~FILE_GH) | ((knight >> 6)  & ~FILE_AB);

    return attacks;
}

U64 getKingAttacks(int sq) {
    U64 king = BIT(sq);
    U64 attacks = ((king >> 1) & ~FILE_H) | ((king << 1) & ~FILE_A) | 
                  ((king >> 7) & ~FILE_A) | ((king >> 9) & ~FILE_H) |
                  ((king << 7) & ~FILE_H) | ((king << 9) & ~FILE_A) |
                  (king >> 8) | (king << 8);

    return attacks;
}

U64 getPawnMoves(int square, Board *board, int color) {
    U64 pawn = BIT(square);
    U64 friendly = color ? board->whiteBB : board->blackBB;
    U64 occ = board->whiteBB | board->blackBB;

    U64 start_rank = color ? RANK_2 : RANK_7;

    U64 single = 0ULL;
    U64 dbl = 0ULL;
    U64 attacks = 0ULL;
    U64 enp = 0ULL;

    single = color ? pawn << 8 : pawn >> 8;
    single &= ~occ;

    if (pawn & start_rank && single) {
        dbl = color ? pawn << 16 : pawn >> 16;
        dbl &= ~occ;
    }

    if (board->enp >= 0) enp = BIT(board->enp);

    attacks = (PAWN_ATTACKS[color][square] & (occ | enp));

    attacks = attacks | single | dbl;
    return attacks & ~friendly;
}

static void initKnightMovementTable(void) {
    for (int sq = 0; sq < 64; sq++) KNIGHT_REF[sq] = getKnightAttacks(sq);
}

static void initKingMovementTable(void) {
    for (int sq = 0; sq < 64; sq++) KING_REF[sq] = getKingAttacks(sq);
}

/* ---------------------------------------------------------------------
 * BETWEEN[a][b]: squares strictly between a and b (exclusive), if they
 * lie on a common rank/file/diagonal - else 0.
 * LINE[a][b]:    the *entire* rank/file/diagonal through a and b,
 *                edge to edge (inclusive of both) - else 0.
 * Used by the checkers/pins-based legal move generator below so it
 * doesn't need make/unmake to find blocking squares or pin rays.
 * ------------------------------------------------------------------- */
static U64 BETWEEN[64][64];
static U64 LINE[64][64];

static const int LINE_DR[8] = {  1, -1,  0,  0,  1,  1, -1, -1 };
static const int LINE_DF[8] = {  0,  0,  1, -1,  1, -1,  1, -1 };

static void initLines(void) {
    for (int a = 0; a < 64; a++) {
        for (int dir = 0; dir < 8; dir++) {
            U64 fullRay = rayAttacks(a, LINE_DR[dir], LINE_DF[dir], 0ULL);
            U64 ray = fullRay;
            while (ray) {
                int b = POP_LSB(ray);
                BETWEEN[a][b] = rayAttacks(a, LINE_DR[dir], LINE_DF[dir], BIT(b)) & ~BIT(b);
                U64 backRay = rayAttacks(a, -LINE_DR[dir], -LINE_DF[dir], 0ULL);
                LINE[a][b] = fullRay | backRay | BIT(a);
            }
        }
    }
}

void initMoveGen() {
    initKnightMovementTable();
    initKingMovementTable();
    initPawnAttacks();
    initSliders();
    initLines();
}

bool isSquareAttackedWithOcc(Board *b, int sq, int attacker, U64 occ) {
    U64 atkBB    = attacker ? b->whiteBB : b->blackBB;
    U64 pawnBB   = b->pawnBB   & atkBB;
    U64 knightBB = b->knightBB & atkBB;
    U64 bishopBB = b->bishopBB & atkBB;
    U64 rookBB   = b->rookBB   & atkBB;
    U64 queenBB  = b->queenBB  & atkBB;
    U64 kingBB   = b->kingBB   & atkBB;

    if (PAWN_ATTACKS[!attacker][sq] & pawnBB) return true;
    if (KNIGHT_REF[sq] & knightBB) return true;
    if (KING_REF[sq] & kingBB) return true;
    if (getBishopAttacks(sq, occ) & (bishopBB | queenBB)) return true;
    if (getRookAttacks(sq, occ) & (rookBB | queenBB)) return true;

    return false;
}

bool isSquareAttacked(Board *b, int sq, int attacker) {
    return isSquareAttackedWithOcc(b, sq, attacker, b->whiteBB | b->blackBB);
}

void clearCastleRights(Board *board, int clear) {
    board->castle &= ~clear;
}

void doMove(Board *board, Move move, Undo *undo) {
    int from  = EXTRACT_FROM(move);
    int to    = EXTRACT_TO(move);
    int piece = EXTRACT_PIECE(move);
    int flag  = EXTRACT_FLAG(move);

    int turn = board->turn;
    U64 fromMask = BIT(from);
    int pieceAgnostic = piece % 6;

    int isEnp = (to == board->enp) && (pieceAgnostic == PAWN);

    undo->capture = -1;
    undo->enp = board->enp;
    undo->castle = board->castle;

    /* en passant management */
    if (isEnp) {
        int toSq = board->enp;
        int capturedSq = board->enp + (board->turn ? -8 : 8);
        U64 toMask = BIT(toSq);
        U64 capMask = BIT(capturedSq);

        board->pawnBB ^= fromMask | toMask | capMask;
        if (turn) {
            board->whiteBB ^= fromMask | toMask;
            board->blackBB ^= capMask;
        } else {
            board->blackBB ^= fromMask | toMask;
            board->whiteBB ^= capMask;
        }

        board->enp = -1;
        SWITCH_SIDE(board);
        return;
    }

    /* clear en passant sq */
    board->enp = -1;

    /* clear castle rights if king / rook moves */
    /* check if rook was captured later */
    if (board->castle) {
        if      (piece == KING_W) clearCastleRights(board, KQ);
        else if (piece == KING_B) clearCastleRights(board, kq);

        if      (piece == ROOK_W && from == H1) clearCastleRights(board, K);
        else if (piece == ROOK_W && from == A1) clearCastleRights(board, Q);

        if      (piece == ROOK_B && from == H8) clearCastleRights(board, k);
        else if (piece == ROOK_B && from == A8) clearCastleRights(board, q);
    }

    /* deal with castling */
    if (IS_CASTLE(flag)) {
        if (piece == K) {
            board->kingBB  &= ~BIT(E1); board->kingBB  |= BIT(G1);
            board->whiteBB &= ~BIT(E1); board->whiteBB |= BIT(G1);
            board->rookBB  &= ~BIT(H1); board->rookBB  |= BIT(F1);
            board->whiteBB &= ~BIT(H1); board->whiteBB |= BIT(F1);
        } else if (piece == Q) {
            board->kingBB  &= ~BIT(E1); board->kingBB  |= BIT(C1);
            board->whiteBB &= ~BIT(E1); board->whiteBB |= BIT(C1);
            board->rookBB  &= ~BIT(A1); board->rookBB  |= BIT(D1);
            board->whiteBB &= ~BIT(A1); board->whiteBB |= BIT(D1);
        } else if (piece == k) {
            board->kingBB  &= ~BIT(E8); board->kingBB  |= BIT(G8);
            board->blackBB &= ~BIT(E8); board->blackBB |= BIT(G8);
            board->rookBB  &= ~BIT(H8); board->rookBB  |= BIT(F8);
            board->blackBB &= ~BIT(H8); board->blackBB |= BIT(F8);
        } else if (piece == q) {
            board->kingBB  &= ~BIT(E8); board->kingBB  |= BIT(C8);
            board->blackBB &= ~BIT(E8); board->blackBB |= BIT(C8);
            board->rookBB  &= ~BIT(A8); board->rookBB  |= BIT(D8);
            board->blackBB &= ~BIT(A8); board->blackBB |= BIT(D8);
        }

        /* clear castle rights for color and return */
        if (turn) clearCastleRights(board, KQ);
        else clearCastleRights(board, kq);

        SWITCH_SIDE(board);
        return;
    }

    U64 *pieceThatMovedBB = (&board->pawnBB + piece % 6);
    U64 *curColorBB = turn ? &board->whiteBB : &board->blackBB;
    U64 *oppColorBB = turn ? &board->blackBB : &board->whiteBB;

    U64 toMask = BIT(to);

    /* pick up piece, wait to put down*/
    *pieceThatMovedBB ^= BIT(from); *curColorBB ^= BIT(from);

    /* check if piece has been captured */
    if (*oppColorBB & toMask) {
        *oppColorBB ^= toMask;

        for (int p = 0; p < 6; p++) {
            U64 *bb = (&board->pawnBB + p);
            if (*bb & toMask) {
                *bb ^= toMask; /* remove from piece BB */
                undo->capture = p;

                /* now check if rook was captured from starting sq.
                 * if so, clear its castle rights */
                if (p == ROOK && to == H1) clearCastleRights(board, K);
                if (p == ROOK && to == A1) clearCastleRights(board, Q);
                if (p == ROOK && to == H8) clearCastleRights(board, k);
                if (p == ROOK && to == A8) clearCastleRights(board, q);

                break;
            }
        }
    }

    /* check if promotion / put down piece */
    if (IS_PROMO(flag)) {
        U64 promoMask = BIT(to);
        U64 *promoBB = (&board->pawnBB + PROMO_PT(flag));
        *promoBB |= promoMask;
        *curColorBB |= promoMask;
    } else {
        *pieceThatMovedBB |= BIT(to); *curColorBB |= BIT(to);
    }

    /* set en passant if double move from pawns starting rank */
    U64 startRank = turn ? RANK_2 : RANK_7;
    if (pieceAgnostic == PAWN && (fromMask & startRank)) {
        if ((from ^ to) == 16) {
            board->enp = turn ? to - 8 : to + 8;
        }
    }

    SWITCH_SIDE(board);
}

void undoMove(Board *board, Move move, Undo *undo) {
    int from  = EXTRACT_FROM(move);
    int to    = EXTRACT_TO(move);
    int piece = EXTRACT_PIECE(move);
    int flag  = EXTRACT_FLAG(move);

    int turn = !board->turn;
    int pieceAgnostic = piece % 6;

    U64 fromMask = BIT(from);
    U64 toMask   = BIT(to);

    U64 *pieceBB    = (&board->pawnBB + pieceAgnostic);
    U64 *curColorBB = turn ? &board->whiteBB : &board->blackBB;
    // U64 *oppColorBB = turn ? &board->blackBB : &board->whiteBB;

    board->castle = undo->castle;
    board->enp = undo->enp;

    SWITCH_SIDE(board);

    if ((pieceAgnostic == PAWN) && (to == undo->enp)) {
        int capSq = to + (turn ? -8 : 8);
        U64 capMask = BIT(capSq);

        *pieceBB ^= fromMask | toMask;
        *curColorBB ^= fromMask | toMask;

        board->pawnBB |= capMask;
        if (turn) board->blackBB |= capMask;
        else board->whiteBB |= capMask;

        return;
    }

    if (IS_CASTLE(flag)) {
        if (piece == K) {
            board->kingBB  &= ~BIT(G1); board->kingBB |= BIT(E1);
            board->rookBB  &= ~BIT(F1); board->rookBB |= BIT(H1);
            board->whiteBB &= ~(BIT(F1) | BIT(G1));
            board->whiteBB |= BIT(E1) | BIT(H1);
        } else if (piece == Q) {
            board->kingBB  &= ~BIT(C1); board->kingBB |= BIT(E1);
            board->rookBB  &= ~BIT(D1); board->rookBB |= BIT(A1);
            board->whiteBB &= ~(BIT(C1) | BIT(D1));
            board->whiteBB |= BIT(E1) | BIT(A1);
        } else if (piece == k) {
            board->kingBB  &= ~BIT(G8); board->kingBB  |= BIT(E8);
            board->rookBB  &= ~BIT(F8); board->rookBB  |= BIT(H8);
            board->blackBB &= ~(BIT(F8) | BIT(G8));
            board->blackBB |= BIT(E8) | BIT(H8);
        } else if (piece == q) {
            board->kingBB  &= ~BIT(C8); board->kingBB  |= BIT(E8);
            board->rookBB  &= ~BIT(D8); board->rookBB  |= BIT(A8);
            board->blackBB &= ~(BIT(C8) | BIT (D8));
            board->blackBB |= BIT(E8) | BIT(A8);
        }
        return;
    }

    if (IS_PROMO(flag)) {
        int promoPiece = PROMO_PT(flag);
        U64 *promoBB = &board->pawnBB + promoPiece;

        *promoBB &= ~toMask;
        *curColorBB &= ~toMask;

        *pieceBB |= fromMask;
        *curColorBB |= fromMask;
    } else {
        *pieceBB ^= fromMask | toMask;
        *curColorBB ^= fromMask | toMask;
    }

    if (undo->capture != -1) {
        int capPiece = undo->capture;
        U64 *capBB = &board->pawnBB + capPiece;
        *capBB |= toMask;

        if (turn) board->blackBB |= toMask;
        else board->whiteBB |= toMask;
    }
}

bool canCastle(Board *board, int right, U64 occ) {
    if (!(board->castle & right)) return false;

    if ((right == K || right == Q) && !board->turn) return false;
    if ((right == k || right == q) && board->turn) return false;

    switch (right) {
        case K: if (occ & (BIT(F1) | BIT(G1)))           return false; break;
        case Q: if (occ & (BIT(B1) | BIT(C1) | BIT(D1))) return false; break;
        case k: if (occ & (BIT(F8) | BIT(G8)))           return false; break;
        case q: if (occ & (BIT(B8) | BIT(C8) | BIT(D8))) return false; break;
        default: return false;
    }

    int kingSq = board->turn ? E1 : E8;

    /* king cannot be in check */
    if (isSquareAttacked(board, kingSq, !board->turn)) return false;

    int path1, path2;

    switch (right) {
        case K: path1 = F1; path2 = G1; break;
        case Q: path1 = D1; path2 = C1; break;
        case k: path1 = F8; path2 = G8; break;
        case q: path1 = D8; path2 = C8; break;
        default: return false;
    }

    if (isSquareAttacked(board, path1, !board->turn)) return false;
    if (isSquareAttacked(board, path2, !board->turn)) return false;

    return true;
}

int pseudoMoves(Board *board, Move moves[]) {
    int length = 0;

    int turn = board->turn;
    U64 pawnBB   = board->pawnBB   & (turn ? board->whiteBB : board->blackBB);
    U64 knightBB = board->knightBB & (turn ? board->whiteBB : board->blackBB);
    U64 rookBB   = board->rookBB   & (turn ? board->whiteBB : board->blackBB);
    U64 bishopBB = board->bishopBB & (turn ? board->whiteBB : board->blackBB);
    U64 queenBB  = board->queenBB  & (turn ? board->whiteBB : board->blackBB);
    U64 kingBB   = board->kingBB   & (turn ? board->whiteBB : board->blackBB);

    U64 friendly  = turn ? board->whiteBB : board->blackBB;
    // U64 enemy = turn ? board->blackBB : board->whiteBB;
    U64 occupancy = board->whiteBB | board->blackBB;
    int kingSq = LSB(kingBB);

    U64 attackMask = 0ULL;

    while (pawnBB) {
        int sq = POP_LSB(pawnBB);
        attackMask = getPawnMoves(sq, board, turn);

        while (attackMask) {
            int to = POP_LSB(attackMask);
            U64 toBB = BIT(to);
            U64 isPromoting = turn ? (toBB & RANK_8) : (toBB & RANK_1);

            if (isPromoting) {
                moves[length++] = ENCODE_MOVE(sq, to, PAWN+turn*6, QUEEN_PROMO_FLAG);
                moves[length++] = ENCODE_MOVE(sq, to, PAWN+turn*6, KNIGHT_PROMO_FLAG);
                moves[length++] = ENCODE_MOVE(sq, to, PAWN+turn*6, ROOK_PROMO_FLAG);
                moves[length++] = ENCODE_MOVE(sq, to, PAWN+turn*6, BISHOP_PROMO_FLAG);
            } else {
                moves[length++] = ENCODE_MOVE(sq, to, PAWN+turn*6, EMPTY_FLAG);
            }
        }
    }
    while (knightBB) {
        int sq = POP_LSB(knightBB);
        attackMask = KNIGHT_REF[sq] & ~friendly;
        while (attackMask) {
            int to = POP_LSB(attackMask);
            moves[length++] = ENCODE_MOVE(sq, to, KNIGHT+turn*6, EMPTY_FLAG);
        }
    }
    while (rookBB) {
        int sq = POP_LSB(rookBB);
        attackMask = getRookAttacks(sq, occupancy) & ~friendly;
        while (attackMask) {
            int to = POP_LSB(attackMask);
            moves[length++] = ENCODE_MOVE(sq, to, ROOK+turn*6, EMPTY_FLAG);
        }
    }
    while (bishopBB) {
        int sq = POP_LSB(bishopBB);
        attackMask = getBishopAttacks(sq, occupancy) & ~friendly;
        while (attackMask) {
            int to = POP_LSB(attackMask);
            moves[length++] = ENCODE_MOVE(sq, to, BISHOP+turn*6, EMPTY_FLAG);
        }
    }
    while (queenBB) {
        int sq = POP_LSB(queenBB);
        attackMask = (getRookAttacks(sq, occupancy) | getBishopAttacks(sq, occupancy)) & ~friendly;
        while (attackMask) {
            int to = POP_LSB(attackMask);
            moves[length++] = ENCODE_MOVE(sq, to, QUEEN+turn*6, EMPTY_FLAG);
        }
    }

    attackMask = KING_REF[kingSq] & ~friendly;
    while (attackMask) {
        int to = POP_LSB(attackMask);
        moves[length++] = ENCODE_MOVE(kingSq, to, KING+turn*6, EMPTY_FLAG);
    }

    int right[4] = { K, Q, k, q };
    int moveTo[4] = { G1, C1, G8, C8 };
    for (int i = 0; i < 4; i++) {
        if (canCastle(board, right[i], occupancy)) {
            moves[length++] = ENCODE_MOVE(kingSq, moveTo[i], right[i], CASTLE_FLAG);
        }
    }

    return length;
}

int legalMovesSlow(Board *board, Move legal[]) {
    Move pseudo[MAX_MOVES];
    int count = pseudoMoves(board, pseudo);

    int legalCount = 0;

    for (int i = 0; i < count; i++) {
        Undo undo;

        doMove(board, pseudo[i], &undo);

        U64 kingBB = board->kingBB & (board->turn ? board->blackBB : board->whiteBB);

        if (!kingBB) continue;
        int kingSq = LSB(kingBB);

        if (!isSquareAttacked(board, kingSq, board->turn)) legal[legalCount++] = pseudo[i];

        undoMove(board, pseudo[i], &undo);
    }

    return legalCount;
}

/* ---------------------------------------------------------------------
 * Fast legal move generation: computes checkers and pinned pieces once
 * (both O(1) via magic bitboards + BETWEEN/LINE lookups), then builds
 * moves directly instead of generating everything and testing each one
 * with a doMove/isSquareAttacked/undoMove cycle. That old approach is
 * kept above as legalMovesSlow for reference/benchmarking.
 *
 * The one case that still needs a make/unmake check is en passant: a
 * capture can expose the king along a rank even when neither pawn was
 * individually pinned (both blockers between king and rook vanish at
 * once). That's rare enough that a full check there costs nothing.
 * ------------------------------------------------------------------- */

static U64 computeCheckers(Board *b, int kingSq, int us, U64 occ) {
    U64 enemy = us ? b->blackBB : b->whiteBB;
    U64 checkers = 0ULL;

    checkers |= PAWN_ATTACKS[us][kingSq] & b->pawnBB & enemy;
    checkers |= KNIGHT_REF[kingSq] & b->knightBB & enemy;
    checkers |= getBishopAttacks(kingSq, occ) & (b->bishopBB | b->queenBB) & enemy;
    checkers |= getRookAttacks(kingSq, occ)   & (b->rookBB   | b->queenBB) & enemy;

    return checkers;
}

static U64 computePinned(Board *b, int kingSq, int us, U64 occ) {
    U64 own   = us ? b->whiteBB : b->blackBB;
    U64 enemy = us ? b->blackBB : b->whiteBB;
    U64 enemyDiag = enemy & (b->bishopBB | b->queenBB);
    U64 enemyOrth = enemy & (b->rookBB   | b->queenBB);

    U64 pinned = 0ULL;

    U64 diagFromKing = getBishopAttacks(kingSq, occ);
    U64 possibleDiagBlockers = diagFromKing & own;
    U64 xrayDiag = getBishopAttacks(kingSq, occ ^ possibleDiagBlockers) & enemyDiag & ~diagFromKing;
    while (xrayDiag) {
        int pinnerSq = POP_LSB(xrayDiag);
        pinned |= BETWEEN[kingSq][pinnerSq] & own;
    }

    U64 orthFromKing = getRookAttacks(kingSq, occ);
    U64 possibleOrthBlockers = orthFromKing & own;
    U64 xrayOrth = getRookAttacks(kingSq, occ ^ possibleOrthBlockers) & enemyOrth & ~orthFromKing;
    while (xrayOrth) {
        int pinnerSq = POP_LSB(xrayOrth);
        pinned |= BETWEEN[kingSq][pinnerSq] & own;
    }

    return pinned;
}

int legalMoves(Board *board, Move legal[]) {
    int length = 0;
    int turn = board->turn;
    U64 own   = turn ? board->whiteBB : board->blackBB;
    U64 enemy = turn ? board->blackBB : board->whiteBB;
    U64 occ   = own | enemy;
    int kingSq = LSB(board->kingBB & own);

    U64 checkers = computeCheckers(board, kingSq, turn, occ);
    int numCheckers = BITS(checkers);

    /* King moves: always generated. Use occupancy with the king removed
     * so a slider "sees through" the square the king is vacating -
     * otherwise the king could illegally step straight back along a
     * checking ray. */
    U64 occNoKing = occ & ~BIT(kingSq);
    U64 kingMoves = KING_REF[kingSq] & ~own;
    while (kingMoves) {
        int to = POP_LSB(kingMoves);
        if (!isSquareAttackedWithOcc(board, to, !turn, occNoKing))
            legal[length++] = ENCODE_MOVE(kingSq, to, KING+turn*6, EMPTY_FLAG);
    }

    if (numCheckers >= 2) return length; /* double check: only the king can move */

    U64 checkMask = ~0ULL;
    if (numCheckers == 1) {
        int checkerSq = LSB(checkers);
        checkMask = BETWEEN[kingSq][checkerSq] | BIT(checkerSq);
    }

    U64 pinned = computePinned(board, kingSq, turn, occ);

    /* Pawns */
    U64 pawnBB = board->pawnBB & own;
    while (pawnBB) {
        int sq = POP_LSB(pawnBB);
        U64 rawMoves = getPawnMoves(sq, board, turn);

        /* En passant is special-cased *before* checkMask/pin filtering:
         * capturing en passant removes the checking/pinning pawn from
         * its actual square, but the move's "to" square is the empty
         * square behind it - which checkMask (built from the checker's
         * own square) and a file/rank pin ray wouldn't recognize as
         * resolving anything. So this one move type always gets the
         * full make/unmake legality check instead of the fast masks. */
        if (board->enp >= 0 && (rawMoves & BIT(board->enp))) {
            Move m = ENCODE_MOVE(sq, board->enp, PAWN+turn*6, EMPTY_FLAG);
            Undo undo;
            doMove(board, m, &undo);
            U64 kbb = board->kingBB & (board->turn ? board->blackBB : board->whiteBB);
            bool safe = !isSquareAttacked(board, LSB(kbb), board->turn);
            undoMove(board, m, &undo);
            if (safe) legal[length++] = m;

            rawMoves &= ~BIT(board->enp); /* don't also handle it below */
        }

        U64 allowed = checkMask;
        if (pinned & BIT(sq)) allowed &= LINE[kingSq][sq];
        U64 moves = rawMoves & allowed;

        while (moves) {
            int to = POP_LSB(moves);
            U64 toBB = BIT(to);

            U64 isPromoting = turn ? (toBB & RANK_8) : (toBB & RANK_1);
            if (isPromoting) {
                legal[length++] = ENCODE_MOVE(sq, to, PAWN+turn*6, QUEEN_PROMO_FLAG);
                legal[length++] = ENCODE_MOVE(sq, to, PAWN+turn*6, KNIGHT_PROMO_FLAG);
                legal[length++] = ENCODE_MOVE(sq, to, PAWN+turn*6, ROOK_PROMO_FLAG);
                legal[length++] = ENCODE_MOVE(sq, to, PAWN+turn*6, BISHOP_PROMO_FLAG);
            } else {
                legal[length++] = ENCODE_MOVE(sq, to, PAWN+turn*6, EMPTY_FLAG);
            }
        }
    }

    /* Knights: a pinned knight can never move without breaking the pin,
     * so skip pinned knights outright. */
    U64 knightBB = board->knightBB & own & ~pinned;
    while (knightBB) {
        int sq = POP_LSB(knightBB);
        U64 moves = KNIGHT_REF[sq] & ~own & checkMask;
        while (moves) { int to = POP_LSB(moves); legal[length++] = ENCODE_MOVE(sq, to, KNIGHT+turn*6, EMPTY_FLAG); }
    }

    /* Bishops */
    U64 bishopBB = board->bishopBB & own;
    while (bishopBB) {
        int sq = POP_LSB(bishopBB);
        U64 moves = getBishopAttacks(sq, occ) & ~own & checkMask;
        if (pinned & BIT(sq)) moves &= LINE[kingSq][sq];
        while (moves) { int to = POP_LSB(moves); legal[length++] = ENCODE_MOVE(sq, to, BISHOP+turn*6, EMPTY_FLAG); }
    }

    /* Rooks */
    U64 rookBB = board->rookBB & own;
    while (rookBB) {
        int sq = POP_LSB(rookBB);
        U64 moves = getRookAttacks(sq, occ) & ~own & checkMask;
        if (pinned & BIT(sq)) moves &= LINE[kingSq][sq];
        while (moves) { int to = POP_LSB(moves); legal[length++] = ENCODE_MOVE(sq, to, ROOK+turn*6, EMPTY_FLAG); }
    }

    /* Queens */
    U64 queenBB = board->queenBB & own;
    while (queenBB) {
        int sq = POP_LSB(queenBB);
        U64 moves = (getRookAttacks(sq, occ) | getBishopAttacks(sq, occ)) & ~own & checkMask;
        if (pinned & BIT(sq)) moves &= LINE[kingSq][sq];
        while (moves) { int to = POP_LSB(moves); legal[length++] = ENCODE_MOVE(sq, to, QUEEN+turn*6, EMPTY_FLAG); }
    }

    /* Castling: cannot happen while in check, canCastle() already checks
     * that plus the king's start/path/landing squares for attacks. */
    if (numCheckers == 0) {
        int right[4] = { K, Q, k, q };
        int moveTo[4] = { G1, C1, G8, C8 };
        for (int i = 0; i < 4; i++) {
            if (canCastle(board, right[i], occ)) {
                legal[length++] = ENCODE_MOVE(kingSq, moveTo[i], right[i], CASTLE_FLAG);
            }
        }
    }

    return length;
}

void makeMove(Board *board, Move move) {
    Undo undo;
    doMove(board, move, &undo);
}

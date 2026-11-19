#include "pgn.h"

#include <string.h>
#include <time.h>

/* PGN uses Standard Algebraic Notation (SAN), not coordinate/UCI notation. */

typedef struct {
    FILE *out;
    int column;
} PgnWriter;

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

static char sanPieceLetter(int piece) {
    switch (piece) {
        case KNIGHT: return 'N';
        case BISHOP: return 'B';
        case ROOK:   return 'R';
        case QUEEN:  return 'Q';
        case KING:   return 'K';
        default:     return '\0';
    }
}

/* Use the engine's PROMO_PT() decoder so SAN follows the piece type that
 * the move encoding itself represents. Note that in the supplied header the
 * ROOK_PROMO_FLAG / BISHOP_PROMO_FLAG names appear reversed relative to this
 * decoder; see the note accompanying the integration instructions. */
static char sanPromotionLetter(int flag) {
    if (!IS_PROMO(flag)) return '\0';
    return sanPieceLetter(PROMO_PT(flag));
}

static bool appendChar(char *out, size_t capacity, size_t *length, char c) {
    if (*length + 1 >= capacity) return false;
    out[(*length)++] = c;
    out[*length] = '\0';
    return true;
}

static bool appendSquare(char *out, size_t capacity, size_t *length, int square) {
    if (square < 0 || square >= 64) return false;
    return appendChar(out, capacity, length, (char)('a' + (square & 7))) &&
           appendChar(out, capacity, length, (char)('1' + (square >> 3)));
}

static bool appendDisambiguation(const Board *board,
                                 int piece,
                                 int from,
                                 int to,
                                 const Move legal[],
                                 int legalCount,
                                 char *out,
                                 size_t capacity,
                                 size_t *length) {
    bool hasAlternative = false;
    bool alternativeOnSameFile = false;
    bool alternativeOnSameRank = false;
    bool movingWhite = (board->whiteBB & BIT(from)) != 0;

    for (int i = 0; i < legalCount; i++) {
        Move candidate = legal[i];
        int candidateFrom = EXTRACT_FROM(candidate);
        int candidateTo = EXTRACT_TO(candidate);
        if (candidateFrom == from || candidateTo != to) continue;
        if (IS_CASTLE(EXTRACT_FLAG(candidate)) || IS_PROMO(EXTRACT_FLAG(candidate))) continue;
        if (pieceTypeAt(board, candidateFrom) != piece) continue;

        bool candidateWhite = (board->whiteBB & BIT(candidateFrom)) != 0;
        if (candidateWhite != movingWhite) continue;

        hasAlternative = true;
        if ((candidateFrom & 7) == (from & 7)) alternativeOnSameFile = true;
        if ((candidateFrom >> 3) == (from >> 3)) alternativeOnSameRank = true;
    }

    if (!hasAlternative) return true;

    /* SAN rule: use file if it uniquely identifies the piece; otherwise rank
     * if that is unique; otherwise include both file and rank. */
    if (!alternativeOnSameFile) {
        return appendChar(out, capacity, length, (char)('a' + (from & 7)));
    }
    if (!alternativeOnSameRank) {
        return appendChar(out, capacity, length, (char)('1' + (from >> 3)));
    }
    return appendChar(out, capacity, length, (char)('a' + (from & 7))) &&
           appendChar(out, capacity, length, (char)('1' + (from >> 3)));
}

static bool buildSan(const Board *board,
                     Move move,
                     const Move legal[],
                     int legalCount,
                     char *san,
                     size_t sanCapacity) {
    int from = EXTRACT_FROM(move);
    int to = EXTRACT_TO(move);
    int flag = EXTRACT_FLAG(move);
    int piece = pieceTypeAt(board, from);
    size_t length = 0;

    if (!san || sanCapacity == 0 || piece < 0) return false;
    san[0] = '\0';

    if (IS_CASTLE(flag)) {
        /* In this engine, castling moves encode the king's destination square. */
        const char *castle = ((to & 7) == 6) ? "O-O" : "O-O-O";
        size_t n = strlen(castle);
        if (n + 1 > sanCapacity) return false;
        memcpy(san, castle, n + 1);
        length = n;
    } else {
        U64 enemy = board->turn ? board->blackBB : board->whiteBB;
        bool isCapture = (enemy & BIT(to)) != 0;

        /* En passant has an empty destination square, so detect it from the
         * board's en-passant target and the pawn's diagonal movement. */
        if (piece == PAWN && board->enp == to && (from & 7) != (to & 7))
            isCapture = true;

        if (piece != PAWN) {
            char letter = sanPieceLetter(piece);
            if (!letter || !appendChar(san, sanCapacity, &length, letter)) return false;
            if (!appendDisambiguation(board, piece, from, to,
                                      legal, legalCount,
                                      san, sanCapacity, &length)) return false;
        } else if (isCapture) {
            if (!appendChar(san, sanCapacity, &length,
                            (char)('a' + (from & 7)))) return false;
        }

        if (isCapture && !appendChar(san, sanCapacity, &length, 'x')) return false;
        if (!appendSquare(san, sanCapacity, &length, to)) return false;

        if (IS_PROMO(flag)) {
            char promotion = sanPromotionLetter(flag);
            if (!promotion ||
                !appendChar(san, sanCapacity, &length, '=') ||
                !appendChar(san, sanCapacity, &length, promotion)) return false;
        }
    }

    /* Determine check/checkmate by applying the move to a temporary board. */
    Board after = *board;
    makeMove(&after, move);

    U64 defendingKing = after.kingBB & (after.turn ? after.whiteBB : after.blackBB);
    if (defendingKing) {
        int kingSquare = LSB(defendingKing);
        bool inCheck = isSquareAttacked(&after, kingSquare, !after.turn);
        if (inCheck) {
            Move replies[MAX_MOVES];
            int replyCount = legalMoves(&after, replies);
            if (!appendChar(san, sanCapacity, &length,
                            replyCount == 0 ? '#' : '+')) return false;
        }
    }

    return true;
}

static void writeTag(FILE *out, const char *name, const char *value) {
    fprintf(out, "[%s \"", name);
    if (value) {
        for (const unsigned char *p = (const unsigned char *)value; *p; p++) {
            if (*p == '"' || *p == '\\') fputc('\\', out);
            /* Newlines/control characters are not valid inside a PGN tag. */
            if (*p == '\n' || *p == '\r' || *p == '\t') {
                fputc(' ', out);
            } else if (*p >= 0x20) {
                fputc(*p, out);
            }
        }
    }
    fputs("\"]\n", out);
}

static void writeToken(PgnWriter *writer, const char *token) {
    int tokenLength = (int)strlen(token);
    if (writer->column > 0 && writer->column + 1 + tokenLength > 80) {
        fputc('\n', writer->out);
        writer->column = 0;
    }
    if (writer->column > 0) {
        fputc(' ', writer->out);
        writer->column++;
    }
    fputs(token, writer->out);
    writer->column += tokenLength;
}

static const char *pgnResult(GameResult result) {
    if (result.winner == RESULT_WHITE_WIN) return "1-0";
    if (result.winner == RESULT_BLACK_WIN) return "0-1";
    return "1/2-1/2";
}

static const char *pgnTermination(GameResult result) {
    if (result.reason == REASON_TIMEOUT) return "time forfeit";
    if (result.reason == REASON_ILLEGAL_NO_MOVE) return "rules infraction";
    return "normal";
}

bool pgnWriteGame(FILE *out,
                  int gameNumber,
                  const char *whiteName,
                  const char *blackName,
                  const char *startFen,
                  const Move moves[],
                  int moveCount,
                  GameResult result) {
    if (!out || moveCount < 0 || (moveCount > 0 && !moves)) return false;

    const char *fen = startFen ? startFen : START_FEN;
    bool nonstandardStart = strcmp(fen, START_FEN) != 0;

    char date[16] = "????.??.??";
    time_t now = time(NULL);
    struct tm *local = localtime(&now);
    if (local) strftime(date, sizeof(date), "%Y.%m.%d", local);

    char round[32];
    char plyCount[32];
    snprintf(round, sizeof(round), "%d", gameNumber);
    snprintf(plyCount, sizeof(plyCount), "%d", moveCount);

    writeTag(out, "Event", "Bot Match");
    writeTag(out, "Site", "Local");
    writeTag(out, "Date", date);
    writeTag(out, "Round", round);
    writeTag(out, "White", whiteName ? whiteName : "White");
    writeTag(out, "Black", blackName ? blackName : "Black");
    writeTag(out, "Result", pgnResult(result));
    writeTag(out, "PlyCount", plyCount);
    writeTag(out, "Termination", pgnTermination(result));
    writeTag(out, "TerminationDetail", reasonName(result.reason));

    if (nonstandardStart) {
        writeTag(out, "SetUp", "1");
        writeTag(out, "FEN", fen);
    }
    fputc('\n', out);

    Board board;
    setFen(&board, (char *)fen);

    PgnWriter writer = { out, 0 };
    /* Keep our own PGN move number. The engine's `fullmove` bookkeeping is
     * not relied on after replay because some compact engines increment it
     * on every ply rather than only after Black moves. */
    int pgnMoveNumber = board.fullmove;

    for (int i = 0; i < moveCount; i++) {
        Move legal[MAX_MOVES];
        int legalCount = legalMoves(&board, legal);

        bool moveIsLegal = false;
        for (int j = 0; j < legalCount; j++) {
            if (legal[j] == moves[i]) {
                moveIsLegal = true;
                break;
            }
        }
        if (!moveIsLegal) return false;

        char san[32];
        if (!buildSan(&board, moves[i], legal, legalCount, san, sizeof(san)))
            return false;

        char moveNumber[24];
        bool whiteToMove = board.turn != 0;
        if (whiteToMove) {
            snprintf(moveNumber, sizeof(moveNumber), "%d.", pgnMoveNumber);
            writeToken(&writer, moveNumber);
        } else if (i == 0) {
            /* PGN requires an explicit black move number when a game starts
             * with Black to move (e.g. from a FEN position). */
            snprintf(moveNumber, sizeof(moveNumber), "%d...", pgnMoveNumber);
            writeToken(&writer, moveNumber);
        }

        writeToken(&writer, san);
        makeMove(&board, moves[i]);
        if (!whiteToMove) pgnMoveNumber++;
    }

    writeToken(&writer, pgnResult(result));
    fputc('\n', out);
    fputc('\n', out);

    return !ferror(out);
}

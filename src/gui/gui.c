#include "gui.h"

#include "game.h"
#include "movegen.h"
#include "pgn.h"

#include "raylib.h"

#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GUI_WIDTH 1100
#define GUI_HEIGHT 780
#define BOARD_SIZE 640.0f
#define MAX_HISTORY (MAX_GAME_PLIES + 8)
#define DEFAULT_CLOCK_SECONDS 300
#define MIN_CLOCK_SECONDS 30
#define MAX_CLOCK_SECONDS 3600
#define CLOCK_STEP_SECONDS 30

/* The engine uses square 0 = a1 and square 63 = h8. */

typedef struct {
    pthread_t thread;
    pthread_mutex_t mutex;
    bool active;
    bool done;
    Move chosen;
    BotFunc bot;
    Board board;
    Move legal[MAX_MOVES];
    int legalCount;
    double timeLeftMs;
    U64 history[MAX_HISTORY];
    BotContext context;
} BotTask;

typedef struct {
    Texture2D pieces[2][6]; /* [0] black, [1] white; piece type PAWN..KING */
    bool pieceLoaded[2][6];

    const char *botName;
    BotFunc bot;
    bool settingsScreen;
    int humanColor; /* 1 = white, 0 = black */
    int humanSeconds;
    int botSeconds;

    Board board;
    Move legal[MAX_MOVES];
    int legalCount;
    Move moveLog[MAX_GAME_PLIES];
    int moveCount;
    U64 positionHistory[MAX_HISTORY];
    int historyLen;
    double clocks[2]; /* indexed by board.turn: 1 white, 0 black */

    bool gameStarted;
    bool gameOver;
    bool resultSaved;
    GameResult result;
    char status[160];

    int selectedSquare;
    Move promotionMoves[4];
    int promotionCount;
    bool hasLastMove;
    Move lastMove;

    bool botTimedOut;
    BotTask botTask;

    float boardX;
    float boardY;
    float squareSize;
} ChessApp;

static const char *PIECE_FILES[2][6] = {
    { "blackPawn.png", "blackKnight.png", "blackBishop.png",
      "blackRook.png", "blackQueen.png", "blackKing.png" },
    { "whitePawn.png", "whiteKnight.png", "whiteBishop.png",
      "whiteRook.png", "whiteQueen.png", "whiteKing.png" }
};

static const Color COLOR_BG       = { 28, 31, 34, 255 };
static const Color COLOR_PANEL    = { 39, 43, 47, 255 };
static const Color COLOR_TEXT     = { 239, 237, 229, 255 };
static const Color COLOR_MUTED    = { 166, 171, 174, 255 };
static const Color COLOR_DARK = { 231, 224, 207, 255 };
static const Color COLOR_LIGHT = { 115, 139, 103, 255 };
static const Color COLOR_ACCENT   = { 220, 181, 91, 255 };
static const Color COLOR_GREEN    = { 102, 167, 119, 255 };
static const Color COLOR_RED      = { 191, 91, 82, 255 };

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

static bool isSquareWhite(const Board *board, int square) {
    return (board->whiteBB & BIT(square)) != 0;
}

static int kingSquare(const Board *board, bool white) {
    U64 king = board->kingBB & (white ? board->whiteBB : board->blackBB);
    return king ? LSB(king) : -1;
}

static bool insufficientMaterial(const Board *board) {
    if (board->pawnBB || board->rookBB || board->queenBB) return false;

    int whiteMinor = __builtin_popcountll(
        board->whiteBB & (board->knightBB | board->bishopBB));
    int blackMinor = __builtin_popcountll(
        board->blackBB & (board->knightBB | board->bishopBB));

    if (whiteMinor == 0 && blackMinor == 0) return true;
    if (whiteMinor + blackMinor > 2) return false;
    if (whiteMinor <= 1 && blackMinor == 0) return true;
    if (blackMinor <= 1 && whiteMinor == 0) return true;

    if (whiteMinor == 1 && blackMinor == 1 &&
        __builtin_popcountll(board->bishopBB) == 2) {
        const U64 lightSquares = 0xAA55AA55AA55AA55ULL;
        U64 whiteBishop = board->bishopBB & board->whiteBB;
        U64 blackBishop = board->bishopBB & board->blackBB;
        bool whiteLight = (whiteBishop & lightSquares) != 0;
        bool blackLight = (blackBishop & lightSquares) != 0;
        return whiteLight == blackLight;
    }
    return false;
}

static bool positionIsInCheck(const Board *board) {
    int square = kingSquare(board, board->turn != 0);
    return square >= 0 && isSquareAttacked((Board *)board, square, !board->turn);
}

static const char *resultReasonName(ResultReason reason) {
    switch (reason) {
        case REASON_CHECKMATE: return "checkmate";
        case REASON_STALEMATE: return "stalemate";
        case REASON_FIFTY_MOVE: return "50-move rule";
        case REASON_REPETITION: return "repetition";
        case REASON_INSUFFICIENT_MATERIAL: return "insufficient material";
        case REASON_TIMEOUT: return "time";
        case REASON_MOVE_LIMIT: return "move limit";
        default: return "game error";
    }
}

static void finishGame(ChessApp *app, GameResult result, const char *message) {
    char messageCopy[sizeof(app->status)];
    snprintf(messageCopy, sizeof(messageCopy), "%s", message ? message : "Game over");

    app->gameOver = true;
    app->result = result;
    app->selectedSquare = -1;
    app->promotionCount = 0;
    snprintf(app->status, sizeof(app->status), "%s", messageCopy);
}

static void finishOnTime(ChessApp *app, int flaggingSide) {
    GameResult result;
    result.winner = flaggingSide ? RESULT_BLACK_WIN : RESULT_WHITE_WIN;
    result.reason = REASON_TIMEOUT;
    result.plyCount = app->moveCount;

    bool humanWon = result.winner ==
        (app->humanColor ? RESULT_WHITE_WIN : RESULT_BLACK_WIN);
    char message[128];
    snprintf(message, sizeof(message), "%s on time - %s",
             humanWon ? "You win" : "Bot wins", resultReasonName(result.reason));
    finishGame(app, result, message);
}

/* This follows the terminal adjudication order used by playGame() in game.c. */
static bool updateGameStatus(ChessApp *app) {
    app->legalCount = legalMoves(&app->board, app->legal);
    bool inCheck = positionIsInCheck(&app->board);

    if (app->legalCount == 0) {
        GameResult result;
        result.plyCount = app->moveCount;
        if (inCheck) {
            result.reason = REASON_CHECKMATE;
            result.winner = app->board.turn ? RESULT_BLACK_WIN : RESULT_WHITE_WIN;
        } else {
            result.reason = REASON_STALEMATE;
            result.winner = RESULT_DRAW;
        }

        bool humanWon = result.winner ==
            (app->humanColor ? RESULT_WHITE_WIN : RESULT_BLACK_WIN);
        char message[128];
        if (result.winner == RESULT_DRAW) {
            snprintf(message, sizeof(message), "Draw - stalemate");
        } else {
            snprintf(message, sizeof(message), "%s - checkmate",
                     humanWon ? "You win" : "Bot wins");
        }
        finishGame(app, result, message);
        return true;
    }

    if (app->board.halfmove >= 100) {
        GameResult result = { RESULT_DRAW, REASON_FIFTY_MOVE, app->moveCount };
        finishGame(app, result, "Draw — 50-move rule");
        return true;
    }

    if (insufficientMaterial(&app->board)) {
        GameResult result = {
            RESULT_DRAW, REASON_INSUFFICIENT_MATERIAL, app->moveCount
        };
        finishGame(app, result, "Draw - insufficient material");
        return true;
    }

    int repetitions = 0;
    U64 currentHash = zobristHash(&app->board);
    for (int i = 0; i < app->historyLen; ++i) {
        if (app->positionHistory[i] == currentHash) ++repetitions;
    }
    if (repetitions >= 3) {
        GameResult result = { RESULT_DRAW, REASON_REPETITION, app->moveCount };
        finishGame(app, result, "Draw - threefold repetition");
        return true;
    }

    if (app->moveCount >= MAX_GAME_PLIES) {
        GameResult result = { RESULT_DRAW, REASON_MOVE_LIMIT, app->moveCount };
        finishGame(app, result, "Draw - move limit");
        return true;
    }

    if (inCheck) {
        snprintf(app->status, sizeof(app->status), "%s to move - check",
                 app->board.turn ? "White" : "Black");
    } else {
        snprintf(app->status, sizeof(app->status), "%s to move",
                 app->board.turn ? "White" : "Black");
    }
    return false;
}

static void applyMove(ChessApp *app, Move move) {
    bool found = false;
    for (int i = 0; i < app->legalCount; ++i) {
        if (app->legal[i] == move) {
            found = true;
            break;
        }
    }
    if (!found || app->gameOver) return;

    makeMove(&app->board, move);
    if (app->moveCount < MAX_GAME_PLIES)
        app->moveLog[app->moveCount++] = move;
    app->lastMove = move;
    app->hasLastMove = true;
    if (app->historyLen < MAX_HISTORY)
        app->positionHistory[app->historyLen++] = zobristHash(&app->board);

    app->selectedSquare = -1;
    app->promotionCount = 0;
    app->legalCount = 0;

    if (updateGameStatus(app)) return;
    if (app->board.turn != app->humanColor) {
        snprintf(app->status, sizeof(app->status), "%s is thinking...", app->botName);
    }
}

static void *botThreadMain(void *arg) {
    BotTask *task = (BotTask *)arg;
    Move chosen = task->bot(&task->board, task->legal, task->legalCount,
                            task->timeLeftMs, 0.0, &task->context);

    pthread_mutex_lock(&task->mutex);
    task->chosen = chosen;
    task->done = true;
    pthread_mutex_unlock(&task->mutex);
    return NULL;
}

static bool botThreadIsDone(BotTask *task) {
    bool done;
    pthread_mutex_lock(&task->mutex);
    done = task->done;
    pthread_mutex_unlock(&task->mutex);
    return done;
}

static void joinBotThread(ChessApp *app) {
    if (!app->botTask.active) return;
    pthread_join(app->botTask.thread, NULL);
    app->botTask.active = false;
}

static void startBotTurn(ChessApp *app) {
    if (app->gameOver || app->board.turn == app->humanColor ||
        app->botTask.active) return;

    app->legalCount = legalMoves(&app->board, app->legal);
    if (app->legalCount == 0) {
        updateGameStatus(app);
        return;
    }

    BotTask *task = &app->botTask;
    task->board = app->board;
    memcpy(task->legal, app->legal, (size_t)app->legalCount * sizeof(Move));
    task->legalCount = app->legalCount;
    task->bot = app->bot;
    task->timeLeftMs = app->clocks[app->board.turn];
    memcpy(task->history, app->positionHistory,
           (size_t)app->historyLen * sizeof(U64));
    task->context.positionHistory = task->history;
    task->context.positionHistoryCount = app->historyLen;
    task->context.positionHash = zobristHash;

    pthread_mutex_lock(&task->mutex);
    task->done = false;
    pthread_mutex_unlock(&task->mutex);

    app->botTimedOut = false;
    app->status[0] = '\0';
    if (pthread_create(&task->thread, NULL, botThreadMain, task) != 0) {
        snprintf(app->status, sizeof(app->status), "Could not start bot thread");
        GameResult result = {
            app->board.turn ? RESULT_WHITE_WIN : RESULT_BLACK_WIN,
            REASON_ILLEGAL_NO_MOVE,
            app->moveCount
        };
        finishGame(app, result, app->status);
        return;
    }
    task->active = true;
}

static void resetGame(ChessApp *app) {
    if (app->botTask.active) joinBotThread(app);

    setFen(&app->board, (char *)START_FEN);
    app->moveCount = 0;
    app->historyLen = 1;
    app->positionHistory[0] = zobristHash(&app->board);
    app->clocks[1] = (double)app->humanSeconds * 1000.0;
    app->clocks[0] = (double)app->botSeconds * 1000.0;
    app->gameStarted = true;
    app->gameOver = false;
    app->resultSaved = false;
    app->hasLastMove = false;
    app->selectedSquare = -1;
    app->promotionCount = 0;
    app->botTimedOut = false;
    app->result = (GameResult){ RESULT_DRAW, REASON_MOVE_LIMIT, 0 };

    updateGameStatus(app);
    if (!app->gameOver && app->board.turn != app->humanColor)
        startBotTurn(app);
}

static void loadPieceTextures(ChessApp *app) {
    const char *assetDir = getenv("CHESS_ASSET_DIR");
    if (!assetDir || !assetDir[0]) assetDir = "assets/sprites";

    for (int color = 0; color < 2; ++color) {
        for (int piece = 0; piece < 6; ++piece) {
            char path[512];
            snprintf(path, sizeof(path), "%s/%s", assetDir,
                     PIECE_FILES[color][piece]);
            if (FileExists(path)) {
                app->pieces[color][piece] = LoadTexture(path);
                app->pieceLoaded[color][piece] =
                    app->pieces[color][piece].id != 0;
            } else {
                app->pieceLoaded[color][piece] = false;
                TraceLog(LOG_WARNING, "Missing chess piece asset: %s", path);
            }
        }
    }
}

static void unloadPieceTextures(ChessApp *app) {
    for (int color = 0; color < 2; ++color)
        for (int piece = 0; piece < 6; ++piece)
            if (app->pieceLoaded[color][piece])
                UnloadTexture(app->pieces[color][piece]);
}

static void updateBoardLayout(ChessApp *app) {
    int width = GetScreenWidth();
    int height = GetScreenHeight();
    float availableHeight = (float)height - 112.0f;
    float availableWidth = (float)width - 390.0f;
    float size = fminf(BOARD_SIZE, fminf(availableHeight, availableWidth));
    app->squareSize = floorf(size / 8.0f);
    size = app->squareSize * 8.0f;
    app->boardX = 28.0f;
    app->boardY = ((float)height - size) * 0.5f + 12.0f;
}

static Rectangle squareRect(const ChessApp *app, int square) {
    int file = square & 7;
    int rank = square >> 3;
    return (Rectangle){
        app->boardX + (float)file * app->squareSize,
        app->boardY + (float)(7 - rank) * app->squareSize,
        app->squareSize,
        app->squareSize
    };
}

static int screenToSquare(const ChessApp *app, Vector2 point) {
    float x = point.x - app->boardX;
    float y = point.y - app->boardY;
    float boardSize = app->squareSize * 8.0f;
    if (x < 0 || y < 0 || x >= boardSize || y >= boardSize) return -1;
    int file = (int)(x / app->squareSize);
    int screenRank = (int)(y / app->squareSize);
    int rank = 7 - screenRank;
    return rank * 8 + file;
}

static bool squareIsLegalTarget(const ChessApp *app, int from, int to) {
    for (int i = 0; i < app->legalCount; ++i)
        if (EXTRACT_FROM(app->legal[i]) == from &&
            EXTRACT_TO(app->legal[i]) == to) return true;
    return false;
}

static void selectOrMoveSquare(ChessApp *app, int square) {
    if (square < 0 || app->gameOver || app->board.turn != app->humanColor ||
        app->botTask.active || app->promotionCount > 0) return;

    if (app->selectedSquare < 0) {
        if (isSquareWhite(&app->board, square) == (app->humanColor != 0) &&
            pieceTypeAt(&app->board, square) >= 0) {
            app->selectedSquare = square;
        }
        return;
    }

    int from = app->selectedSquare;
    if (from == square) {
        app->selectedSquare = -1;
        return;
    }

    Move matches[4];
    int matchCount = 0;
    for (int i = 0; i < app->legalCount; ++i) {
        Move move = app->legal[i];
        if (EXTRACT_FROM(move) == from && EXTRACT_TO(move) == square) {
            if (matchCount < 4) matches[matchCount++] = move;
        }
    }

    if (matchCount == 1) {
        applyMove(app, matches[0]);
        return;
    }
    if (matchCount > 1) {
        app->promotionCount = matchCount;
        memcpy(app->promotionMoves, matches,
               (size_t)matchCount * sizeof(Move));
        return;
    }

    /* Clicking another friendly piece changes the selection. */
    if (pieceTypeAt(&app->board, square) >= 0 &&
        isSquareWhite(&app->board, square) == (app->humanColor != 0)) {
        app->selectedSquare = square;
    } else {
        app->selectedSquare = -1;
    }
}

static void drawTextCentered(const char *text, int x, int y, int fontSize,
                             Color color) {
    DrawText(text, x - MeasureText(text, fontSize) / 2, y, fontSize, color);
}

static void drawBoard(ChessApp *app) {
    const float s = app->squareSize;
    for (int rank = 7; rank >= 0; --rank) {
        for (int file = 0; file < 8; ++file) {
            int square = rank * 8 + file;
            Rectangle rect = squareRect(app, square);
            Color base = ((rank + file) & 1) ? COLOR_DARK : COLOR_LIGHT;
            DrawRectangleRec(rect, base);

            if (app->hasLastMove &&
                (square == EXTRACT_FROM(app->lastMove) ||
                 square == EXTRACT_TO(app->lastMove))) {
                DrawRectangleRec(rect, (Color){ 226, 195, 99, 105 });
            }
            if (square == app->selectedSquare) {
                DrawRectangleRec(rect, (Color){ 255, 220, 82, 145 });
            }

            int piece = pieceTypeAt(&app->board, square);
            if (piece >= 0) {
                int color = isSquareWhite(&app->board, square) ? 1 : 0;
                if (app->pieceLoaded[color][piece]) {
                    Texture2D texture = app->pieces[color][piece];
                    float padding = s * 0.07f;
                    Rectangle source = { 0, 0, (float)texture.width,
                                         (float)texture.height };
                    Rectangle destination = {
                        rect.x + padding,
                        rect.y + padding,
                        s - 2.0f * padding,
                        s - 2.0f * padding
                    };
                    DrawTexturePro(texture, source, destination, (Vector2){ 0, 0 }, 0.0f, WHITE);
                } else {
                    perror("textures");
                    return;
                }
            }

            if (!app->gameOver && app->selectedSquare >= 0 &&
                squareIsLegalTarget(app, app->selectedSquare, square)) {
                bool occupied = pieceTypeAt(&app->board, square) >= 0;
                Vector2 center = { rect.x + s * 0.5f, rect.y + s * 0.5f };
                if (occupied) {
                    DrawRing(center, s * 0.39f, s * 0.45f, 0, 360, 32, (Color){ 24, 28, 25, 185 });
                } else {
                    DrawCircleV(center, s * 0.105f, (Color){ 24, 28, 25, 125 });
                }
            }
        }
    }

    for (int file = 0; file < 8; ++file) {
        char label[2] = { (char)('a' + file), '\0' };
        DrawText(label,
                 (int)(app->boardX + (float)file * s + s - 15),
                 (int)(app->boardY + s * 8.0f - 19), 13,
                 (file & 1) ? COLOR_LIGHT : COLOR_DARK);
    }
    for (int row = 0; row < 8; ++row) {
        char label[2] = { (char)('8' - row), '\0' };
        DrawText(label, (int)(app->boardX + 5),
                 (int)(app->boardY + (float)row * s + 4), 13,
                 (row & 1) ? COLOR_DARK : COLOR_LIGHT);
    }
}

static bool button(Rectangle rect, const char *label, bool enabled,
                   int fontSize) {
    Vector2 mouse = GetMousePosition();
    bool hovered = CheckCollisionPointRec(mouse, rect);
    Color fill = enabled ? (hovered ? (Color){ 76, 83, 87, 255 }
                                     : (Color){ 57, 63, 67, 255 })
                         : (Color){ 48, 52, 55, 255 };
    DrawRectangleRounded(rect, 0.16f, 8, fill);
    DrawRectangleRoundedLines(rect, 0.16f, 8,
                              enabled ? (Color){ 91, 99, 103, 255 }
                                      : (Color){ 61, 65, 68, 255 });
    drawTextCentered(label, (int)(rect.x + rect.width * 0.5f),
                     (int)(rect.y + (rect.height - fontSize) * 0.5f),
                     fontSize, enabled ? COLOR_TEXT : COLOR_MUTED);
    return enabled && hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

static void formatClock(double milliseconds, char out[16]) {
    int seconds = (int)ceil(fmax(0.0, milliseconds) / 1000.0);
    snprintf(out, 16, "%d:%02d", seconds / 60, seconds % 60);
}

static void drawClockPanel(const char *title, double milliseconds,
                           bool active, float x, float y, float width) {
    Rectangle box = { x, y, width, 88 };
    DrawRectangleRounded(box, 0.12f, 8,
                         active ? (Color){ 54, 64, 57, 255 } : COLOR_PANEL);
    DrawRectangleRoundedLines(box, 0.12f, 8,
                              active ? COLOR_GREEN : (Color){ 57, 62, 66, 255 });
    DrawText(title, (int)(x + 16), (int)(y + 12), 16,
             active ? COLOR_GREEN : COLOR_MUTED);
    char clockText[16];
    formatClock(milliseconds, clockText);
    DrawText(clockText, (int)(x + 16), (int)(y + 34), 31,
             milliseconds <= 10000.0 ? COLOR_RED : COLOR_TEXT);
}

static void drawPromotionChooser(ChessApp *app, float panelX, float panelY,
                                 float panelWidth) {
    if (app->promotionCount == 0) return;
    DrawRectangleRounded((Rectangle){ panelX, panelY, panelWidth, 130 },
                         0.12f, 8, COLOR_PANEL);
    DrawText("Promote pawn to", (int)(panelX + 16), (int)(panelY + 12),
             17, COLOR_TEXT);

    const int desiredPieces[4] = { QUEEN, ROOK, BISHOP, KNIGHT };
    for (int i = 0; i < 4; ++i) {
        Rectangle option = {
            panelX + 12.0f + (float)i * (panelWidth - 24.0f) / 4.0f,
            panelY + 48.0f,
            (panelWidth - 32.0f) / 4.0f,
            60.0f
        };
        char label[2] = { "QRBN"[i], '\0' };
        if (button(option, label, true, 24)) {
            for (int j = 0; j < app->promotionCount; ++j) {
                if (PROMO_PT(EXTRACT_FLAG(app->promotionMoves[j])) == desiredPieces[i]) {
                    applyMove(app, app->promotionMoves[j]);
                    break;
                }
            }
        }
    }
}

static void drawSettings(ChessApp *app) {
    int width = GetScreenWidth();
    int height = GetScreenHeight();
    float centerX = (float)width * 0.5f;
    float top = (float)height * 0.5f - 230.0f;
    float panelWidth = 560.0f;
    float panelX = centerX - panelWidth * 0.5f;

    DrawText("CHESS", (int)centerX - MeasureText("CHESS", 46) / 2,
             (int)top, 46, COLOR_TEXT);
    drawTextCentered(app->botName, (int)centerX, (int)top + 66, 20, COLOR_MUTED);

    DrawText("Play as", (int)(panelX + 25), (int)top + 122, 17, COLOR_MUTED);
    Rectangle whiteButton = { panelX + 25, top + 151, 245, 54 };
    Rectangle blackButton = { panelX + 290, top + 151, 245, 54 };
    bool chooseWhite = button(whiteButton, "White", true, 19);
    bool chooseBlack = button(blackButton, "Black", true, 19);
    if (chooseWhite) app->humanColor = 1;
    if (chooseBlack) app->humanColor = 0;
    DrawRectangleRoundedLines(whiteButton, 0.16f, 8,
        app->humanColor ? COLOR_ACCENT : (Color){ 91, 99, 103, 255 });
    DrawRectangleRoundedLines(blackButton, 0.16f, 8,
        !app->humanColor ? COLOR_ACCENT : (Color){ 91, 99, 103, 255 });

    DrawText("Your clock", (int)(panelX + 25), (int)top + 232, 17, COLOR_MUTED);
    DrawText("Bot clock", (int)(panelX + 290), (int)top + 232, 17, COLOR_MUTED);

    Rectangle humanMinus = { panelX + 25, top + 264, 54, 58 };
    Rectangle humanPlus = { panelX + 216, top + 264, 54, 58 };
    Rectangle botMinus = { panelX + 290, top + 264, 54, 58 };
    Rectangle botPlus = { panelX + 481, top + 264, 54, 58 };
    char clock[16];
    formatClock((double)app->humanSeconds * 1000.0, clock);
    drawTextCentered(clock, (int)(panelX + 148), (int)top + 281, 25, COLOR_TEXT);
    formatClock((double)app->botSeconds * 1000.0, clock);
    drawTextCentered(clock, (int)(panelX + 413), (int)top + 281, 25, COLOR_TEXT);

    if (button(humanMinus, "-", app->humanSeconds > MIN_CLOCK_SECONDS, 26))
        app->humanSeconds -= CLOCK_STEP_SECONDS;
    if (button(humanPlus, "+", app->humanSeconds < MAX_CLOCK_SECONDS, 26))
        app->humanSeconds += CLOCK_STEP_SECONDS;
    if (button(botMinus, "-", app->botSeconds > MIN_CLOCK_SECONDS, 26))
        app->botSeconds -= CLOCK_STEP_SECONDS;
    if (button(botPlus, "+", app->botSeconds < MAX_CLOCK_SECONDS, 26))
        app->botSeconds += CLOCK_STEP_SECONDS;

    drawTextCentered("Independent time controls · 30-second steps",
                     (int)centerX, (int)top + 348, 15, COLOR_MUTED);
    Rectangle startButton = { centerX - 145.0f, top + 390, 290, 62 };
    if (button(startButton, "Start game", true, 21)) {
        app->settingsScreen = false;
        resetGame(app);
    }
}

static void drawGamePanel(ChessApp *app) {
    int width = GetScreenWidth();
    float panelX = app->boardX + app->squareSize * 8.0f + 34.0f;
    float panelWidth = (float)width - panelX - 28.0f;
    if (panelWidth < 260.0f) panelWidth = 260.0f;

    DrawText("CHESS", (int)panelX, 34, 23, COLOR_TEXT);
    DrawText(app->botName, (int)panelX, 62, 16, COLOR_MUTED);

    const char *humanTitle = app->humanColor ? "YOU  ·  WHITE" : "YOU  ·  BLACK";
    const char *botTitle = app->humanColor ? "BOT  ·  BLACK" : "BOT  ·  WHITE";
    float firstY = 112.0f;
    bool humanActive = app->gameStarted && !app->gameOver &&
                       app->board.turn == app->humanColor;
    bool botActive = app->gameStarted && !app->gameOver &&
                     app->board.turn != app->humanColor;
    drawClockPanel(humanTitle,
                   app->clocks[app->humanColor], humanActive,
                   panelX, firstY, panelWidth);
    drawClockPanel(botTitle,
                   app->clocks[!app->humanColor], botActive,
                   panelX, firstY + 104.0f, panelWidth);

    DrawText("STATUS", (int)panelX, (int)firstY + 220, 14, COLOR_MUTED);
    DrawText(app->status, (int)panelX, (int)firstY + 244,
             17, app->gameOver ? COLOR_ACCENT : COLOR_TEXT);

    if (app->promotionCount > 0) {
        drawPromotionChooser(app, panelX, firstY + 292, panelWidth);
    } else {
        DrawText("HOW TO MOVE", (int)panelX, (int)firstY + 306, 14, COLOR_MUTED);
        DrawText("1. Select one of your pieces", (int)panelX,
                 (int)firstY + 332, 15, COLOR_TEXT);
        DrawText("2. Select a highlighted square", (int)panelX,
                 (int)firstY + 356, 15, COLOR_TEXT);
        DrawText("Only legal moves are selectable.", (int)panelX,
                 (int)firstY + 390, 14, COLOR_MUTED);
    }

    float buttonY = (float)GetScreenHeight() - 104.0f;
    Rectangle newGame = { panelX, buttonY, panelWidth, 44 };
    if (button(newGame, "New game", true, 17)) {
        if (app->botTask.active) joinBotThread(app);
        app->settingsScreen = true;
        app->gameStarted = false;
        app->selectedSquare = -1;
        app->promotionCount = 0;
    }

    if (app->gameOver) {
        Rectangle again = { panelX, buttonY + 54, panelWidth, 40 };
        if (button(again, "Play again", true, 16)) {
            app->settingsScreen = true;
            app->gameStarted = false;
        }
    }
}

static void handleBotCompletion(ChessApp *app) {
    if (!app->botTask.active || !botThreadIsDone(&app->botTask)) return;
    joinBotThread(app);

    if (app->gameOver) return;
    if (app->botTimedOut || app->clocks[app->board.turn] <= 0.0) {
        finishOnTime(app, app->board.turn);
        return;
    }

    Move chosen = app->botTask.chosen;
    bool isLegal = false;
    for (int i = 0; i < app->legalCount; ++i) {
        if (app->legal[i] == chosen) {
            isLegal = true;
            break;
        }
    }

    if (!isLegal) {
        GameResult result;
        result.winner = app->humanColor ? RESULT_WHITE_WIN : RESULT_BLACK_WIN;
        result.reason = REASON_ILLEGAL_NO_MOVE;
        result.plyCount = app->moveCount;
        finishGame(app, result, "You win — bot returned an illegal move");
        return;
    }

    applyMove(app, chosen);
}

int runChessGui(const char *botName, U64 seed) {
    BotFunc bot = findBot(botName);
    if (!bot) {
        fprintf(stderr, "Unknown bot: %s\n", botName ? botName : "(null)");
        fprintf(stderr, "Available bots:\n");
        listBots();
        return 1;
    }

    ChessApp app = { 0 };
    app.botName = botName;
    app.bot = bot;
    app.settingsScreen = true;
    app.humanColor = 1;
    app.humanSeconds = DEFAULT_CLOCK_SECONDS;
    app.botSeconds = DEFAULT_CLOCK_SECONDS;
    app.selectedSquare = -1;
    app.status[0] = '\0';
    pthread_mutex_init(&app.botTask.mutex, NULL);

    initMoveGen();
    initZobrist();
    botRngSeed(&g_botRng, seed);

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    InitWindow(GUI_WIDTH, GUI_HEIGHT, "Chess");
    if (!IsWindowReady()) {
        fprintf(stderr, "Could not initialize the Raylib window.\n");
        pthread_mutex_destroy(&app.botTask.mutex);
        return 1;
    }
    SetWindowMinSize(980, 720);
    SetTargetFPS(60);
    loadPieceTextures(&app);

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        updateBoardLayout(&app);

        if (app.gameStarted && !app.gameOver) {
            int activeSide = app.board.turn;
            app.clocks[activeSide] -= (double)dt * 1000.0;
            if (app.clocks[activeSide] <= 0.0) {
                app.clocks[activeSide] = 0.0;
                if (activeSide == app.humanColor) {
                    finishOnTime(&app, activeSide);
                } else if (app.botTask.active) {
                    app.botTimedOut = true;
                    snprintf(app.status, sizeof(app.status), "Bot has run out of time...");
                } else {
                    finishOnTime(&app, activeSide);
                }
            }

            handleBotCompletion(&app);
            if (!app.gameOver && app.board.turn != app.humanColor &&
                !app.botTask.active) {
                startBotTurn(&app);
            }

            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
                !app.settingsScreen && app.board.turn == app.humanColor &&
                !app.botTask.active && app.promotionCount == 0 && !app.gameOver) {
                int square = screenToSquare(&app, GetMousePosition());
                selectOrMoveSquare(&app, square);
            }
        }

        BeginDrawing();
        ClearBackground(COLOR_BG);
        if (app.settingsScreen) {
            drawSettings(&app);
        } else {
            drawBoard(&app);
            drawGamePanel(&app);
        }
        EndDrawing();
    }

    if (app.botTask.active) joinBotThread(&app);
    unloadPieceTextures(&app);
    CloseWindow();
    pthread_mutex_destroy(&app.botTask.mutex);
    return 0;
}

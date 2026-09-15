#include "bot.h"
#include <string.h>

void botRngSeed(BotRng *r, U64 seed) {
    r->state = seed ? seed : 0x2545F4914F6CDD1DULL;
}

U64 botRngNext(BotRng *r) {
    U64 x = r->state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    r->state = x;
    return x;
}

BotRng g_botRng;

#define MAX_BOTS 64

typedef struct {
    char name[64];
    BotFunc fn;
} BotRegistryEntry;

static BotRegistryEntry registry[MAX_BOTS];
static int registryCount = 0;

void registerBot(const char *name, BotFunc fn) {
    if (registryCount >= MAX_BOTS) {
        fprintf(stderr, "bot registry full, increase MAX_BOTS to add '%s'\n", name);
        exit(1);
    }
    for (int i = 0; i < registryCount; i++) {
        if (strcmp(registry[i].name, name) == 0) {
            fprintf(stderr, "duplicate bot name '%s' (registered from two files?)\n", name);
            exit(1);
        }
    }
    strncpy(registry[registryCount].name, name, sizeof(registry[registryCount].name) - 1);
    registry[registryCount].fn = fn;
    registryCount++;
}

BotFunc findBot(const char *name) {
    for (int i = 0; i < registryCount; i++) {
        if (strcmp(registry[i].name, name) == 0) return registry[i].fn;
    }
    return NULL;
}

void listBots(void) {
    for (int i = 0; i < registryCount; i++) printf("  %s\n", registry[i].name);
}

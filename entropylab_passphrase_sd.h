#pragma once

#include <storage/storage.h>
#include <stdint.h>
#include <stdbool.h>

#define PASSPHRASE_SD_PATH "/ext/apps/Tools/entropylab"
#define PASSPHRASE_EFF_LONG_PATH "/ext/apps/Tools/entropylab/eff_large_wordlist.txt"
#define PASSPHRASE_BIP39_PATH "/ext/apps/Tools/entropylab/bip39_english.txt"
#define PASSPHRASE_SLIP39_PATH "/ext/apps/Tools/entropylab/slip39_english.txt"

// Wordlist sizes and entropy
#define EFF_LONG_SIZE 7776    // 6^5 dice rolls, ~12.925 bits per word
#define BIP39_SIZE 2048       // 2^11 words, 11.0 bits per word
#define SLIP39_SIZE 1024      // 2^10 words, 10.0 bits per word

typedef enum {
    PassphraseListEFFLong,     // EFF Long wordlist (7776 words, ~12.925 bits/word)
    PassphraseListBIP39,       // BIP-39 Bitcoin wordlist (2048 words, 11.0 bits/word)
    PassphraseListSLIP39,      // SLIP-39 Shamir wordlist (1024 words, 10.0 bits/word)
    PassphraseListCount
} PassphraseListType;

typedef struct {
    PassphraseListType type;
    uint16_t word_count;
    bool is_loaded;
    Storage* storage;
    File* file;              // Keep file open for reading
    char current_word[32];   // Buffer for current word
    
    // Performance optimizations
    uint32_t* line_offsets;  // File offsets for each word (indexed access)
    uint16_t cache_size;     // Number of cached words
    char** word_cache;       // Cache for frequently accessed words
    uint16_t* cache_indices; // Indices of cached words
    bool is_indexed;         // Whether line offsets have been built
    
    // Progress tracking
    bool is_building_index;  // Whether we're currently building the index
    float index_progress;    // Progress of index building (0.0 to 1.0)
} PassphraseSDContext;

// Initialize SD card wordlist context
PassphraseSDContext* flipper_rng_passphrase_sd_alloc(void);

// Free SD card wordlist context
void flipper_rng_passphrase_sd_free(PassphraseSDContext* ctx);

// Check if wordlist exists on SD card
bool flipper_rng_passphrase_sd_exists(PassphraseSDContext* ctx, PassphraseListType type);

// Load wordlist info (count words, verify format)
// Load wordlist from SD card
bool flipper_rng_passphrase_sd_load(PassphraseSDContext* ctx, PassphraseListType type);

// Get a word by index (0-based)
const char* flipper_rng_passphrase_sd_get_word(PassphraseSDContext* ctx, uint16_t index);

// Create default wordlist files on SD card if they don't exist
bool flipper_rng_passphrase_sd_create_defaults(Storage* storage);

// Build index for fast word access (async with progress callback)
bool flipper_rng_passphrase_sd_build_index(PassphraseSDContext* ctx, void (*progress_callback)(float progress, void* context), void* callback_context);

// Get a word by index using indexed access (much faster)
const char* flipper_rng_passphrase_sd_get_word_indexed(PassphraseSDContext* ctx, uint16_t index);

// Check if index is built
bool flipper_rng_passphrase_sd_is_indexed(PassphraseSDContext* ctx);

// Get entropy bits for wordlist type
float flipper_rng_passphrase_sd_entropy_bits(PassphraseListType type, uint8_t num_words);

#include "entropylab_passphrase_sd.h"
#include <furi.h>
#include <string.h>
#include <stdlib.h>

#define TAG "FlipperRNG-DicewareSD"

// Helper function to read a line from file
static bool storage_file_read_line_helper(File* file, char* buffer, size_t buffer_size) {
    if(!file || !buffer || buffer_size == 0) return false;
    
    size_t pos = 0;
    uint8_t byte;
    
    while(pos < buffer_size - 1) {
        if(storage_file_read(file, &byte, 1) != 1) {
            // End of file or error
            if(pos > 0) {
                buffer[pos] = '\0';
                return true;
            }
            return false;
        }
        
        if(byte == '\n') {
            // End of line
            buffer[pos] = '\0';
            return true;
        } else if(byte == '\r') {
            // Skip carriage return
            continue;
        } else {
            buffer[pos++] = byte;
        }
    }
    
    buffer[pos] = '\0';
    return true;
}

// Helper to extract word from EFF format line (skips dice numbers)
static void extract_word_from_line(char* line, char* word, size_t word_size) {
    // EFF format: "11111   word" or just "word"
    // Skip any leading numbers and whitespace
    
    char* ptr = line;
    
    // Skip digits
    while(*ptr && (*ptr >= '0' && *ptr <= '9')) {
        ptr++;
    }
    
    // Skip whitespace and tabs
    while(*ptr && (*ptr == ' ' || *ptr == '\t')) {
        ptr++;
    }
    
    // Copy the word
    size_t i = 0;
    while(*ptr && *ptr != '\n' && *ptr != '\r' && i < word_size - 1) {
        word[i++] = *ptr++;
    }
    word[i] = '\0';
    
    // Trim trailing whitespace
    while(i > 0 && (word[i-1] == ' ' || word[i-1] == '\t')) {
        word[--i] = '\0';
    }
}

PassphraseSDContext* flipper_rng_passphrase_sd_alloc(void) {
    PassphraseSDContext* ctx = malloc(sizeof(PassphraseSDContext));
    if(ctx) {
        ctx->type = PassphraseListEFFLong;
        ctx->word_count = 0;
        ctx->is_loaded = false;
        ctx->storage = furi_record_open(RECORD_STORAGE);
        ctx->file = storage_file_alloc(ctx->storage);
        memset(ctx->current_word, 0, sizeof(ctx->current_word));
        
        // Initialize performance optimization fields
        ctx->line_offsets = NULL;
        ctx->cache_size = 0;
        ctx->word_cache = NULL;
        ctx->cache_indices = NULL;
        ctx->is_indexed = false;
        ctx->is_building_index = false;
        ctx->index_progress = 0.0f;
    }
    return ctx;
}

void flipper_rng_passphrase_sd_free(PassphraseSDContext* ctx) {
    if(ctx) {
        // Free index and cache
        if(ctx->line_offsets) {
            free(ctx->line_offsets);
        }
        if(ctx->word_cache) {
            for(uint16_t i = 0; i < ctx->cache_size; i++) {
                if(ctx->word_cache[i]) {
                    free(ctx->word_cache[i]);
                }
            }
            free(ctx->word_cache);
        }
        if(ctx->cache_indices) {
            free(ctx->cache_indices);
        }
        
        if(ctx->file) {
            if(ctx->is_loaded) {
                storage_file_close(ctx->file);
            }
            storage_file_free(ctx->file);
        }
        if(ctx->storage) {
            furi_record_close(RECORD_STORAGE);
        }
        free(ctx);
    }
}

static const char* flipper_rng_passphrase_sd_get_path(PassphraseListType type) {
    switch(type) {
        case PassphraseListEFFLong:
            return PASSPHRASE_EFF_LONG_PATH;
        case PassphraseListBIP39:
            return PASSPHRASE_BIP39_PATH;
        case PassphraseListSLIP39:
            return PASSPHRASE_SLIP39_PATH;
        default:
            return NULL;
    }
}

static uint16_t flipper_rng_passphrase_sd_get_expected_count(PassphraseListType type) {
    switch(type) {
        case PassphraseListEFFLong:
            return EFF_LONG_SIZE;
        case PassphraseListBIP39:
            return BIP39_SIZE;
        case PassphraseListSLIP39:
            return SLIP39_SIZE;
        default:
            return 0;
    }
}

bool flipper_rng_passphrase_sd_exists(PassphraseSDContext* ctx, PassphraseListType type) {
    if(!ctx || type == PassphraseListEFFLong) return false;
    
    const char* path = flipper_rng_passphrase_sd_get_path(type);
    if(!path) return false;
    
    return storage_file_exists(ctx->storage, path);
}

bool flipper_rng_passphrase_sd_load(PassphraseSDContext* ctx, PassphraseListType type) {
    if(!ctx) return false;
    
    // Close any previously opened file
    if(ctx->is_loaded) {
        storage_file_close(ctx->file);
        ctx->is_loaded = false;
    }
    
    if(type == PassphraseListEFFLong) {
        ctx->type = PassphraseListEFFLong;
        ctx->is_loaded = true;
        return true;
    }
    
    const char* path = flipper_rng_passphrase_sd_get_path(type);
    if(!path) return false;
    
    // Open the file
    if(!storage_file_open(ctx->file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        FURI_LOG_E(TAG, "Failed to open wordlist file: %s", path);
        return false;
    }
    
    // Get expected count
    ctx->word_count = flipper_rng_passphrase_sd_get_expected_count(type);
    ctx->type = type;
    ctx->is_loaded = true;
    
    FURI_LOG_I(TAG, "Opened wordlist %s with %d words", path, ctx->word_count);
    
    return true;
}

const char* flipper_rng_passphrase_sd_get_word(PassphraseSDContext* ctx, uint16_t index) {
    if(!ctx || !ctx->is_loaded) return NULL;
    
    // Bounds check
    if(index >= ctx->word_count) {
        FURI_LOG_E(TAG, "Word index %d out of bounds (max %d)", index, ctx->word_count - 1);
        return NULL;
    }
    
    // Seek to beginning of file
    storage_file_seek(ctx->file, 0, true);
    
    // Read lines until we reach the desired index
    char line_buffer[64];
    for(uint16_t i = 0; i <= index; i++) {
        if(!storage_file_read_line_helper(ctx->file, line_buffer, sizeof(line_buffer))) {
            FURI_LOG_E(TAG, "Failed to read word at index %d", index);
            return NULL;
        }
    }
    
    // Extract just the word from the line (skip dice numbers)
    extract_word_from_line(line_buffer, ctx->current_word, sizeof(ctx->current_word));
    
    return ctx->current_word;
}

bool flipper_rng_passphrase_sd_create_defaults(Storage* storage) {
    // Directory should already exist (created when FAP is installed)
    // Just create the README file
    
    // Create a README file explaining how to add wordlists
    File* file = storage_file_alloc(storage);
    const char* readme_path = "/ext/apps/Tools/FlipperRNG/README_WORDLISTS.txt";
    
    if(storage_file_open(file, readme_path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        const char* readme_content = 
            "FlipperRNG Diceware Wordlists\n"
            "==============================\n\n"
            "Place wordlist files here:\n"
            "- eff_large_wordlist.txt: EFF Long wordlist (7776 words)\n"
            "- eff_short_wordlist_1.txt: EFF Short wordlist #1 (1296 words)\n"
            "- eff_short_wordlist_2_0.txt: EFF Short wordlist #2 (1296 words)\n\n"
            "Download wordlists from:\n"
            "https://www.eff.org/dice\n\n"
            "Format: One word per line\n"
            "The EFF Long list provides ~12.9 bits per word\n"
            "The EFF Short lists provide ~10.3 bits per word\n";
        
        storage_file_write(file, readme_content, strlen(readme_content));
        storage_file_close(file);
    }
    
    storage_file_free(file);
    return true;
}

float flipper_rng_passphrase_sd_entropy_bits(PassphraseListType type, uint8_t num_words) {
    float bits_per_word;
    
    switch(type) {
        case PassphraseListEFFLong:
            // log2(7776) ≈ 12.925
            bits_per_word = 12.925f;
            break;
        case PassphraseListBIP39:
            // log2(2048) = 11.0
            bits_per_word = 11.0f;
            break;
        case PassphraseListSLIP39:
            // log2(1024) = 10.0
            bits_per_word = 10.0f;
            break;
        default:
            // Default to EFF Long
            bits_per_word = 12.925f;
            break;
    }
    
    return num_words * bits_per_word;
}

// Build index for fast word access
bool flipper_rng_passphrase_sd_build_index(PassphraseSDContext* ctx, void (*progress_callback)(float progress, void* context), void* callback_context) {
    if(!ctx || !ctx->is_loaded || ctx->type == PassphraseListEFFLong) {
        return false;
    }
    
    // Don't rebuild if already indexed
    if(ctx->is_indexed) {
        return true;
    }
    
    ctx->is_building_index = true;
    ctx->index_progress = 0.0f;
    
    FURI_LOG_I(TAG, "Building index for %d words...", ctx->word_count);
    
    // Allocate memory for line offsets
    ctx->line_offsets = malloc(ctx->word_count * sizeof(uint32_t));
    if(!ctx->line_offsets) {
        FURI_LOG_E(TAG, "Failed to allocate memory for line offsets");
        ctx->is_building_index = false;
        return false;
    }
    
    // Seek to beginning and build index
    storage_file_seek(ctx->file, 0, true);
    
    char line_buffer[64];
    uint16_t word_index = 0;
    
    while(word_index < ctx->word_count) {
        // Store current file position
        ctx->line_offsets[word_index] = storage_file_tell(ctx->file);
        
        // Read the line to advance file pointer
        if(!storage_file_read_line_helper(ctx->file, line_buffer, sizeof(line_buffer))) {
            FURI_LOG_E(TAG, "Failed to read line %d during indexing", word_index);
            break;
        }
        
        word_index++;
        
        // Update progress periodically
        if(word_index % 100 == 0 || word_index == ctx->word_count) {
            ctx->index_progress = (float)word_index / ctx->word_count;
            if(progress_callback) {
                progress_callback(ctx->index_progress, callback_context);
            }
        }
    }
    
    ctx->is_indexed = (word_index == ctx->word_count);
    ctx->is_building_index = false;
    
    if(ctx->is_indexed) {
        FURI_LOG_I(TAG, "Index built successfully for %d words", ctx->word_count);
    } else {
        FURI_LOG_E(TAG, "Index building failed at word %d", word_index);
        free(ctx->line_offsets);
        ctx->line_offsets = NULL;
    }
    
    return ctx->is_indexed;
}

// Get a word by index using indexed access (much faster)
const char* flipper_rng_passphrase_sd_get_word_indexed(PassphraseSDContext* ctx, uint16_t index) {
    if(!ctx || !ctx->is_loaded || !ctx->is_indexed) {
        return NULL;
    }
    
    // Bounds check
    if(index >= ctx->word_count) {
        FURI_LOG_E(TAG, "Word index %d out of bounds (max %d)", index, ctx->word_count - 1);
        return NULL;
    }
    
    // Seek directly to the word's position
    if(!storage_file_seek(ctx->file, ctx->line_offsets[index], true)) {
        FURI_LOG_E(TAG, "Failed to seek to word %d", index);
        return NULL;
    }
    
    // Read the line
    char line_buffer[64];
    if(!storage_file_read_line_helper(ctx->file, line_buffer, sizeof(line_buffer))) {
        FURI_LOG_E(TAG, "Failed to read word at index %d", index);
        return NULL;
    }
    
    // Extract just the word from the line (skip dice numbers)
    extract_word_from_line(line_buffer, ctx->current_word, sizeof(ctx->current_word));
    
    return ctx->current_word;
}

// Check if index is built
bool flipper_rng_passphrase_sd_is_indexed(PassphraseSDContext* ctx) {
    return ctx && ctx->is_indexed;
}

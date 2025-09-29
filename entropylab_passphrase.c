#include "entropylab.h"  // Must be first to define FlipperRngState
#include "entropylab_passphrase.h"
#include "entropylab_passphrase_sd.h"
#include "entropylab_entropy.h"
#include <furi.h>
#include <string.h>
#include <stdio.h>

#define TAG "FlipperRNG-Passphrase"

// Generate a random index for the wordlist (0 to max_value-1)
uint16_t flipper_rng_passphrase_get_random_index(FlipperRngState* state, uint16_t max_value) {
    // Get 2 bytes of entropy and modulo by max_value
    uint8_t random_bytes[2];
    flipper_rng_extract_random_bytes(state, random_bytes, 2);
    
    uint16_t random_value = (random_bytes[0] << 8) | random_bytes[1];
    
    // To avoid modulo bias, we'll use rejection sampling
    // Max valid value is the largest multiple of max_value that fits in 16 bits
    uint16_t max_valid = (65535 / max_value) * max_value;
    
    // If we got a value that's too large, get new random bytes
    while(random_value >= max_valid) {
        flipper_rng_extract_random_bytes(state, random_bytes, 2);
        random_value = (random_bytes[0] << 8) | random_bytes[1];
    }
    
    return random_value % max_value;
}

// Generate a diceware passphrase
void flipper_rng_passphrase_generate(
    FlipperRngState* state, 
    char* passphrase, 
    size_t max_length, 
    uint8_t num_words) {
    
    if(!state || !passphrase || max_length == 0) {
        FURI_LOG_E(TAG, "Invalid parameters for diceware generation");
        return;
    }
    
    // Clamp number of words to valid range
    if(num_words < PASSPHRASE_MIN_WORDS) {
        num_words = PASSPHRASE_MIN_WORDS;
    } else if(num_words > PASSPHRASE_MAX_WORDS) {
        num_words = PASSPHRASE_MAX_WORDS;
    }
    
    // Clear the passphrase buffer
    memset(passphrase, 0, max_length);
    
    size_t current_pos = 0;
    
    for(uint8_t i = 0; i < num_words; i++) {
        // Get random word index
        uint16_t word_index = flipper_rng_passphrase_get_random_index(state, PASSPHRASE_WORDLIST_SIZE);
        
        // Get the word
        const char* word = passphrase_wordlist[word_index];
        size_t word_len = strlen(word);
        
        // Check if we have space for this word plus a space (or null terminator)
        size_t space_needed = word_len + ((i < num_words - 1) ? 1 : 0);
        if(current_pos + space_needed >= max_length) {
            FURI_LOG_W(TAG, "Passphrase buffer too small, truncating at %d words", i);
            break;
        }
        
        // Copy the word
        memcpy(passphrase + current_pos, word, word_len);
        current_pos += word_len;
        
        // Add space between words (but not after the last word)
        if(i < num_words - 1) {
            passphrase[current_pos] = ' ';
            current_pos++;
        }
    }
    
    // Ensure null termination
    passphrase[current_pos] = '\0';
    
    FURI_LOG_I(TAG, "Generated %d-word passphrase: %s", num_words, passphrase);
}

// Generate a diceware passphrase from SD card wordlist
void flipper_rng_passphrase_generate_sd(
    FlipperRngState* state,
    void* sd_context,
    char* passphrase,
    size_t max_length,
    uint8_t num_words) {
    
    PassphraseSDContext* ctx = (PassphraseSDContext*)sd_context;
    
    if(!state || !ctx || !passphrase || max_length == 0) {
        FURI_LOG_E(TAG, "Invalid parameters for SD diceware generation");
        return;
    }
    
    // Clamp number of words to valid range
    if(num_words < PASSPHRASE_MIN_WORDS) {
        num_words = PASSPHRASE_MIN_WORDS;
    } else if(num_words > PASSPHRASE_MAX_WORDS) {
        num_words = PASSPHRASE_MAX_WORDS;
    }
    
    // Clear the passphrase buffer
    memset(passphrase, 0, max_length);
    
    size_t current_pos = 0;
    uint16_t word_count = ctx->word_count;
    
    for(uint8_t i = 0; i < num_words; i++) {
        // Get random word index
        uint16_t word_index = flipper_rng_passphrase_get_random_index(state, word_count);
        
        // Get the word from SD card using indexed access if available
        const char* word;
        if(flipper_rng_passphrase_sd_is_indexed(ctx)) {
            word = flipper_rng_passphrase_sd_get_word_indexed(ctx, word_index);
        } else {
            // Fallback to sequential access (slower)
            word = flipper_rng_passphrase_sd_get_word(ctx, word_index);
        }
        
        if(!word) {
            FURI_LOG_E(TAG, "Failed to get word at index %d", word_index);
            break;
        }
        
        size_t word_len = strlen(word);
        
        // Check if we have space for this word plus a space (or null terminator)
        size_t space_needed = word_len + ((i < num_words - 1) ? 1 : 0);
        if(current_pos + space_needed >= max_length) {
            FURI_LOG_W(TAG, "Passphrase buffer too small, truncating at %d words", i);
            break;
        }
        
        // Copy the word
        memcpy(passphrase + current_pos, word, word_len);
        current_pos += word_len;
        
        // Add space between words (but not after the last word)
        if(i < num_words - 1) {
            passphrase[current_pos] = ' ';
            current_pos++;
        }
    }
    
    // Ensure null termination
    passphrase[current_pos] = '\0';
    
    FURI_LOG_I(TAG, "Generated %d-word passphrase from SD wordlist", num_words);
}

// Calculate entropy bits for a given number of words
float flipper_rng_passphrase_entropy_bits(uint8_t num_words) {
    // Each word from a 1848-word embedded list provides log2(1848) ≈ 10.85 bits of entropy
    return num_words * 10.85f;
}

#include "entropylab.h"  // Must be first to define FlipperRngState
#include "entropylab_passphrase.h"
#include "entropylab_passphrase_sd.h"
#include "entropylab_entropy.h"
#include "entropylab_secure.h"
#include "entropylab_log.h"
#include <furi.h>
#include <string.h>
#include <stdio.h>

#define TAG "EntropyLab-Passphrase"

// Generate a random index for the wordlist (0 to max_value-1)
// Uses constant-time rejection sampling to prevent timing side-channel attacks
uint16_t flipper_rng_passphrase_get_random_index(FlipperRngState* state, uint16_t max_value) {
    // To avoid modulo bias, we'll use rejection sampling
    // Max valid value is the largest multiple of max_value that fits in 16 bits
    uint16_t max_valid = (65535 / max_value) * max_value;
    
    // Calculate rejection probability to determine max iterations needed
    // Worst case: EFF wordlist (7776) has ~3.1% rejection rate
    // 4 iterations gives us 99.99%+ success probability
    const uint8_t MAX_ITERATIONS = 4;
    
    uint8_t random_bytes[2];
    uint16_t random_value = 0;
    bool found_valid = false;
    
    // CONSTANT-TIME: Always do exactly MAX_ITERATIONS, regardless of when we find valid value
    // This prevents timing side-channel attacks that could leak information about rejected values
    for(uint8_t iteration = 0; iteration < MAX_ITERATIONS; iteration++) {
        // Get 2 bytes of entropy
        flipper_rng_extract_random_bytes(state, random_bytes, 2);
        uint16_t candidate = (random_bytes[0] << 8) | random_bytes[1];
        
        // Check if this value is valid (no modulo bias)
        bool is_valid = (candidate < max_valid);
        
        // Use constant-time selection: update random_value only on first valid candidate
        // This ensures we always do the same number of operations
        if(is_valid && !found_valid) {
            random_value = candidate;
            found_valid = true;
        }
        
        // Always wipe the random bytes, regardless of whether we used them
        secure_wipe(random_bytes, sizeof(random_bytes));
    }
    
    // Extremely unlikely (< 0.01% chance), but if we didn't find valid value after MAX_ITERATIONS,
    // use the last candidate modulo max_value (introduces tiny bias but prevents infinite loop)
    if(!found_valid) {
        LOG_W(TAG, "Rejection sampling failed after %d iterations, using fallback", MAX_ITERATIONS);
        flipper_rng_extract_random_bytes(state, random_bytes, 2);
        random_value = (random_bytes[0] << 8) | random_bytes[1];
        secure_wipe(random_bytes, sizeof(random_bytes));
    }
    
    uint16_t result = random_value % max_value;
    
    return result;
}

// Generate a diceware passphrase - now uses only SD wordlists
void flipper_rng_passphrase_generate(
    FlipperRngState* state, 
    char* passphrase, 
    size_t max_length, 
    uint8_t num_words) {
    
    UNUSED(state);
    UNUSED(num_words);
    
    // This function is no longer used - all passphrases use SD wordlists
    // Use flipper_rng_passphrase_generate_sd() instead
    LOG_E(TAG, "Embedded wordlist removed - use SD wordlist generation");
    
    if(passphrase && max_length > 0) {
        snprintf(passphrase, max_length, "Use SD wordlist mode");
    }
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
        LOG_E(TAG, "Invalid parameters for SD diceware generation");
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
            LOG_E(TAG, "Failed to get word at index %d", word_index);
            break;
        }
        
        size_t word_len = strlen(word);
        
        // Check if we have space for this word plus a space (or null terminator)
        size_t space_needed = word_len + ((i < num_words - 1) ? 1 : 0);
        if(current_pos + space_needed >= max_length) {
            LOG_W(TAG, "Passphrase buffer too small, truncating at %d words", i);
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
    
    LOG_D(TAG, "Generated %d-word passphrase from SD wordlist", num_words);
    
    // Note: Passphrase is NOT wiped here - it needs to be displayed to user
    // Wipe will happen when:
    // 1. User generates a new passphrase (cleared at start of this function)
    // 2. User exits the passphrase view (exit callback)
    // 3. App closes (view free)
}

// Calculate entropy bits for a given number of words
float flipper_rng_passphrase_entropy_bits(uint8_t num_words) {
    // Each word from the EFF long wordlist (7776 words) provides log2(7776) ≈ 12.925 bits of entropy
    return num_words * 12.925f;
}

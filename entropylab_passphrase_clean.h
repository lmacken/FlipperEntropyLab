#pragma once

#include <stdint.h>
#include <stddef.h>

// Forward declaration - check if already defined
#ifndef FLIPPER_RNG_STATE_DEFINED
struct FlipperRngState;
typedef struct FlipperRngState FlipperRngState;
#endif

// Passphrase configuration
#define PASSPHRASE_MIN_WORDS 3
#define PASSPHRASE_MAX_WORDS 12
#define PASSPHRASE_DEFAULT_WORDS 6

// Wordlists are shipped as files with the app
// Official EFF wordlists provide the highest quality and security

// Function prototypes
void flipper_rng_passphrase_generate(
    FlipperRngState* state, 
    char* passphrase, 
    size_t max_length, 
    uint8_t num_words);

void flipper_rng_passphrase_generate_sd(
    FlipperRngState* state,
    void* sd_context,  // PassphraseSDContext*
    char* passphrase,
    size_t max_length,
    uint8_t num_words);

uint16_t flipper_rng_passphrase_get_random_index(FlipperRngState* state, uint16_t max_value);

float flipper_rng_passphrase_entropy_bits(uint8_t num_words);

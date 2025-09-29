#pragma once

#include "entropylab.h"
#include <furi_hal_random.h>
#include <furi_hal_adc.h>
#include <furi_hal_power.h>
#include <furi_hal_cortex.h>
#include <furi_hal_infrared.h>
#include <infrared_worker.h>

// Entropy mixing constants (based on LFSR)
#define ENTROPY_MIX_TAP1 0x80200003
#define ENTROPY_MIX_TAP2 0x40100001
#define ENTROPY_MIX_TAP3 0x20080000
#define ENTROPY_MIX_TAP4 0x10040000

// Von Neumann extractor for debiasing
typedef struct {
    uint8_t prev_bit;
    bool has_prev;
} VonNeumannExtractor;

// Entropy source statistics
typedef struct {
    uint32_t samples;
    uint32_t bits_extracted;
    float shannon_entropy;
} EntropySourceStats;

// Function prototypes for entropy collection
void flipper_rng_init_entropy_sources(FlipperRngState* state);
void flipper_rng_deinit_entropy_sources(FlipperRngState* state);

// Individual entropy collectors - High-quality, environment-independent sources
uint32_t flipper_rng_get_hardware_random(void);
uint32_t flipper_rng_get_adc_noise(FuriHalAdcHandle* handle);
uint32_t flipper_rng_get_battery_noise(void);
uint32_t flipper_rng_get_temperature_noise(void);
uint32_t flipper_rng_get_subghz_rssi_noise(void);
uint32_t flipper_rng_get_subghz_rssi_noise_ex(FlipperRngState* state);
uint32_t flipper_rng_get_infrared_noise(void);

// Entropy processing
void flipper_rng_add_entropy(FlipperRngState* state, uint32_t entropy, uint8_t bits);
void flipper_rng_mix_entropy_pool(FlipperRngState* state);
uint8_t flipper_rng_extract_random_byte(FlipperRngState* state);
void flipper_rng_extract_random_bytes(FlipperRngState* state, uint8_t* buffer, size_t count);

// Von Neumann debiasing
void von_neumann_init(VonNeumannExtractor* extractor);
bool von_neumann_extract(VonNeumannExtractor* extractor, uint8_t input_bit, uint8_t* output_bit);

// Entropy quality estimation
float flipper_rng_estimate_entropy_quality(uint8_t* data, size_t length);
void flipper_rng_update_quality_metric(FlipperRngState* state);

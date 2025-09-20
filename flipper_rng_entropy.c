#include "flipper_rng_entropy.h"
#include <math.h>

#define TAG "FlipperRNG"

// Initialize entropy sources
void flipper_rng_init_entropy_sources(FlipperRngState* state) {
    FURI_LOG_I(TAG, "Initializing entropy sources: 0x%02lX", (unsigned long)state->entropy_sources);
    
    // Initialize ADC for noise collection
    if(state->entropy_sources & EntropySourceADC) {
        FURI_LOG_I(TAG, "Acquiring ADC handle...");
        state->adc_handle = furi_hal_adc_acquire();
        if(state->adc_handle) {
            FURI_LOG_I(TAG, "Configuring ADC...");
            furi_hal_adc_configure(state->adc_handle);
            FURI_LOG_I(TAG, "ADC configured successfully");
        } else {
            FURI_LOG_W(TAG, "Failed to acquire ADC handle, disabling ADC entropy");
            state->entropy_sources &= ~EntropySourceADC;
        }
    }
    
    // Initialize other hardware as needed
    FURI_LOG_I(TAG, "Entropy sources initialized: 0x%02lX", (unsigned long)state->entropy_sources);
}

void flipper_rng_deinit_entropy_sources(FlipperRngState* state) {
    if(state->adc_handle) {
        furi_hal_adc_release(state->adc_handle);
        state->adc_handle = NULL;
    }
}

// Get hardware random number
uint32_t flipper_rng_get_hardware_random(void) {
    return furi_hal_random_get();
}

// Get ADC noise (least significant bits contain noise)
uint32_t flipper_rng_get_adc_noise(FuriHalAdcHandle* handle) {
    if(!handle) return 0;
    
    uint32_t noise = 0;
    // Sample multiple ADC channels and XOR the LSBs
    for(int i = 0; i < 4; i++) {
        uint16_t adc_val = furi_hal_adc_read(handle, FuriHalAdcChannelVREFINT);
        noise = (noise << 8) | (adc_val & 0xFF);
        furi_delay_us(10); // Small delay between samples
    }
    
    return noise;
}

// Get timing jitter from high-resolution timer
uint32_t flipper_rng_get_timing_jitter(void) {
    static uint32_t last_time = 0;
    uint32_t current_time = DWT->CYCCNT;
    uint32_t jitter = current_time - last_time;
    last_time = current_time;
    
    // Mix with microsecond counter
    jitter ^= (furi_get_tick() << 16);
    jitter ^= (DWT->CYCCNT & 0xFFFF);
    
    return jitter;
}

// Get CPU execution jitter
uint32_t flipper_rng_get_cpu_jitter(void) {
    uint32_t start = DWT->CYCCNT;
    
    // Perform some operations that have variable timing
    volatile uint32_t dummy = 0;
    for(volatile int i = 0; i < 100; i++) {
        dummy += i * i;
        dummy ^= (dummy >> 3);
    }
    
    uint32_t end = DWT->CYCCNT;
    return (end - start) ^ dummy;
}

// Get battery voltage noise
uint32_t flipper_rng_get_battery_noise(void) {
    float voltage = furi_hal_power_get_battery_voltage(FuriHalPowerICFuelGauge);
    float current = furi_hal_power_get_battery_current(FuriHalPowerICFuelGauge);
    
    // Convert to integer and use LSBs as noise
    union { float f; uint32_t i; } voltage_conv = { .f = voltage };
    union { float f; uint32_t i; } current_conv = { .f = current };
    
    return (voltage_conv.i & 0xFFFF) ^ ((current_conv.i & 0xFFFF) << 16);
}

// Get temperature noise
uint32_t flipper_rng_get_temperature_noise(void) {
    float temp = furi_hal_power_get_battery_temperature(FuriHalPowerICFuelGauge);
    union { float f; uint32_t i; } temp_conv = { .f = temp };
    
    // Mix with battery charge percentage
    uint8_t charge = furi_hal_power_get_pct();
    
    return (temp_conv.i & 0xFFFF) ^ (charge << 8) ^ (furi_get_tick() << 16);
}

// Add entropy to the pool
void flipper_rng_add_entropy(FlipperRngState* state, uint32_t entropy, uint8_t bits) {
    furi_mutex_acquire(state->mutex, FuriWaitForever);
    
    // Add entropy bytes to pool
    for(int i = 0; i < 4 && bits > 0; i++) {
        uint8_t byte = (entropy >> (i * 8)) & 0xFF;
        
        // XOR with existing pool data
        state->entropy_pool[state->entropy_pool_pos] ^= byte;
        
        // Rotate through pool
        state->entropy_pool_pos = (state->entropy_pool_pos + 1) % RNG_POOL_SIZE;
        
        bits = (bits > 8) ? (bits - 8) : 0;
    }
    
    state->samples_collected++;
    state->last_entropy_bits = bits;
    
    furi_mutex_release(state->mutex);
}

// Mix entropy pool using LFSR-based mixing
void flipper_rng_mix_entropy_pool(FlipperRngState* state) {
    furi_mutex_acquire(state->mutex, FuriWaitForever);
    
    uint32_t lfsr = 0xACE1u;
    
    for(int round = 0; round < 4; round++) {
        for(size_t i = 0; i < RNG_POOL_SIZE; i++) {
            // LFSR step
            uint32_t bit = ((lfsr >> 0) ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 5)) & 1;
            lfsr = (lfsr >> 1) | (bit << 15);
            
            // Mix with pool
            state->entropy_pool[i] ^= (lfsr & 0xFF);
            
            // Diffusion step
            if(i > 0) {
                state->entropy_pool[i] ^= state->entropy_pool[i - 1];
            }
            if(i < RNG_POOL_SIZE - 1) {
                state->entropy_pool[i] ^= state->entropy_pool[i + 1] >> 1;
            }
        }
    }
    
    furi_mutex_release(state->mutex);
}

// Extract a random byte from the pool
uint8_t flipper_rng_extract_random_byte(FlipperRngState* state) {
    furi_mutex_acquire(state->mutex, FuriWaitForever);
    
    // Simple extraction: XOR multiple pool positions
    uint8_t result = 0;
    size_t positions[8] = {
        state->entropy_pool_pos,
        (state->entropy_pool_pos + 511) % RNG_POOL_SIZE,
        (state->entropy_pool_pos + 1023) % RNG_POOL_SIZE,
        (state->entropy_pool_pos + 1531) % RNG_POOL_SIZE,
        (state->entropy_pool_pos + 2047) % RNG_POOL_SIZE,
        (state->entropy_pool_pos + 2557) % RNG_POOL_SIZE,
        (state->entropy_pool_pos + 3067) % RNG_POOL_SIZE,
        (state->entropy_pool_pos + 3583) % RNG_POOL_SIZE,
    };
    
    for(int i = 0; i < 8; i++) {
        result ^= state->entropy_pool[positions[i]];
    }
    
    // Advance position
    state->entropy_pool_pos = (state->entropy_pool_pos + 1) % RNG_POOL_SIZE;
    state->bytes_generated++;
    
    furi_mutex_release(state->mutex);
    
    return result;
}

// Von Neumann debiasing implementation
void von_neumann_init(VonNeumannExtractor* extractor) {
    extractor->prev_bit = 0;
    extractor->has_prev = false;
}

bool von_neumann_extract(VonNeumannExtractor* extractor, uint8_t input_bit, uint8_t* output_bit) {
    input_bit = input_bit & 1;
    
    if(!extractor->has_prev) {
        extractor->prev_bit = input_bit;
        extractor->has_prev = true;
        return false;
    }
    
    if(extractor->prev_bit != input_bit) {
        *output_bit = extractor->prev_bit;
        extractor->has_prev = false;
        return true;
    }
    
    extractor->has_prev = false;
    return false;
}

// Estimate entropy quality using Shannon entropy
float flipper_rng_estimate_entropy_quality(uint8_t* data, size_t length) {
    if(length == 0) return 0.0f;
    
    // Count byte frequencies
    uint32_t freq[256] = {0};
    for(size_t i = 0; i < length; i++) {
        freq[data[i]]++;
    }
    
    // Calculate Shannon entropy
    float entropy = 0.0f;
    for(int i = 0; i < 256; i++) {
        if(freq[i] > 0) {
            float p = (float)freq[i] / (float)length;
            entropy -= p * log2f(p);
        }
    }
    
    // Normalize to 0-1 range (max entropy is 8 bits)
    return entropy / 8.0f;
}

// Update quality metric
void flipper_rng_update_quality_metric(FlipperRngState* state) {
    furi_mutex_acquire(state->mutex, FuriWaitForever);
    
    // Sample a portion of the pool for quality estimation
    const size_t sample_size = 256;
    uint8_t sample[sample_size];
    
    size_t start_pos = (state->entropy_pool_pos + RNG_POOL_SIZE - sample_size) % RNG_POOL_SIZE;
    for(size_t i = 0; i < sample_size; i++) {
        sample[i] = state->entropy_pool[(start_pos + i) % RNG_POOL_SIZE];
    }
    
    state->entropy_quality = flipper_rng_estimate_entropy_quality(sample, sample_size);
    
    furi_mutex_release(state->mutex);
}

// Main entropy collection function
void flipper_rng_collect_hardware_rng(FlipperRngState* state) {
    if(state->entropy_sources & EntropySourceHardwareRNG) {
        uint32_t hw_random = flipper_rng_get_hardware_random();
        flipper_rng_add_entropy(state, hw_random, 32);
    }
}

void flipper_rng_collect_adc_entropy(FlipperRngState* state) {
    if((state->entropy_sources & EntropySourceADC) && state->adc_handle) {
        uint32_t adc_noise = flipper_rng_get_adc_noise(state->adc_handle);
        flipper_rng_add_entropy(state, adc_noise, 16); // ADC noise has less entropy
    }
}

void flipper_rng_collect_timing_entropy(FlipperRngState* state) {
    if(state->entropy_sources & EntropySourceTiming) {
        uint32_t timing_jitter = flipper_rng_get_timing_jitter();
        flipper_rng_add_entropy(state, timing_jitter, 8); // Timing has moderate entropy
    }
}

void flipper_rng_collect_cpu_jitter(FlipperRngState* state) {
    if(state->entropy_sources & EntropySourceCPUJitter) {
        uint32_t cpu_jitter = flipper_rng_get_cpu_jitter();
        flipper_rng_add_entropy(state, cpu_jitter, 4); // CPU jitter has low entropy
    }
}

void flipper_rng_collect_battery_entropy(FlipperRngState* state) {
    if(state->entropy_sources & EntropySourceBatteryVoltage) {
        uint32_t battery_noise = flipper_rng_get_battery_noise();
        flipper_rng_add_entropy(state, battery_noise, 8);
    }
}

void flipper_rng_collect_temperature_entropy(FlipperRngState* state) {
    if(state->entropy_sources & EntropySourceTemperature) {
        uint32_t temp_noise = flipper_rng_get_temperature_noise();
        flipper_rng_add_entropy(state, temp_noise, 4); // Temperature changes slowly
    }
}

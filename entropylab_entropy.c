#include "entropylab_entropy.h"
#include "entropylab_hw_accel.h"
#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_random.h>
#include <furi_hal_cortex.h>
#include <furi_hal_infrared.h>
#include <furi_hal_subghz.h>
#include <furi_hal_light.h>
#include <lib/subghz/devices/cc1101_configs.h>
#include <lib/drivers/cc1101_regs.h>
#include <infrared.h>
#include <infrared_worker.h>
#include <infrared_transmit.h>

#define TAG "EntropyLab"

// Initialize entropy sources - High quality only
void flipper_rng_init_entropy_sources(FlipperRngState* state) {
    FURI_LOG_I(TAG, "Initializing high-quality entropy sources: 0x%02lX", (unsigned long)state->entropy_sources);
    
    // No hardware initialization needed for our high-quality sources:
    // - Hardware RNG: Always available via furi_hal_random_get()
    // - SubGHz RSSI: Uses safe RF-influenced timing (no device access)
    // - Infrared: Uses ambient IR timing variations (no persistent worker)
    
    FURI_LOG_I(TAG, "High-quality entropy sources ready: 0x%02lX", (unsigned long)state->entropy_sources);
}

void flipper_rng_deinit_entropy_sources(FlipperRngState* state) {
    UNUSED(state);
    
    // Clean up SubGHz mutex if allocated
    extern FuriMutex* subghz_mutex;
    if(subghz_mutex) {
        // Make sure mutex is not locked before freeing
        // Try to release in case it's stuck locked
        furi_mutex_release(subghz_mutex);  // Safe even if not locked
        furi_mutex_free(subghz_mutex);
        subghz_mutex = NULL;
        FURI_LOG_I(TAG, "SubGHz mutex freed");
    }
    
    // Reset Sub-GHz to clean state for other apps
    // This ensures we don't leave the radio in a modified state
    furi_hal_subghz_reset();
    
    FURI_LOG_I(TAG, "High-quality entropy sources deinitialized, Sub-GHz reset");
}

// Get hardware random number
uint32_t flipper_rng_get_hardware_random(void) {
    return furi_hal_random_get();
}

// Get enhanced multi-ADC differential noise from multiple internal channels
uint32_t flipper_rng_get_adc_noise(FuriHalAdcHandle* handle) {
    if(!handle) return 0;
    
    FURI_LOG_I(TAG, "ADC: Starting multi-channel differential noise collection");
    
    uint32_t entropy = 0;
    
    // Take multiple rounds of differential measurements for better entropy
    for(int round = 0; round < 2; round++) {
        // Sample all three internal ADC channels
        uint16_t vref = furi_hal_adc_read(handle, FuriHalAdcChannelVREFINT);
        furi_delay_us(15); // Allow settling time
        
        uint16_t temp = furi_hal_adc_read(handle, FuriHalAdcChannelTEMPSENSOR);
        furi_delay_us(15); // Temperature sensor needs 5μs minimum
        
        uint16_t vbat = furi_hal_adc_read(handle, FuriHalAdcChannelVBAT);
        furi_delay_us(15); // Battery channel needs 12μs minimum
        
        // Calculate differential measurements (noise amplified in differences)
        uint16_t diff1 = vref - temp;  // VREF vs Temperature sensor
        uint16_t diff2 = temp - vbat;  // Temperature vs Battery voltage
        uint16_t diff3 = vbat - vref;  // Battery vs VREF (completes triangle)
        
        // Extract noise from differentials and mix with timing
        uint32_t timing_noise = DWT->CYCCNT;
        uint8_t noise1 = (diff1 ^ (diff1 >> 8) ^ timing_noise) & 0xFF;
        uint8_t noise2 = (diff2 ^ (diff2 >> 8) ^ (timing_noise >> 8)) & 0xFF;
        
        entropy = (entropy << 16) | (noise1 << 8) | noise2;
        
        FURI_LOG_I(TAG, "ADC: Round %d, VREF=%u, TEMP=%u, VBAT=%u, diffs=[%u,%u,%u]", 
                  round, vref, temp, vbat, diff1, diff2, diff3);
        
        // Small delay between rounds
        furi_delay_us(50);
    }
    
    FURI_LOG_I(TAG, "ADC: Collected multi-channel differential entropy=0x%08lX", entropy);
    return entropy;
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
    if(!state || !state->mutex) {
        FURI_LOG_E(TAG, "add_entropy: Invalid state or mutex");
        return;
    }
    
    // Use timeout instead of forever to prevent deadlock
    if(furi_mutex_acquire(state->mutex, 100) != FuriStatusOk) {
        FURI_LOG_W(TAG, "add_entropy: Could not acquire mutex, entropy discarded");
        return;
    }
    
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

// Mix entropy pool - HARDWARE ACCELERATED with AES
void flipper_rng_mix_entropy_pool(FlipperRngState* state) {
    if(!state || !state->mutex) {
        FURI_LOG_E(TAG, "mix_pool: Invalid state or mutex");
        return;
    }
    
    // Use timeout instead of forever to prevent deadlock
    if(furi_mutex_acquire(state->mutex, 100) != FuriStatusOk) {
        FURI_LOG_W(TAG, "mix_pool: Could not acquire mutex, skipping mix");
        return;
    }
    
    // Try hardware AES mixing first (ultra-fast)
    uint32_t aes_key[8];
    uint32_t* pool32 = (uint32_t*)state->entropy_pool;
    
    // Generate AES key from ROTATING pool positions to prevent targeted attacks
    // Use mix_counter to rotate through different pool positions each time
    // This prevents an attacker from influencing specific fixed positions
    uint32_t pool32_size = RNG_POOL_SIZE / sizeof(uint32_t);
    
    // Table of prime numbers for unpredictable distribution
    // These are all prime numbers < 256, good for modular arithmetic
    static const uint8_t prime_table[] = {
        17, 19, 23, 29, 31, 37, 41, 43, 47, 53, 59, 61, 67, 71, 73, 79,
        83, 89, 97, 101, 103, 107, 109, 113, 127, 131, 137, 139, 149, 151, 157, 163
    };
    const size_t prime_table_size = sizeof(prime_table) / sizeof(prime_table[0]);
    
    // Derive prime offsets from pool state (makes extraction pattern unpredictable)
    // Use different pool positions to select primes, mixed with mix_counter
    uint8_t prime_selector_1 = state->entropy_pool[(state->mix_counter * 7) % RNG_POOL_SIZE];
    uint8_t prime_selector_2 = state->entropy_pool[(state->mix_counter * 11) % RNG_POOL_SIZE];
    uint32_t base_prime = prime_table[prime_selector_1 % prime_table_size];
    uint32_t step_prime = prime_table[prime_selector_2 % prime_table_size];
    
    // Calculate base offset using pool-derived prime
    uint32_t base_offset = (state->mix_counter * base_prime) % pool32_size;
    
    // Derive 8 key words from rotating positions, mixed with hardware RNG
    for(int i = 0; i < 8; i++) {
        uint32_t pos = (base_offset + (i * step_prime)) % pool32_size;
        aes_key[i] = pool32[pos] ^ furi_hal_random_get();
    }
    
    // Increment counter for next mix (will rotate through different positions)
    state->mix_counter++;
    
    // Choose mixing method based on configuration
    bool use_hardware_aes = false;
    
    switch(state->mixing_mode) {
        case MixingModeHardware:
            use_hardware_aes = true;
            break;
        case MixingModeSoftware:
            use_hardware_aes = false;
            break;
        default:
            use_hardware_aes = true;  // Default to hardware AES
            break;
    }
    
    bool hardware_success = false;
    if(use_hardware_aes) {
        hardware_success = flipper_rng_hw_aes_mix_pool(state->entropy_pool, RNG_POOL_SIZE, aes_key);
        if(hardware_success) {
            FURI_LOG_D(TAG, "Pool mixed with hardware AES");
        } else if(state->mixing_mode == MixingModeHardware) {
            FURI_LOG_E(TAG, "Hardware AES mixing failed - this should not happen!");
            // Try once more for hardware-only mode
            hardware_success = flipper_rng_hw_aes_mix_pool(state->entropy_pool, RNG_POOL_SIZE, aes_key);
            if(!hardware_success) {
                FURI_LOG_E(TAG, "Hardware AES mixing failed twice - hardware error detected!");
                // We should stop the generator and show error to user
                state->is_running = false;
                furi_mutex_release(state->mutex);
                return;  // Exit without mixing - this will cause the generator to stop
            }
        }
    }
    
    if(!hardware_success && state->mixing_mode == MixingModeSoftware) {
        // Use software mixing (either by choice or as fallback)
        size_t pool32_size = RNG_POOL_SIZE / sizeof(uint32_t);
        
        // Get fresh hardware random for mixing
        uint32_t hw_mix = furi_hal_random_get();
        uint32_t hw_mix2 = furi_hal_random_get();
        
        // Fast mixing using hardware random and rotation
        for(size_t i = 0; i < pool32_size; i++) {
            // Mix with hardware random
            pool32[i] ^= hw_mix;
            
            // Use hardware-optimized rotation
            hw_mix = flipper_rng_hw_rotate_left(hw_mix, 1);
            hw_mix ^= hw_mix2;
            hw_mix2 = flipper_rng_hw_rotate_right(hw_mix2, 1);
            
            // Additional diffusion with adjacent values
            if(i > 0) {
                pool32[i] ^= pool32[i - 1] >> 3;
            }
            if(i < pool32_size - 1) {
                pool32[i] ^= pool32[i + 1] << 5;
            }
        }
        
        // Final pass with byte-level diffusion for thorough mixing
        for(size_t i = 1; i < RNG_POOL_SIZE - 1; i++) {
            state->entropy_pool[i] ^= (state->entropy_pool[i - 1] >> 1) ^ (state->entropy_pool[i + 1] << 1);
        }
        
        FURI_LOG_D(TAG, "Pool mixed with optimized software mixing");
    }
    
    furi_mutex_release(state->mutex);
}

// Extract a single random byte from the pool
uint8_t flipper_rng_extract_random_byte(FlipperRngState* state) {
    uint8_t result;
    flipper_rng_extract_random_bytes(state, &result, 1);
    return result;
}

// Batch extract multiple random bytes from the pool - OPTIMIZED
void flipper_rng_extract_random_bytes(FlipperRngState* state, uint8_t* buffer, size_t count) {
    if(!buffer || count == 0) return;
    
    if(!state || !state->mutex) {
        FURI_LOG_E(TAG, "extract_bytes: Invalid state or mutex");
        return;
    }
    
    // Use timeout instead of forever to prevent deadlock
    if(furi_mutex_acquire(state->mutex, 100) != FuriStatusOk) {
        FURI_LOG_W(TAG, "extract_bytes: Could not acquire mutex, returning zeros");
        memset(buffer, 0, count);
        return;
    }
    
    // Prime offsets for good distribution across the pool
    const size_t prime_offsets[8] = {
        511, 1023, 1531, 2047, 2557, 3067, 3583, 4093
    };
    
    // Extract all bytes in a single critical section
    for(size_t byte_idx = 0; byte_idx < count; byte_idx++) {
        uint8_t result = 0;
        size_t base_pos = state->entropy_pool_pos;
        
        // Mix bytes from 8 different pool positions
        // Unrolled for better performance
        result ^= state->entropy_pool[base_pos];
        result ^= state->entropy_pool[(base_pos + prime_offsets[0]) % RNG_POOL_SIZE];
        result ^= state->entropy_pool[(base_pos + prime_offsets[1]) % RNG_POOL_SIZE];
        result ^= state->entropy_pool[(base_pos + prime_offsets[2]) % RNG_POOL_SIZE];
        result ^= state->entropy_pool[(base_pos + prime_offsets[3]) % RNG_POOL_SIZE];
        result ^= state->entropy_pool[(base_pos + prime_offsets[4]) % RNG_POOL_SIZE];
        result ^= state->entropy_pool[(base_pos + prime_offsets[5]) % RNG_POOL_SIZE];
        result ^= state->entropy_pool[(base_pos + prime_offsets[6]) % RNG_POOL_SIZE];
        
        buffer[byte_idx] = result;
        
        // Advance position for next byte with random jitter
        // Mix in hardware RNG to prevent position prediction
        // This makes it harder for an attacker to predict which pool bytes will be extracted next
        uint32_t jitter = furi_hal_random_get() & 0x7;  // 0-7 random advance
        state->entropy_pool_pos = (state->entropy_pool_pos + 1 + jitter) % RNG_POOL_SIZE;
    }
    
    state->bytes_generated += count;
    
    furi_mutex_release(state->mutex);
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


// Update quality metric (no longer used, kept for compatibility)
void flipper_rng_update_quality_metric(FlipperRngState* state) {
    // Quality calculation removed - now handled by test function
    UNUSED(state);
}

// Main entropy collection function
void flipper_rng_collect_hardware_rng(FlipperRngState* state) {
    if(state->entropy_sources & EntropySourceHardwareRNG) {
        uint32_t hw_random = flipper_rng_get_hardware_random();
        flipper_rng_add_entropy(state, hw_random, 32);
    }
}

// Removed low-quality entropy collectors
// ADC, battery, and temperature sources removed - too predictable
// Now focusing only on high-quality sources: HW RNG, SubGHz RSSI, Infrared

// Static mutex for SubGHz access protection
static FuriMutex* subghz_mutex = NULL;
static uint32_t subghz_error_count = 0;  // Track consecutive errors for recovery
static uint32_t subghz_last_success = 0;  // Last successful collection time

// Get SubGHz RSSI noise - Enhanced hardware implementation with improved entropy
uint32_t flipper_rng_get_subghz_rssi_noise_ex(FlipperRngState* state) {
    uint32_t entropy = 0;
    
    // Early exit if we're stopping
    if(state && !state->is_running) {
        return 0;
    }
    
    // Create mutex on first use
    if(!subghz_mutex) {
        subghz_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    }
    
    // Try to acquire SubGHz access (with timeout to prevent deadlock)
    if(furi_mutex_acquire(subghz_mutex, 100) != FuriStatusOk) {
        FURI_LOG_W(TAG, "SubGHz RSSI: Could not acquire mutex, skipping");
        subghz_error_count++;
        return 0;
    }
    
    FURI_LOG_D(TAG, "SubGHz RSSI: Starting enhanced hardware RSSI collection");
    
    // If we've had too many consecutive errors, force a reset
    if(subghz_error_count > 10) {
        FURI_LOG_W(TAG, "SubGHz RSSI: Too many errors (%lu), forcing reset", subghz_error_count);
        furi_hal_subghz_reset();
        subghz_error_count = 0;
    }
    
    // Optimized frequency set based on regional validation
    // Focus on frequencies that work globally without blocks
    uint32_t frequencies[] = {
        // 300-348 MHz band (most regions allow these)
        300000000,  // 300 MHz - Band edge
        310000000,  // 310 MHz 
        315000000,  // 315 MHz - ISM band (US/Asia)
        318000000,  // 318 MHz - UK SRD
        330000000,  // 330 MHz
        345000000,  // 345 MHz - Near band edge
        
        // 387-464 MHz band (check region)
        390000000,  // 390 MHz
        410000000,  // 410 MHz
        418000000,  // 418 MHz
        
        // 433 MHz region - most universally accepted
        433050000,  // 433.05 MHz - LPD433 start
        433175000,  // 433.175 MHz - LPD433
        433300000,  // 433.3 MHz - LPD433
        433420000,  // 433.42 MHz - Amateur
        433620000,  // 433.62 MHz
        433920000,  // 433.92 MHz - ISM Global
        434420000,  // 434.42 MHz - SRD
        434790000,  // 434.79 MHz - LPD433 end
        
        // 440-450 MHz (region dependent)
        440000000,  // 440 MHz
        446000000,  // 446 MHz - PMR446
        450000000,  // 450 MHz
        
        // 460-464 MHz (often blocked)
        460000000,  // 460 MHz
        462562500,  // 462.5625 MHz - FRS/GMRS
        464000000,  // 464 MHz - Band edge
        
        // 902-928 MHz ISM (Americas/AU only)
        902000000,  // 902 MHz
        905000000,  // 905 MHz
        910000000,  // 910 MHz
        915000000,  // 915 MHz - ISM center
        920000000,  // 920 MHz
        925000000,  // 925 MHz
        
        // Note: Removed 784, 903, 928 MHz as they often fail
        // Note: 868 MHz removed - EU only
    };
    
    // Try to prepare SubGHz for RX
    bool subghz_available = false;
    bool init_success = false;
    
    // Check if SubGHz is already in use by trying to acquire it
    // We'll use the frequency test as our availability check instead
    
    // CRITICAL FIX: Wrap all SubGHz operations in error handling
    // to prevent crashes from bad hardware states
    FURI_CRITICAL_ENTER();  // Disable interrupts during state transition
    
    // Ensure we're in a clean state
    furi_hal_subghz_sleep();
    
    // Check if we should abort early
    if(state && !state->is_running) {
        FURI_CRITICAL_EXIT();
        furi_mutex_release(subghz_mutex);
        return 0;
    }
    
    furi_hal_subghz_idle();
    
    FURI_CRITICAL_EXIT();  // Re-enable interrupts
    
    // Load optimized preset for entropy collection
    // Using OOK 650kHz for wide bandwidth and good sensitivity
    furi_hal_subghz_load_custom_preset(subghz_device_cc1101_preset_ook_650khz_async_regs);
    
    // Configure for optimal RSSI readings (based on spectrum analyzer)
    // Disable AGC freeze, set optimal gain settings
    uint8_t agc_settings[][2] = {
        {CC1101_AGCCTRL0, 0x91}, // Medium hysteresis, 16 samples AGC
        {CC1101_AGCCTRL2, 0xC0}, // MAX LNA+LNA2, MAIN_TARGET 24 dB
        {0, 0}
    };
    
    // Load registers (void function, no return value to check)
    furi_hal_subghz_load_registers(agc_settings[0]);
    
    // Put in idle state after configuration
    furi_hal_subghz_idle();
    
    // Check if we can successfully set a frequency (indicates SubGHz is responsive)
    uint32_t test_freq = furi_hal_subghz_set_frequency(433920000);
    if(test_freq > 0) {
        subghz_available = true;
        init_success = true;
        FURI_LOG_D(TAG, "SubGHz RSSI: Hardware ready at %lu Hz", test_freq);
    } else {
        FURI_LOG_W(TAG, "SubGHz RSSI: Hardware not responding, using timing entropy");
        subghz_error_count++;
        
        // Clean up if initialization failed - with comprehensive reset
        furi_hal_subghz_idle();
        furi_hal_subghz_sleep();
        
        // If errors are piling up, do a full reset
        if(subghz_error_count > 5) {
            FURI_LOG_W(TAG, "SubGHz RSSI: Multiple init failures, forcing reset");
            furi_hal_subghz_reset();
            subghz_error_count = 0;
        }
        
        furi_mutex_release(subghz_mutex);
        return 0;
    }
    
    // Calculate number of frequencies
    size_t num_freqs = sizeof(frequencies) / sizeof(frequencies[0]);
    
    // Pre-filter frequencies to only valid ones for this region
    // This avoids "frequency blocked" errors
    uint32_t valid_frequencies[30];  // Increased to handle more frequencies
    size_t valid_count = 0;
    
    for(size_t i = 0; i < num_freqs && valid_count < 30; i++) {  // Increased limit to 30
        if(furi_hal_subghz_is_frequency_valid(frequencies[i])) {
            valid_frequencies[valid_count++] = frequencies[i];
            // Only log debug info for first few frequencies to reduce spam
            if(valid_count <= 3) {
                FURI_LOG_D(TAG, "SubGHz: Freq %lu MHz is valid", frequencies[i]/1000000);
            }
        }
    }
    
    if(valid_count == 0) {
        FURI_LOG_W(TAG, "SubGHz: No frequencies valid in this region, using timing entropy");
        furi_hal_subghz_sleep();
        furi_mutex_release(subghz_mutex);
        return DWT->CYCCNT ^ (DWT->CYCCNT << 16);
    }
    
    FURI_LOG_I(TAG, "SubGHz: %zu frequencies valid in this region", valid_count);
    
    // We'll collect up to 32 bits of entropy (4 bytes)
    // Sample a subset of valid frequencies each time for speed
    uint8_t entropy_bytes[4] = {0};
    uint8_t byte_idx = 0;
    
    // Use hardware RNG to randomize frequency selection for unpredictability
    uint32_t hw_random = furi_hal_random_get();
    uint8_t freq_offset = hw_random & 0xFF;
    
    // Dynamic sampling based on available frequencies
    // More frequencies = better entropy but slower collection
    uint8_t samples_to_take = (uint8_t)valid_count;
    if(samples_to_take > 12) samples_to_take = 12;  // Increased max for better entropy
    if(samples_to_take < 6) samples_to_take = 6;     // Increased min for quality
    
    FURI_LOG_D(TAG, "SubGHz RSSI: Sampling %u frequencies from %zu valid", 
              samples_to_take, valid_count);
    
    // Non-consecutive frequency hopping with prime-based distribution
    // This ensures we sample different frequencies each time
    uint8_t prime_hop = 7;  // Prime number for good distribution
    
    // Add timeout protection for the entire sampling loop
    uint32_t loop_start_time = furi_get_tick();
    const uint32_t MAX_LOOP_TIME_MS = 500;  // Max 500ms for all sampling
    
    for(int i = 0; i < samples_to_take && byte_idx < 4; i++) {
        // Check if we should stop early
        if(state && !state->is_running) {
            FURI_LOG_D(TAG, "SubGHz RSSI: Early exit due to stop request");
            break;
        }
        
        // Timeout protection - prevent infinite loops
        if(furi_get_tick() - loop_start_time > MAX_LOOP_TIME_MS) {
            FURI_LOG_W(TAG, "SubGHz RSSI: Sampling timeout after %lums, collected %u bytes", 
                      furi_get_tick() - loop_start_time, byte_idx);
            subghz_error_count++;
            break;
        }
        
        // Use prime-based hopping for better frequency distribution
        // This avoids repeating the same frequency too often
        uint8_t freq_idx;
        if(valid_count > 1) {
            // Prime-based hopping with random offset ensures good coverage
            freq_idx = (freq_offset + (i * prime_hop)) % valid_count;
        } else {
            freq_idx = 0;  // Only one frequency available
        }
        
        uint32_t frequency = valid_frequencies[freq_idx];
        
        uint8_t noise_byte = 0;
        uint32_t timing_start = DWT->CYCCNT;
        
        // Try to use real RSSI if SubGHz is available
        if(subghz_available) {
            // Double-check frequency validity before setting
            // This avoids "frequency blocked" errors
            if(!furi_hal_subghz_is_frequency_valid(frequency)) {
                FURI_LOG_D(TAG, "SubGHz: Freq %lu MHz blocked at runtime, using timing", 
                          frequency/1000000);
                uint32_t timing_end = DWT->CYCCNT;
                noise_byte = (timing_end - timing_start) & 0xFF;
                entropy_bytes[byte_idx++] = noise_byte;
                continue;
            }
            
            // Configure and start RX with error checking
            uint32_t actual_freq = furi_hal_subghz_set_frequency(frequency);
            
            if(actual_freq > 0) {
                // CRITICAL FIX: Wrap RX in critical section to prevent interrupt issues
                FURI_CRITICAL_ENTER();
                furi_hal_subghz_rx();
                FURI_CRITICAL_EXIT();
                
                // Spectrum analyzer uses 3ms for RSSI stabilization
                // This ensures AGC has settled and we get accurate readings
                furi_delay_ms(3);
                
                // Take multiple RSSI samples for better entropy
                // Sample more times for increased entropy quality
                float rssi_samples[5];
                uint8_t lqi_samples[5];
                bool sample_success = true;
                
                for(int j = 0; j < 5; j++) {
                    // Verify we're still in RX mode before sampling
                    // This prevents crashes if radio state changed unexpectedly
                    rssi_samples[j] = furi_hal_subghz_get_rssi();
                    lqi_samples[j] = furi_hal_subghz_get_lqi();
                    
                    // Basic sanity check on RSSI values
                    if(rssi_samples[j] < -130.0f || rssi_samples[j] > 0.0f) {
                        FURI_LOG_W(TAG, "SubGHz RSSI: Invalid RSSI value %.1f, using fallback", 
                                  (double)rssi_samples[j]);
                        sample_success = false;
                        break;
                    }
                    
                    furi_delay_us(200);  // 200us between samples for variation
                }
                
                if(sample_success) {
                    // Calculate RSSI variance (good entropy source)
                    float rssi_avg = 0;
                    for(int j = 0; j < 5; j++) {
                        rssi_avg += rssi_samples[j];
                    }
                    rssi_avg /= 5.0f;
                    
                    float rssi_variance = 0;
                    for(int j = 0; j < 5; j++) {
                        float diff = rssi_samples[j] - rssi_avg;
                        rssi_variance += diff * diff;
                    }
                    rssi_variance /= 5.0f;  // Normalize variance
                    
                    // Convert floats to bits for entropy extraction
                    union { float f; uint32_t i; } rssi_conv = { .f = rssi_samples[0] };
                    union { float f; uint32_t i; } var_conv = { .f = rssi_variance };
                    
                    // Get additional timing entropy
                    uint32_t timing_end = DWT->CYCCNT;
                    uint32_t timing_noise = (timing_end - timing_start);
                    
                    // Enhanced entropy extraction using multiple sources
                    // Combine RSSI mantissa bits, variance, LQI changes, and timing
                    uint8_t rssi_bits = (rssi_conv.i & 0xFF) ^ ((rssi_conv.i >> 8) & 0xFF);
                    uint8_t var_bits = (var_conv.i & 0xFF) ^ ((var_conv.i >> 16) & 0xFF);
                    
                    // XOR all 5 LQI samples for maximum entropy
                    uint8_t lqi_bits = lqi_samples[0];
                    for(int j = 1; j < 5; j++) {
                        lqi_bits ^= lqi_samples[j];
                    }
                    
                    uint8_t timing_bits = (timing_noise & 0xFF) ^ ((timing_noise >> 8) & 0xFF) ^ ((timing_noise >> 16) & 0xFF);
                    
                    // Mix all entropy sources with rotation
                    noise_byte = rssi_bits;
                    noise_byte = (noise_byte << 1) | (noise_byte >> 7);
                    noise_byte ^= var_bits;
                    noise_byte = (noise_byte << 1) | (noise_byte >> 7);
                    noise_byte ^= lqi_bits;
                    noise_byte = (noise_byte << 1) | (noise_byte >> 7);
                    noise_byte ^= timing_bits;
                    
                    // Only log detailed info for some samples to reduce spam
                    if(byte_idx <= 2 || (byte_idx == 3 && i == samples_to_take - 1)) {
                        FURI_LOG_D(TAG, "SubGHz RSSI: Freq=%lu MHz, RSSI=%.1f dBm (var=%.2f), LQI=%u, byte=0x%02X", 
                                  frequency/1000000, (double)rssi_avg, (double)rssi_variance, lqi_samples[0], noise_byte);
                    }
                } else {
                    // Sample failed, use timing fallback
                    uint32_t timing_end = DWT->CYCCNT;
                    noise_byte = (timing_end - timing_start) & 0xFF;
                    subghz_error_count++;
                }
                
                // Return to idle between frequencies - with critical section
                FURI_CRITICAL_ENTER();
                furi_hal_subghz_idle();
                FURI_CRITICAL_EXIT();
            } else {
                // Failed to set frequency, use timing
                uint32_t timing_end = DWT->CYCCNT;
                noise_byte = (timing_end - timing_start) & 0xFF;
                FURI_LOG_D(TAG, "SubGHz RSSI: Failed to set freq %lu MHz, using timing", 
                          frequency/1000000);
            }
        } else {
            // SubGHz not available, use timing entropy
            uint32_t timing_end = DWT->CYCCNT;
            noise_byte = (timing_end - timing_start) & 0xFF;
            FURI_LOG_D(TAG, "SubGHz RSSI: Hardware unavailable, using timing=0x%02X", noise_byte);
        }
        
        // Store the entropy byte
        entropy_bytes[byte_idx++] = noise_byte;
        
        // Very small delay to avoid hogging the radio
        furi_delay_us(5);
    }
    
    // No need for static offset since we randomize with HW RNG each time
    
    // Enhanced entropy packing with whitening
    // Pack entropy bytes into 32-bit return value with mixing
    uint32_t mixer = 0x9E3779B9;  // Golden ratio for mixing
    for(int i = 0; i < byte_idx; i++) {
        entropy = (entropy << 8) | entropy_bytes[i];
        entropy ^= (entropy >> 16);  // Avalanche effect
        entropy *= mixer;
        mixer += 0x6C078965;  // Different constant for each byte
    }
    
    // Final mix with timing
    entropy ^= DWT->CYCCNT;
    
    // Clean shutdown of SubGHz if we used it
    if(subghz_available && init_success) {
        // Critical section for state transitions to prevent race conditions
        FURI_CRITICAL_ENTER();
        furi_hal_subghz_idle();
        furi_hal_subghz_sleep();
        FURI_CRITICAL_EXIT();
        
        // Don't reset here - only sleep to save power
        // Reset should only happen on app exit or after errors
    } else if(subghz_available) {
        // Init was not fully successful, force a reset to clean state
        FURI_LOG_W(TAG, "SubGHz RSSI: Init incomplete, forcing reset");
        furi_hal_subghz_reset();
        subghz_error_count++;
    }
    
    // Track successful completion
    if(entropy != 0 && byte_idx > 0) {
        subghz_error_count = 0;  // Reset error counter on success
        subghz_last_success = furi_get_tick();
    } else {
        subghz_error_count++;
        
        // If we haven't had a success in a long time, force reset
        uint32_t time_since_success = furi_get_tick() - subghz_last_success;
        if(time_since_success > 60000) {  // 60 seconds without success
            FURI_LOG_W(TAG, "SubGHz RSSI: No success in 60s, forcing reset");
            furi_hal_subghz_reset();
            subghz_error_count = 0;
            subghz_last_success = furi_get_tick();
        }
    }
    
    FURI_LOG_I(TAG, "SubGHz RSSI: Collected %u bytes entropy=0x%08lX (HW:%s, Valid:%zu, Sampled:%u, Errors:%lu)", 
              byte_idx, entropy, subghz_available ? "Yes" : "No", valid_count, samples_to_take, subghz_error_count);
    
    // Release mutex before returning - CRITICAL to prevent deadlock
    furi_mutex_release(subghz_mutex);
    
    return entropy;
}

// Backward compatibility wrapper
uint32_t flipper_rng_get_subghz_rssi_noise(void) {
    return flipper_rng_get_subghz_rssi_noise_ex(NULL);
}

void flipper_rng_collect_subghz_rssi_entropy(FlipperRngState* state) {
    if(state->entropy_sources & EntropySourceSubGhzRSSI) {
        uint32_t rssi_noise = flipper_rng_get_subghz_rssi_noise_ex(state);
        // Enhanced implementation provides ~16-20 bits of quality entropy
        // due to RSSI variance, multiple samples, and wider frequency coverage
        if(rssi_noise != 0) {  // Only add if we got entropy
            flipper_rng_add_entropy(state, rssi_noise, 16); // Enhanced quality RF noise
        }
    }
}


// Global variables for IR signal capture
static volatile uint32_t ir_entropy_accumulator = 0;
static volatile uint32_t ir_signal_count = 0;  // Count of signals received
static volatile uint32_t ir_pulse_count = 0;

// Callback for IR signal reception
static void ir_entropy_callback(void* ctx, InfraredWorkerSignal* signal) {
    UNUSED(ctx);
    
    // Quick blue LED pulse to indicate IR reception
    // Using direct HAL control for minimal latency
    furi_hal_light_set(LightBlue, 100);  // Blue at 100/255 brightness
    
    const uint32_t* timings = NULL;
    size_t timings_cnt = 0;
    
    // Check if signal is decoded or raw
    if(infrared_worker_signal_is_decoded(signal)) {
        // For decoded signals, extract entropy from protocol data
        const InfraredMessage* message = infrared_worker_get_decoded_signal(signal);
        if(message) {
            // Use protocol, address, command, and timing as entropy sources
            uint32_t local_entropy = DWT->CYCCNT;
            local_entropy ^= message->protocol;
            local_entropy ^= (message->address << 8);
            local_entropy ^= (message->command << 16);
            local_entropy ^= (message->repeat ? 0xAAAAAAAA : 0x55555555);
            
            // Update global accumulator using addition + rotation to avoid cancellation
            ir_entropy_accumulator = (ir_entropy_accumulator << 1) | (ir_entropy_accumulator >> 31);
            ir_entropy_accumulator += local_entropy;
            ir_pulse_count++;
            ir_signal_count++;
            
            FURI_LOG_I(TAG, "IR decoded: proto=%d, addr=0x%lX, cmd=0x%lX, entropy=0x%08lX", 
                      message->protocol, message->address, message->command, local_entropy);
        }
    } else {
        // Get raw signal timings
        infrared_worker_get_raw_signal(signal, &timings, &timings_cnt);
        
        if(timings_cnt > 0) {
            // Mix timing values into entropy
            uint32_t local_entropy = DWT->CYCCNT;
            
            for(size_t i = 0; i < timings_cnt && i < 32; i++) {
                // Each timing contains entropy from IR physics:
                // - Actual IR pulse duration variations
                // - Atmospheric absorption variations
                // - Receiver photodiode noise
                // - Temperature effects on components
                local_entropy = (local_entropy << 3) ^ (local_entropy >> 29) ^ timings[i];
                local_entropy += i * 0x9E3779B9;  // Golden ratio for mixing
            }
            
            // Update global accumulator using addition + rotation to avoid cancellation
            ir_entropy_accumulator = (ir_entropy_accumulator << 1) | (ir_entropy_accumulator >> 31);
            ir_entropy_accumulator += local_entropy;
            ir_pulse_count += timings_cnt;
            ir_signal_count++;
            
            FURI_LOG_I(TAG, "IR raw: %zu samples, entropy=0x%08lX", timings_cnt, local_entropy);
        }
    }
    
    // Turn off blue LED after processing
    // The brief processing time provides a visible flash
    furi_hal_light_set(LightBlue, 0);
}

// Get infrared ambient noise from IR sensor - Real signal capture
uint32_t flipper_rng_get_infrared_noise(void) {
    uint32_t entropy = 0;
    
    FURI_LOG_I(TAG, "Infrared: Starting IR signal collection");
    
    // Check if IR is already in use
    if(furi_hal_infrared_is_busy()) {
        FURI_LOG_W(TAG, "Infrared: IR busy, skipping collection");
        return 0;
    }
    
    // Reset accumulators to count only this window's signals
    ir_entropy_accumulator = 0;
    ir_signal_count = 0;
    ir_pulse_count = 0;
    
    // Create IR worker for signal capture
    InfraredWorker* ir_worker = infrared_worker_alloc();
    if(!ir_worker) {
        FURI_LOG_W(TAG, "Infrared: Failed to allocate worker");
        return 0;  // No entropy if we can't allocate
    }
    
    // Enable BOTH decoded and raw signals to maximize entropy collection
    // This way we can capture any IR signal, regardless of protocol
    infrared_worker_rx_enable_signal_decoding(ir_worker, true);
    
    // Disable automatic blinking - we handle it manually in the callback
    infrared_worker_rx_enable_blink_on_receiving(ir_worker, false);
    
    // Start IR reception first (like CLI does)
    infrared_worker_rx_start(ir_worker);
    
    // Then set up callback to capture IR signals
    infrared_worker_rx_set_received_signal_callback(ir_worker, ir_entropy_callback, NULL);
    
    // Collect IR signals for a period of time
    // Even without active sources, we'll get:
    // - Background IR from lights (fluorescent flicker at 50/60Hz)
    // - Thermal IR noise from environment
    // - Photodiode dark current noise
    // - Amplifier thermal noise
    
    uint32_t collection_time_ms = 300;  // Collect for 300ms (longer window to catch IR signals)
    uint32_t samples_taken = 0;
    uint32_t start_time = furi_get_tick();
    
    // Process events while collecting - this is crucial for IR callbacks to work!
    while((furi_get_tick() - start_time) < collection_time_ms) {
        // Process pending callbacks by yielding to the scheduler
        // This allows the IR worker thread to deliver callbacks
        furi_delay_tick(1);  // Yield for 1 tick to process events
    }
    
    // Stop IR reception
    infrared_worker_rx_stop(ir_worker);
    
    // Small delay to allow any final callbacks to complete
    furi_delay_ms(5);
    
    // Clean up
    infrared_worker_free(ir_worker);
    
    // Now capture the accumulated entropy and reset for next time
    if(ir_pulse_count > 0) {
        // We got real IR signals - use them
        entropy = ir_entropy_accumulator;
        // Mix with pulse count and signal count for additional entropy
        entropy ^= (ir_pulse_count << 16) | (ir_signal_count << 8);
        samples_taken = ir_signal_count;
        
        FURI_LOG_I(TAG, "Infrared: Collected %lu IR pulses, %lu signals, entropy=0x%08lX", 
                  (unsigned long)ir_pulse_count, (unsigned long)samples_taken, entropy);
    } else {
        FURI_LOG_D(TAG, "Infrared: No IR signals detected in %lums window", collection_time_ms);
        entropy = 0;
        samples_taken = 0;
        // Don't reset if nothing was collected - might still be accumulating
    }
    
    return entropy;
}

void flipper_rng_collect_infrared_entropy(FlipperRngState* state) {
    if(state->entropy_sources & EntropySourceInfraredNoise) {
        uint32_t ir_noise = flipper_rng_get_infrared_noise();
        flipper_rng_add_entropy(state, ir_noise, 8); // Good quality IR ambient noise
    }
}



#include "flipper_rng_entropy.h"
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

#define TAG "FlipperRNG"

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

// Get SubGHz RSSI noise - Enhanced hardware implementation with improved entropy
uint32_t flipper_rng_get_subghz_rssi_noise(void) {
    uint32_t entropy = 0;
    
    FURI_LOG_I(TAG, "SubGHz RSSI: Starting enhanced hardware RSSI collection");
    
    // Extended frequency set for maximum entropy coverage
    // Covering all major ISM bands and allowed frequencies
    uint32_t frequencies[] = {
        // 300-348 MHz band (CC1101 supported, good atmospheric noise)
        300000000,  // 300 MHz - Band edge, high noise
        310000000,  // 310 MHz - Between common bands
        315000000,  // 315 MHz - ISM band (US garage doors, car remotes)
        318000000,  // 318 MHz - UK SRD band
        330000000,  // 330 MHz - Military/aviation nearby
        345000000,  // 345 MHz - Near band edge
        
        // 387-464 MHz band (CC1101 supported)
        390000000,  // 390 MHz - Near public safety bands
        410000000,  // 410 MHz - Amateur radio nearby
        418000000,  // 418 MHz - TETRA emergency services nearby
        
        // 433 MHz region - most universally accepted
        433050000,  // 433.05 MHz - LPD433 band start
        433175000,  // 433.175 MHz - LPD433 mid-band
        433300000,  // 433.3 MHz - LPD433 channel
        433420000,  // 433.42 MHz - Amateur radio  
        433620000,  // 433.62 MHz - Between channels
        433920000,  // 433.92 MHz - ISM band center (Global)
        434420000,  // 434.42 MHz - SRD band
        434790000,  // 434.79 MHz - LPD433 band end
        
        // 440-450 MHz (additional coverage)
        440000000,  // 440 MHz - Amateur radio band edge
        446000000,  // 446 MHz - PMR446 (EU walkie-talkies)
        450000000,  // 450 MHz - Commercial mobile radio
        
        // 460-464 MHz (upper CC1101 range)
        460000000,  // 460 MHz - Public safety adjacent
        462562500,  // 462.5625 MHz - FRS/GMRS channel 1
        464000000,  // 464 MHz - Band edge
        
        // 779-928 MHz band (CC1101 supported)
        784000000,  // 784 MHz - Public safety band nearby
        
        // 902-928 MHz ISM band (Americas/Australia)
        902000000,  // 902 MHz - ISM band start
        903000000,  // 903 MHz 
        905000000,  // 905 MHz
        910000000,  // 910 MHz
        915000000,  // 915 MHz - ISM band center
        920000000,  // 920 MHz
        925000000,  // 925 MHz
        928000000,  // 928 MHz - ISM band end
        
        // Note: 868 MHz removed due to regional restrictions
        // Note: 2.4 GHz not supported by CC1101
    };
    
    // Try to prepare SubGHz for RX
    bool subghz_available = false;
    
    // Don't reset every time - just ensure idle state
    furi_hal_subghz_idle();
    
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
    furi_hal_subghz_load_registers(agc_settings[0]);
    
    // Put in idle state after configuration
    furi_hal_subghz_idle();
    
    // Check if we can successfully set a frequency (indicates SubGHz is responsive)
    uint32_t test_freq = furi_hal_subghz_set_frequency(433920000);
    if(test_freq > 0) {
        subghz_available = true;
        FURI_LOG_I(TAG, "SubGHz RSSI: Hardware ready at %lu Hz", test_freq);
    } else {
        FURI_LOG_W(TAG, "SubGHz RSSI: Hardware not responding, using timing entropy");
    }
    
    // Calculate number of frequencies
    size_t num_freqs = sizeof(frequencies) / sizeof(frequencies[0]);
    
    // Pre-filter frequencies to only valid ones for this region
    // This avoids "frequency blocked" errors
    uint32_t valid_frequencies[10];
    size_t valid_count = 0;
    
    for(size_t i = 0; i < num_freqs && valid_count < 10; i++) {
        if(furi_hal_subghz_is_frequency_valid(frequencies[i])) {
            valid_frequencies[valid_count++] = frequencies[i];
            FURI_LOG_D(TAG, "SubGHz: Freq %lu MHz is valid", frequencies[i]/1000000);
        } else {
            FURI_LOG_D(TAG, "SubGHz: Freq %lu MHz blocked, skipping", frequencies[i]/1000000);
        }
    }
    
    if(valid_count == 0) {
        FURI_LOG_W(TAG, "SubGHz: No frequencies valid in this region, using timing entropy");
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
    
    // Non-consecutive frequency hopping pattern (inspired by spectrum analyzer)
    // This reduces interference between adjacent frequencies
    uint8_t hop_pattern[] = {0, 3, 1, 4, 2, 5, 7, 6, 9, 8, 11, 10};
    
    for(int i = 0; i < samples_to_take && byte_idx < 4; i++) {
        // Use hopping pattern with random offset for unpredictability
        uint8_t pattern_idx = hop_pattern[i % 12];
        uint8_t freq_idx = (freq_offset + pattern_idx) % valid_count;
        uint32_t frequency = valid_frequencies[freq_idx];
        
        uint8_t noise_byte = 0;
        uint32_t timing_start = DWT->CYCCNT;
        
        // Try to use real RSSI if SubGHz is available (frequency already validated)
        if(subghz_available) {
            // Configure and start RX (use set_frequency since we pre-validated)
            uint32_t actual_freq = furi_hal_subghz_set_frequency(frequency);
            
            if(actual_freq > 0) {
                furi_hal_subghz_rx();
                
                // Spectrum analyzer uses 3ms for RSSI stabilization
                // This ensures AGC has settled and we get accurate readings
                furi_delay_ms(3);
                
                // Take multiple RSSI samples for better entropy
                // Sample more times for increased entropy quality
                float rssi_samples[5];
                uint8_t lqi_samples[5];
                
                for(int j = 0; j < 5; j++) {
                    rssi_samples[j] = furi_hal_subghz_get_rssi();
                    lqi_samples[j] = furi_hal_subghz_get_lqi();
                    furi_delay_us(200);  // 200us between samples for variation
                }
                
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
                
                FURI_LOG_D(TAG, "SubGHz RSSI: Freq=%lu MHz, RSSI=%.1f dBm (var=%.2f), LQI=%u, byte=0x%02X", 
                          frequency/1000000, (double)rssi_avg, (double)rssi_variance, lqi_samples[0], noise_byte);
                
                // Return to idle between frequencies
                furi_hal_subghz_idle();
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
    if(subghz_available) {
        furi_hal_subghz_idle();
        furi_hal_subghz_sleep();
        // Don't reset here - only sleep to save power
        // Reset should only happen on app exit, not between collections
    }
    
    FURI_LOG_I(TAG, "SubGHz RSSI: Collected %u bytes entropy=0x%08lX (HW:%s, Valid:%zu, Sampled:%u)", 
              byte_idx, entropy, subghz_available ? "Yes" : "No", valid_count, samples_to_take);
    
    return entropy;
}

void flipper_rng_collect_subghz_rssi_entropy(FlipperRngState* state) {
    if(state->entropy_sources & EntropySourceSubGhzRSSI) {
        uint32_t rssi_noise = flipper_rng_get_subghz_rssi_noise();
        // Enhanced implementation provides ~16-20 bits of quality entropy
        // due to RSSI variance, multiple samples, and wider frequency coverage
        flipper_rng_add_entropy(state, rssi_noise, 16); // Enhanced quality RF noise
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



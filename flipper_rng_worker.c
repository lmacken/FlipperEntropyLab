#include "flipper_rng.h"
#include "flipper_rng_entropy.h"
#include "flipper_rng_views.h"
#include <furi_hal_random.h>
#include <furi_hal_serial.h>
#include <furi_hal_usb_cdc.h>
#include <storage/storage.h>
#include <toolbox/stream/file_stream.h>
#include <math.h>
#include <string.h>

#define TAG "FlipperRNG"
#define OUTPUT_BUFFER_SIZE 256

// Worker thread - Version 3: Multi-source entropy collection
int32_t flipper_rng_worker_thread(void* context) {
    FlipperRngApp* app = context;
    
    FURI_LOG_I(TAG, "Worker thread started - V3: Multi-source entropy");
    FURI_LOG_I(TAG, "Output mode: %d (0=USB, 1=UART, 2=Viz)", app->state->output_mode);
    FURI_LOG_I(TAG, "Entropy sources: 0x%02lX", app->state->entropy_sources);
    
    // Initialize entropy sources
    flipper_rng_init_entropy_sources(app->state);
    
    // Output buffer
    uint8_t output_buffer[OUTPUT_BUFFER_SIZE];
    size_t buffer_pos = 0;
    
    // Mark as used to avoid warning
    UNUSED(output_buffer);
    
    uint32_t counter = 0;
    uint32_t mix_counter = 0;
    
    FURI_LOG_I(TAG, "Worker entering main loop, is_running=%d", app->state->is_running);
    
    while(app->state->is_running) {
        // Log periodically
        if(counter % 100 == 0) {
            FURI_LOG_I(TAG, "Worker running: cycle=%lu, bytes=%lu, is_running=%d", 
                       counter, app->state->bytes_generated, app->state->is_running);
        }
        
        // Collect entropy from enabled sources
        uint32_t entropy_bits = 0;
        
        // Hardware RNG - highest quality
        if(app->state->entropy_sources & EntropySourceHardwareRNG) {
            uint32_t hw_random = furi_hal_random_get();
            flipper_rng_add_entropy(app->state, hw_random, 32);
            entropy_bits += 32;
        }
        
        // ADC noise - medium quality
        if(app->state->entropy_sources & EntropySourceADC) {
            if(app->state->adc_handle) {
                uint32_t adc_noise = flipper_rng_get_adc_noise(app->state->adc_handle);
                flipper_rng_add_entropy(app->state, adc_noise, 8);
                entropy_bits += 8;
            }
        }
        
        // Timing jitter - low-medium quality
        if(app->state->entropy_sources & EntropySourceTiming) {
            uint32_t timing = flipper_rng_get_timing_jitter();
            flipper_rng_add_entropy(app->state, timing, 4);
            entropy_bits += 4;
        }
        
        // CPU jitter - low quality but unique
        if(app->state->entropy_sources & EntropySourceCPUJitter) {
            uint32_t cpu_jitter = flipper_rng_get_cpu_jitter();
            flipper_rng_add_entropy(app->state, cpu_jitter, 2);
            entropy_bits += 2;
        }
        
        // Battery voltage - very low quality, slow changing
        if((app->state->entropy_sources & EntropySourceBatteryVoltage) && (counter % 100 == 0)) {
            uint32_t battery = flipper_rng_get_battery_noise();
            flipper_rng_add_entropy(app->state, battery, 2);
            entropy_bits += 2;
        }
        
        // Temperature - very low quality, very slow changing
        if((app->state->entropy_sources & EntropySourceTemperature) && (counter % 500 == 0)) {
            uint32_t temp = flipper_rng_get_temperature_noise();
            flipper_rng_add_entropy(app->state, temp, 1);
            entropy_bits += 1;
        }
        
        // Mix the entropy pool periodically
        mix_counter++;
        if(mix_counter >= 32) {
            flipper_rng_mix_entropy_pool(app->state);
            mix_counter = 0;
        }
        
        // Extract mixed bytes from the pool
        for(int i = 0; i < 4 && buffer_pos < OUTPUT_BUFFER_SIZE; i++) {
            output_buffer[buffer_pos++] = flipper_rng_extract_random_byte(app->state);
            app->state->bytes_generated++;
        }
        
        // Reset buffer when full or output data
        if(buffer_pos >= OUTPUT_BUFFER_SIZE) {
            // Output data based on selected mode
            if(app->state->output_mode == OutputModeUSB) {
                // Send to USB CDC interface 1 (should work now in dual mode)
                FURI_LOG_I(TAG, "Attempting to send %zu bytes to USB CDC interface 1", buffer_pos);
                furi_hal_cdc_send(1, (uint8_t*)output_buffer, buffer_pos);
                FURI_LOG_I(TAG, "USB send call completed");
            } else if(app->state->output_mode == OutputModeUART) {
                // Send to GPIO UART (pins 13/14)
                if(app->state->serial_handle) {
                    furi_hal_serial_tx(app->state->serial_handle, (uint8_t*)output_buffer, buffer_pos);
                    FURI_LOG_D(TAG, "Sent %zu bytes to UART", buffer_pos);
                } else {
                    FURI_LOG_W(TAG, "UART not initialized");
                }
            } else if(app->state->output_mode == OutputModeFile) {
                // Save to SD card file
                Storage* storage = furi_record_open(RECORD_STORAGE);
                File* file = storage_file_alloc(storage);
                
                // Append to file (create if doesn't exist)
                if(storage_file_open(file, "/ext/flipper_rng.bin", 
                                    FSAM_WRITE, FSOM_OPEN_APPEND)) {
                    size_t written = storage_file_write(file, (const void*)output_buffer, buffer_pos);
                    storage_file_close(file);
                    FURI_LOG_I(TAG, "Wrote %zu bytes to /ext/flipper_rng.bin", written);
                } else {
                    FURI_LOG_W(TAG, "Failed to open file for writing");
                }
                
                storage_file_free(file);
                furi_record_close(RECORD_STORAGE);
            }
            
            buffer_pos = 0;
            FURI_LOG_I(TAG, "Buffer output, %lu total bytes generated", 
                       app->state->bytes_generated);
        }
        
        // Update stats to reflect what we're actually doing
        app->state->samples_collected = counter;
        
        // Update quality metric based on actual entropy pool
        if(counter % 100 == 0) {
            flipper_rng_update_quality_metric(app->state);
        }
        
        // Calculate active source count
        uint8_t active_sources = 0;
        uint32_t sources = app->state->entropy_sources;
        while(sources) {
            active_sources += sources & 1;
            sources >>= 1;
        }
        
        // Update visualization more frequently for smooth animation
        // Update every 10 iterations or when we have enough new data
        if(app->state->output_mode == OutputModeVisualization) {
            static uint32_t vis_counter = 0;
            vis_counter++;
            
            // Update based on poll rate:
            // Fast polls (1-10ms): update every 10 iterations
            // Medium polls (50ms): update every 2 iterations  
            // Slow polls (100ms+): update every iteration
            bool should_update = false;
            if(app->state->poll_interval_ms <= 10) {
                should_update = (vis_counter % 10 == 0);
            } else if(app->state->poll_interval_ms <= 50) {
                should_update = (vis_counter % 2 == 0);
            } else {
                should_update = true;  // Update every iteration for slow polls
            }
            
            if(should_update) {
                // Generate fresh random data for visualization
                uint8_t vis_buffer[128];
                for(int i = 0; i < 128; i++) {
                    // Use fresh random bytes for better visualization
                    vis_buffer[i] = flipper_rng_extract_random_byte(app->state);
                }
                flipper_rng_visualization_update(app, vis_buffer, 128);
            }
        }
        
        // Update test if test is running  
        if(app->state->test_running && buffer_pos > 0) {
            flipper_rng_test_update(app, output_buffer, buffer_pos);
        }
        
        
        counter++;
        
        // Delay with quick exit check, respecting poll interval
        uint32_t delay_ms = app->state->poll_interval_ms;
        // Break delay into small chunks for responsive stopping
        uint32_t chunks = (delay_ms > 10) ? (delay_ms / 10) : 1;
        uint32_t chunk_delay = delay_ms / chunks;
        
        for(uint32_t i = 0; i < chunks && app->state->is_running; i++) {
            furi_delay_ms(chunk_delay);
        }
    }
    
    // Clean up entropy sources before exiting
    flipper_rng_deinit_entropy_sources(app->state);
    
    FURI_LOG_I(TAG, "Worker thread stopped cleanly");
    
    return 0;
}

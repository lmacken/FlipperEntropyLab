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
    
    FURI_LOG_I(TAG, "Worker thread started - V4: Multi-source entropy with always-on visualization");
    FURI_LOG_I(TAG, "Output mode: %d (0=USB, 1=UART, 2=File) - Visualization always available", app->state->output_mode);
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
    uint32_t total_entropy_bits = 0;
    
    // Record start time for rate calculation
    app->state->start_time = furi_get_tick();
    
    FURI_LOG_I(TAG, "Worker entering main loop, is_running=%d", app->state->is_running);
    
    while(app->state->is_running) {
        // Log periodically
        if(counter % 100 == 0) {
            FURI_LOG_I(TAG, "Worker running: cycle=%lu, bytes=%lu, is_running=%d", 
                       counter, app->state->bytes_generated, app->state->is_running);
            
            // Calculate entropy rate
            uint32_t elapsed_ms = furi_get_tick() - app->state->start_time;
            if(elapsed_ms > 0) {
                app->state->entropy_rate = (float)total_entropy_bits * 1000.0f / (float)elapsed_ms;
            }
        }
        
        // Collect entropy from enabled sources
        uint32_t entropy_bits = 0;
        
        // Hardware RNG - highest quality
        if(app->state->entropy_sources & EntropySourceHardwareRNG) {
            uint32_t hw_random = furi_hal_random_get();
            flipper_rng_add_entropy(app->state, hw_random, 32);
            entropy_bits += 32;
            app->state->bits_from_hw_rng += 32;
        }
        
        // ADC noise - enhanced multi-channel differential (medium-high quality)
        if(app->state->entropy_sources & EntropySourceADC) {
            if(app->state->adc_handle) {
                uint32_t adc_noise = flipper_rng_get_adc_noise(app->state->adc_handle);
                flipper_rng_add_entropy(app->state, adc_noise, 12); // Enhanced from 8 to 12 bits
                entropy_bits += 12;
                app->state->bits_from_adc += 12;
            }
        }
        
        
        // Battery voltage - very low quality, slow changing
        if((app->state->entropy_sources & EntropySourceBatteryVoltage) && (counter % 100 == 0)) {
            uint32_t battery = flipper_rng_get_battery_noise();
            flipper_rng_add_entropy(app->state, battery, 2);
            entropy_bits += 2;
            app->state->bits_from_battery += 2;
        }
        
        // Temperature - very low quality, very slow changing
        if((app->state->entropy_sources & EntropySourceTemperature) && (counter % 500 == 0)) {
            uint32_t temp = flipper_rng_get_temperature_noise();
            flipper_rng_add_entropy(app->state, temp, 1);
            entropy_bits += 1;
            app->state->bits_from_temperature += 1;
        }
        
        // SubGHz RSSI - high quality, but slower due to frequency switching
        if((app->state->entropy_sources & EntropySourceSubGhzRSSI) && (counter % 10 == 0)) {
            uint32_t rssi_noise = flipper_rng_get_subghz_rssi_noise();
            flipper_rng_add_entropy(app->state, rssi_noise, 10);
            entropy_bits += 10;
            app->state->bits_from_subghz_rssi += 10;
        }
        
        // Infrared - good quality, ambient IR noise and timing variations
        if((app->state->entropy_sources & EntropySourceInfraredNoise) && (counter % 25 == 0)) {
            uint32_t ir_noise = flipper_rng_get_infrared_noise();
            flipper_rng_add_entropy(app->state, ir_noise, 8);
            entropy_bits += 8;
            app->state->bits_from_infrared += 8;
        }
        
        // Mix the entropy pool periodically
        mix_counter++;
        if(mix_counter >= 32) {
            flipper_rng_mix_entropy_pool(app->state);
            mix_counter = 0;
        }
        
        // Extract mixed bytes from the pool
        // Generate more bytes per iteration for better throughput
        int bytes_to_generate = 16; // Generate 16 bytes per iteration
        for(int i = 0; i < bytes_to_generate && buffer_pos < OUTPUT_BUFFER_SIZE; i++) {
            uint8_t random_byte = flipper_rng_extract_random_byte(app->state);
            output_buffer[buffer_pos++] = random_byte;
            app->state->bytes_generated++;
            
            // Update histogram (16 bins, so divide byte value by 16)
            app->state->byte_histogram[random_byte >> 4]++;
        }
        
        // Output data when we have some (more frequent for UART)
        bool should_output = false;
        if(app->state->output_mode == OutputModeUART) {
            // For UART, send smaller chunks more frequently (every 32 bytes)
            should_output = (buffer_pos >= 32);
        } else {
            // For USB and File modes, wait for full buffer
            should_output = (buffer_pos >= OUTPUT_BUFFER_SIZE);
        }
        
        if(should_output) {
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
        total_entropy_bits += entropy_bits;
        
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
        
        // Update visualization with responsive timing - always available regardless of output mode
        if(true) {  // Always update visualization data for monitoring
            static uint32_t vis_counter = 0;
            static uint32_t last_vis_update = 0;
            vis_counter++;
            
            // Use configurable visualization refresh rate
            uint32_t target_vis_interval_ms = app->state->visual_refresh_ms;
            uint32_t current_time = furi_get_tick();
            
            bool should_update = false;
            
            // Time-based updates using configurable refresh rate only
            if(current_time - last_vis_update >= target_vis_interval_ms) {
                should_update = true;
                last_vis_update = current_time;
            }
            
            // No overrides - respect user's visual refresh rate setting completely
            
            if(should_update) {
                // Generate fresh random data for visualization
                uint8_t vis_buffer[128];
                for(int i = 0; i < 128; i++) {
                    // Use fresh random bytes for better visualization
                    vis_buffer[i] = flipper_rng_extract_random_byte(app->state);
                }
                flipper_rng_visualization_update(app, vis_buffer, 128);
                
                FURI_LOG_I(TAG, "Visualization updated: poll=%lums, visual_rate=%lums, vis_counter=%lu (always-on monitoring)", 
                          app->state->poll_interval_ms, app->state->visual_refresh_ms, vis_counter);
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

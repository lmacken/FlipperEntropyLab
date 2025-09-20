#include "flipper_rng.h"
#include "flipper_rng_entropy.h"
#include "flipper_rng_views.h"
#include <furi_hal_random.h>

#define TAG "FlipperRNG"
#define OUTPUT_BUFFER_SIZE 256

// Worker thread - Version 2: Adding Hardware RNG only
int32_t flipper_rng_worker_thread(void* context) {
    FlipperRngApp* app = context;
    
    FURI_LOG_I(TAG, "Worker thread started - V2: Hardware RNG only");
    
    // Output buffer
    volatile uint8_t output_buffer[OUTPUT_BUFFER_SIZE];
    size_t buffer_pos = 0;
    
    // Mark as used to avoid warning
    UNUSED(output_buffer);
    
    uint32_t counter = 0;
    
    while(app->state->is_running) {
        // Log periodically
        if(counter % 100 == 0) {
            FURI_LOG_I(TAG, "Worker running: cycle=%lu, bytes=%lu", 
                       counter, app->state->bytes_generated);
        }
        
        // Collect from hardware RNG only (safest source)
        if(app->state->entropy_sources & EntropySourceHardwareRNG) {
            uint32_t hw_random = furi_hal_random_get();
            
            // Add to our simplified pool (just use as-is for now)
            output_buffer[buffer_pos++] = (hw_random >> 0) & 0xFF;
            output_buffer[buffer_pos++] = (hw_random >> 8) & 0xFF;
            output_buffer[buffer_pos++] = (hw_random >> 16) & 0xFF;
            output_buffer[buffer_pos++] = (hw_random >> 24) & 0xFF;
            
            app->state->bytes_generated += 4;
            
            // Reset buffer when full
            if(buffer_pos >= OUTPUT_BUFFER_SIZE) {
                buffer_pos = 0;
                FURI_LOG_I(TAG, "Buffer filled, %lu total bytes generated", 
                           app->state->bytes_generated);
            }
        }
        
        // Update stats
        app->state->samples_collected = counter;
        app->state->entropy_quality = 0.95f; // Hardware RNG is high quality
        
        // Update visualization if that's the output mode
        if(app->state->output_mode == OutputModeVisualization && counter % 10 == 0) {
            // Create a buffer of random data for visualization
            uint8_t vis_buffer[128];
            for(int i = 0; i < 128; i += 4) {
                uint32_t rand_val = furi_hal_random_get();
                vis_buffer[i] = (rand_val >> 0) & 0xFF;
                if(i + 1 < 128) vis_buffer[i + 1] = (rand_val >> 8) & 0xFF;
                if(i + 2 < 128) vis_buffer[i + 2] = (rand_val >> 16) & 0xFF;
                if(i + 3 < 128) vis_buffer[i + 3] = (rand_val >> 24) & 0xFF;
            }
            flipper_rng_visualization_update(app, vis_buffer, 128);
        }
        
        
        counter++;
        
        // Delay with quick exit check
        for(int i = 0; i < 10 && app->state->is_running; i++) {
            furi_delay_ms(1);
        }
    }
    
    FURI_LOG_I(TAG, "Worker thread stopped cleanly");
    
    return 0;
}

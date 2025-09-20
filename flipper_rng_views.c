#include "flipper_rng_views.h"
#include "flipper_rng_entropy.h"
#include <gui/elements.h>

#define TAG "FlipperRNG"

// Configuration options
static const char* entropy_source_names[] = {
    "All Sources",
    "HW RNG Only",
    "ADC + HW RNG",
    "Timing Based",
    "Custom Mix",
};

static const char* output_mode_names[] = {
    "USB CDC",
    "UART GPIO",
    "Visualization",
    "File",
};

static const char* poll_interval_names[] = {
    "1 ms",
    "5 ms",
    "10 ms",
    "50 ms",
    "100 ms",
    "500 ms",
};

static const uint32_t poll_interval_values[] = {
    1, 5, 10, 50, 100, 500,
};

void flipper_rng_source_changed(VariableItem* item) {
    FlipperRngApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    
    switch(index) {
    case 0: // All sources
        app->state->entropy_sources = EntropySourceAll;
        break;
    case 1: // HW RNG only
        app->state->entropy_sources = EntropySourceHardwareRNG;
        break;
    case 2: // ADC + HW RNG
        app->state->entropy_sources = EntropySourceHardwareRNG | EntropySourceADC;
        break;
    case 3: // Timing based
        app->state->entropy_sources = EntropySourceTiming | EntropySourceCPUJitter | EntropySourceButtonTiming;
        break;
    case 4: // Custom mix
        app->state->entropy_sources = EntropySourceHardwareRNG | EntropySourceTiming | EntropySourceBatteryVoltage;
        break;
    }
    
    variable_item_set_current_value_text(item, entropy_source_names[index]);
}

void flipper_rng_output_mode_changed(VariableItem* item) {
    FlipperRngApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    
    app->state->output_mode = (OutputMode)index;
    variable_item_set_current_value_text(item, output_mode_names[index]);
}

void flipper_rng_poll_interval_changed(VariableItem* item) {
    FlipperRngApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    
    app->state->poll_interval_ms = poll_interval_values[index];
    variable_item_set_current_value_text(item, poll_interval_names[index]);
}

void flipper_rng_setup_config_view(FlipperRngApp* app) {
    VariableItem* item;
    
    // Entropy sources
    item = variable_item_list_add(
        app->variable_item_list,
        "Entropy Sources",
        COUNT_OF(entropy_source_names),
        flipper_rng_source_changed,
        app
    );
    variable_item_set_current_value_index(item, 0);
    variable_item_set_current_value_text(item, entropy_source_names[0]);
    
    // Output mode
    item = variable_item_list_add(
        app->variable_item_list,
        "Output Mode",
        COUNT_OF(output_mode_names),
        flipper_rng_output_mode_changed,
        app
    );
    variable_item_set_current_value_index(item, 0);
    variable_item_set_current_value_text(item, output_mode_names[0]);
    
    // Poll interval
    item = variable_item_list_add(
        app->variable_item_list,
        "Poll Interval",
        COUNT_OF(poll_interval_names),
        flipper_rng_poll_interval_changed,
        app
    );
    variable_item_set_current_value_index(item, 2); // Default 10ms
    variable_item_set_current_value_text(item, poll_interval_names[2]);
}

// Visualization drawing
void flipper_rng_visualization_draw_callback(Canvas* canvas, void* context) {
    FlipperRngVisualizationModel* model = context;
    
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    
    if(model->viz_mode == 0) {
        // MODE 0: Original visualization with all elements
        // Title with app name and version
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 2, 10, "FlipperRNG v1.0");
        
        // Status
        canvas_set_font(canvas, FontSecondary);
        if(model->is_running) {
            canvas_draw_str(canvas, 2, 20, "Status: Generating");
            
            // Draw entropy quality bar with percentage
            canvas_draw_str(canvas, 2, 30, "Quality:");
            int bar_width = (int)(model->entropy_quality * 55.0f);  // Smaller bar (55 instead of 70)
            canvas_draw_box(canvas, 40, 26, bar_width, 6);          // Moved closer (40 instead of 45)
            canvas_draw_frame(canvas, 40, 26, 55, 6);               // Smaller frame (55 instead of 70)
            // Add percentage after the bar
            char quality_buffer[8];
            snprintf(quality_buffer, sizeof(quality_buffer), "%.0f%%", (double)(model->entropy_quality * 100));
            canvas_draw_str(canvas, 98, 30, quality_buffer);        // Adjusted position for smaller bar
            
            // Bytes generated
            char buffer[32];
            snprintf(buffer, sizeof(buffer), "Bytes: %lu", model->bytes_generated);
            canvas_draw_str(canvas, 2, 40, buffer);
            
            // Random data visualization as pixels
            for(int y = 0; y < 3; y++) {
                for(int x = 0; x < 128; x++) {
                    int data_idx = (y * 128 + x) % 128;
                    uint8_t byte = model->random_data[data_idx];
                    
                    // Draw pixels based on bit patterns
                    for(int bit = 0; bit < 8 && (x + bit) < 128; bit++) {
                        if(byte & (1 << bit)) {
                            canvas_draw_dot(canvas, x + bit, 45 + y * 3);
                        }
                    }
                }
            }
            
            // Draw random walk pattern
            int walk_x = 64;
            int walk_y = 56;
            for(int i = 0; i < 32; i++) {
                uint8_t direction = model->random_data[i] & 0x03;
                switch(direction) {
                case 0: walk_x = MIN(walk_x + 1, 127); break;
                case 1: walk_x = MAX(walk_x - 1, 0); break;
                case 2: walk_y = MIN(walk_y + 1, 63); break;
                case 3: walk_y = MAX(walk_y - 1, 50); break;
                }
                canvas_draw_dot(canvas, walk_x, walk_y);
            }
            
            // Minimal mode indicator (just a dot)
            canvas_draw_dot(canvas, 125, 60);
        } else {
            canvas_draw_str(canvas, 2, 20, "Status: Stopped");
            canvas_draw_str(canvas, 2, 30, "Press Back to return");
        }
    } else {
        // MODE 1: Full screen random walk - CLEAN VERSION
        if(model->is_running) {
            // Pure visualization - no text overlays
            
            // Draw the walk path with connected lines
            int walk_x = model->walk_x;
            int walk_y = model->walk_y;
            
            // Process all random bytes to create a longer walk
            for(int i = 0; i < 128; i++) {
                uint8_t byte = model->random_data[i];
                
                // Use 2 bits at a time for direction
                for(int j = 0; j < 4; j++) {
                    uint8_t direction = (byte >> (j * 2)) & 0x03;
                    int prev_x = walk_x;
                    int prev_y = walk_y;
                    
                    switch(direction) {
                    case 0: walk_x = MIN(walk_x + 2, 127); break;
                    case 1: walk_x = MAX(walk_x - 2, 0); break;
                    case 2: walk_y = MIN(walk_y + 2, 63); break;
                    case 3: walk_y = MAX(walk_y - 2, 0); break;  // Use full screen height
                    }
                    
                    // Draw line from previous to new position
                    canvas_draw_line(canvas, prev_x, prev_y, walk_x, walk_y);
                }
            }
            
            // Draw current position as a small dot (minimal)
            canvas_draw_dot(canvas, walk_x, walk_y);
            
        } else {
            // Minimal stopped message
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str(canvas, 30, 30, "Start generator");
            canvas_draw_str(canvas, 40, 40, "for visualization");
        }
    }
}

bool flipper_rng_visualization_input_callback(InputEvent* event, void* context) {
    FlipperRngApp* app = context;
    bool consumed = false;
    
    // Only respond to InputTypeShort to avoid double-triggering
    if(event->type == InputTypeShort) {
        if(event->key == InputKeyBack) {
            FURI_LOG_I(TAG, "Visualization: Back button pressed");
            return false; // Let view dispatcher handle back
        }
        
        if(event->key == InputKeyOk) {
            if(app->state->is_running) {
                // Toggle visualization mode
                FURI_LOG_I(TAG, "OK button pressed - toggling visualization mode");
                with_view_model(
                    app->visualization_view,
                    FlipperRngVisualizationModel* model,
                    {
                        uint8_t old_mode = model->viz_mode;
                        model->viz_mode = (model->viz_mode + 1) % 2;
                        // Reset walk position when switching to mode 1
                        if(model->viz_mode == 1) {
                            model->walk_x = 64;
                            model->walk_y = 32;
                        }
                        FURI_LOG_I(TAG, "Mode changed: %d -> %d", old_mode, model->viz_mode);
                    },
                    true
                );
                consumed = true;
            } else {
                FURI_LOG_I(TAG, "Visualization: OK pressed while stopped, going back");
                return false; // Return to menu when OK is pressed while stopped
            }
        }
    }
    
    return consumed || true;
}

void flipper_rng_visualization_update(FlipperRngApp* app, uint8_t* data, size_t length) {
    if(!app || !app->visualization_view) {
        return;
    }
    
    with_view_model(
        app->visualization_view,
        FlipperRngVisualizationModel* model,
        {
            // Copy new random data
            size_t copy_len = MIN(length, (size_t)128);
            if(data && copy_len > 0) {
                memcpy(model->random_data, data, copy_len);
            }
            
            // Update statistics
            model->is_running = app->state->is_running;
            model->entropy_quality = app->state->entropy_quality;
            model->bytes_generated = app->state->bytes_generated;
            model->data_pos = (model->data_pos + 1) % 128;
            
            // Update walk position for mode 1 (persistent walk)
            if(model->viz_mode == 1 && data && length > 0) {
                // Move walk based on first byte of new data
                uint8_t direction = data[0] & 0x03;
                switch(direction) {
                case 0: model->walk_x = MIN(model->walk_x + 1, 127); break;
                case 1: model->walk_x = MAX(model->walk_x - 1, 0); break;
                case 2: model->walk_y = MIN(model->walk_y + 1, 63); break;
                case 3: model->walk_y = MAX(model->walk_y - 1, 10); break;
                }
            }
        },
        true
    );
}

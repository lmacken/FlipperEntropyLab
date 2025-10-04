#include "entropylab_views.h"
#include "entropylab_entropy.h"
#include <gui/elements.h>
#include <math.h>


#define TAG "FlipperRNG"

// Helper function to format byte counts in human-readable format
static void format_bytes(char* buffer, size_t buffer_size, uint32_t bytes) {
    if(bytes < 1024) {
        // Less than 1 KB - show raw bytes
        snprintf(buffer, buffer_size, "%lu B", bytes);
    } else if(bytes < 1024 * 1024) {
        // 1 KB to 1 MB - show KB with 1 decimal
        float kb = (float)bytes / 1024.0f;
        snprintf(buffer, buffer_size, "%.1f KB", (double)kb);
    } else if(bytes < 1024 * 1024 * 100) {
        // 1 MB to 100 MB - show MB with 2 decimals
        float mb = (float)bytes / (1024.0f * 1024.0f);
        snprintf(buffer, buffer_size, "%.2f MB", (double)mb);
    } else {
        // 100+ MB - show MB with 1 decimal
        float mb = (float)bytes / (1024.0f * 1024.0f);
        snprintf(buffer, buffer_size, "%.1f MB", (double)mb);
    }
}

// Configuration options
static const char* entropy_source_names[] = {
    "All",
    "HW Only",
    "HW+RF", 
    "HW+IR",
    "RF+IR",
};

static const char* output_mode_names[] = {
    "UART",
    "File",
};

static const char* poll_interval_names[] = {
    "1ms",
    "5ms", 
    "10ms",
    "50ms",
    "100ms",
    "500ms",
};

static const char* visual_refresh_names[] = {
    "100ms",
    "200ms",
    "500ms", 
    "1s",
};

static const char* mixing_mode_names[] = {
    "HW AES",
    "SW XOR",
};

static const char* wordlist_names[] = {
    "EFF",
    "BIP-39", 
    "SLIP-39"
};

static const uint32_t visual_refresh_values[] = {
    100, 200, 500, 1000,
};

static const uint32_t poll_interval_values[] = {
    1, 5, 10, 50, 100, 500,
};

static const uint32_t entropy_source_values[] = {
    EntropySourceAll,                                                              // All high-quality
    EntropySourceHardwareRNG,                                                      // HW RNG only
    EntropySourceHardwareRNG | EntropySourceSubGhzRSSI,                          // HW + RF
    EntropySourceHardwareRNG | EntropySourceInfraredNoise,                       // HW + IR
    EntropySourceSubGhzRSSI | EntropySourceInfraredNoise,                        // RF + IR only
};

static const PassphraseListType wordlist_values[] = {
    PassphraseListEFFLong,
    PassphraseListBIP39,
    PassphraseListSLIP39,
};

void flipper_rng_source_changed(VariableItem* item) {
    FlipperRngApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    
    switch(index) {
    case 0: // All high-quality sources
        app->state->entropy_sources = EntropySourceAll;
        break;
    case 1: // HW RNG only (fastest)
        app->state->entropy_sources = EntropySourceHardwareRNG;
        break;
    case 2: // HW + RF (hardware + atmospheric)
        app->state->entropy_sources = EntropySourceHardwareRNG | EntropySourceSubGhzRSSI;
        break;
    case 3: // HW + IR (hardware + ambient)
        app->state->entropy_sources = EntropySourceHardwareRNG | EntropySourceInfraredNoise;
        break;
    case 4: // RF + IR only (pure environmental)
        app->state->entropy_sources = EntropySourceSubGhzRSSI | EntropySourceInfraredNoise;
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

void flipper_rng_visual_refresh_changed(VariableItem* item) {
    FlipperRngApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    
    app->state->visual_refresh_ms = visual_refresh_values[index];
    variable_item_set_current_value_text(item, visual_refresh_names[index]);
}

void flipper_rng_mixing_mode_changed(VariableItem* item) {
    FlipperRngApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    
    app->state->mixing_mode = (MixingMode)index;
    variable_item_set_current_value_text(item, mixing_mode_names[index]);
    
    FURI_LOG_I(TAG, "Mixing mode changed to: %s", mixing_mode_names[index]);
}

void flipper_rng_wordlist_changed(VariableItem* item) {
    FlipperRngApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    
    app->state->wordlist_type = wordlist_values[index];
    variable_item_set_current_value_text(item, wordlist_names[index]);
    
    FURI_LOG_I(TAG, "Wordlist changed to: %s", wordlist_names[index]);
}


void flipper_rng_setup_config_view(FlipperRngApp* app) {
    VariableItem* item;
    
    // Entropy sources
    item = variable_item_list_add(
        app->variable_item_list,
        "Entropy Source",
        COUNT_OF(entropy_source_names),
        flipper_rng_source_changed,
        app
    );
    // Find the index for the current entropy source setting
    uint32_t entropy_index = 0;
    for(uint32_t i = 0; i < COUNT_OF(entropy_source_values); i++) {
        if(entropy_source_values[i] == app->state->entropy_sources) {
            entropy_index = i;
            break;
        }
    }
    variable_item_set_current_value_index(item, entropy_index);
    variable_item_set_current_value_text(item, entropy_source_names[entropy_index]);
    
    // Pool Mixing (moved up to be right after Entropy Source)
    item = variable_item_list_add(
        app->variable_item_list,
        "Pool Mixing",
        COUNT_OF(mixing_mode_names),
        flipper_rng_mixing_mode_changed,
        app
    );
    variable_item_set_current_value_index(item, app->state->mixing_mode);
    variable_item_set_current_value_text(item, mixing_mode_names[app->state->mixing_mode]);
    
    // Output mode
    item = variable_item_list_add(
        app->variable_item_list,
        "Output Mode",
        COUNT_OF(output_mode_names),
        flipper_rng_output_mode_changed,
        app
    );
    variable_item_set_current_value_index(item, app->state->output_mode);
    variable_item_set_current_value_text(item, output_mode_names[app->state->output_mode]);
    
    // Wordlist Selection (moved below Output Mode)
    item = variable_item_list_add(
        app->variable_item_list,
        "Wordlist",
        COUNT_OF(wordlist_names),
        flipper_rng_wordlist_changed,
        app
    );
    // Find the index for the current wordlist setting
    uint32_t wordlist_index = 0;
    for(uint32_t i = 0; i < COUNT_OF(wordlist_values); i++) {
        if(wordlist_values[i] == app->state->wordlist_type) {
            wordlist_index = i;
            break;
        }
    }
    variable_item_set_current_value_index(item, wordlist_index);
    variable_item_set_current_value_text(item, wordlist_names[wordlist_index]);
    
    // Poll interval
    item = variable_item_list_add(
        app->variable_item_list,
        "Poll Rate",
        COUNT_OF(poll_interval_names),
        flipper_rng_poll_interval_changed,
        app
    );
    // Find the index for the current poll interval
    uint32_t poll_index = 0;
    for(uint32_t i = 0; i < COUNT_OF(poll_interval_values); i++) {
        if(poll_interval_values[i] == app->state->poll_interval_ms) {
            poll_index = i;
            break;
        }
    }
    variable_item_set_current_value_index(item, poll_index);
    variable_item_set_current_value_text(item, poll_interval_names[poll_index]);
    
    // Visual refresh rate
    item = variable_item_list_add(
        app->variable_item_list,
        "Visual Rate",
        COUNT_OF(visual_refresh_names),
        flipper_rng_visual_refresh_changed,
        app
    );
    // Find the index for the current visual refresh rate
    uint32_t visual_index = 0;
    for(uint32_t i = 0; i < COUNT_OF(visual_refresh_values); i++) {
        if(visual_refresh_values[i] == app->state->visual_refresh_ms) {
            visual_index = i;
            break;
        }
    }
    variable_item_set_current_value_index(item, visual_index);
    variable_item_set_current_value_text(item, visual_refresh_names[visual_index]);
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
        canvas_draw_str(canvas, 2, 10, "Entropy Lab v1.0");
        
        // Status
        canvas_set_font(canvas, FontSecondary);
        if(model->is_running) {
            canvas_draw_str(canvas, 2, 20, "Status: Generating");
            
            
            // Bytes generated with human-readable format
            char buffer[32];
            char bytes_str[24];
            format_bytes(bytes_str, sizeof(bytes_str), model->bytes_generated);
            snprintf(buffer, sizeof(buffer), "Bytes: %s", bytes_str);
            canvas_draw_str(canvas, 2, 30, buffer);
            
            // Random data visualization as pixels
            for(int y = 0; y < 3; y++) {
                for(int x = 0; x < 128; x++) {
                    int data_idx = (y * 128 + x) % 128;
                    uint8_t byte = model->random_data[data_idx];
                    
                    // Draw pixels based on bit patterns
                    for(int bit = 0; bit < 8 && (x + bit) < 128; bit++) {
                        if(byte & (1 << bit)) {
                            canvas_draw_dot(canvas, x + bit, 38 + y * 3);
                        }
                    }
                }
            }
            
            // Draw random walk pattern
            int walk_x = 64;
            int walk_y = 50;
            for(int i = 0; i < 32; i++) {
                uint8_t direction = model->random_data[i] & 0x03;
                switch(direction) {
                case 0: walk_x = MIN(walk_x + 1, 127); break;
                case 1: walk_x = MAX(walk_x - 1, 0); break;
                case 2: walk_y = MIN(walk_y + 1, 63); break;
                case 3: walk_y = MAX(walk_y - 1, 48); break;
                }
                canvas_draw_dot(canvas, walk_x, walk_y);
            }
            
            // Minimal mode indicator (just a dot)
            canvas_draw_dot(canvas, 125, 60);
        } else {
            canvas_draw_str(canvas, 2, 20, "Status: Stopped");
            canvas_draw_str(canvas, 2, 30, "Press Back to return");
        }
    } else if(model->viz_mode == 1) {
        // MODE 1: Full screen random walk - CLEAN VERSION
        if(model->is_running) {
            // Pure visualization - no text overlays
            
            // Always start from center for consistent visualization
            int walk_x = 64;
            int walk_y = 32;
            
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
    } else if(model->viz_mode == 2) {
        // MODE 2: Bit Rain (Matrix style)
        if(model->is_running) {
            // Draw falling "rain" of bits
            for(int col = 0; col < 16; col++) {
                int x = col * 8;
                // Use 8 bytes for each column
                for(int row = 0; row < 8; row++) {
                    uint8_t byte = model->random_data[col * 8 + row];
                    int y = row * 8;
                    
                    // Draw bits as dots with varying intensity (simulate falling)
                    for(int bit = 0; bit < 8; bit++) {
                        if(byte & (1 << bit)) {
                            int dot_y = y + bit;
                            if(dot_y < 64) {
                                canvas_draw_dot(canvas, x + (col % 8), dot_y);
                                // Add trail effect
                                if(dot_y > 0 && (row % 2 == 0)) {
                                    canvas_draw_dot(canvas, x + (col % 8), dot_y - 1);
                                }
                            }
                        }
                    }
                }
            }
            
            // Clean - no title overlay
        } else {
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str(canvas, 30, 30, "Start generator");
        }
    } else if(model->viz_mode == 3) {
        // MODE 3: Spiral Galaxy
        if(model->is_running) {
            int center_x = 64;
            int center_y = 32;
            
            // Draw entropy as a spiral pattern
            for(int i = 0; i < 128; i++) {
                uint8_t byte = model->random_data[i];
                float angle = (float)i * 0.3f + (byte & 0x0F) * 0.1f;
                float radius = (float)i * 0.25f;
                
                int x = center_x + (int)(cosf(angle) * radius);
                int y = center_y + (int)(sinf(angle) * radius * 0.6f);  // Compress Y
                
                if(x >= 0 && x < 128 && y >= 0 && y < 64) {
                    // Draw based on byte value intensity
                    if(byte & 0x80) canvas_draw_dot(canvas, x, y);
                    if(byte & 0x40 && i > 0) {
                        // Connect to previous point
                        float prev_angle = (float)(i-1) * 0.3f;
                        float prev_radius = (float)(i-1) * 0.25f;
                        int prev_x = center_x + (int)(cosf(prev_angle) * prev_radius);
                        int prev_y = center_y + (int)(sinf(prev_angle) * prev_radius * 0.6f);
                        canvas_draw_line(canvas, prev_x, prev_y, x, y);
                    }
                }
            }
            
            // Clean - no title overlay
        } else {
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str(canvas, 30, 30, "Start generator");
        }
    } else if(model->viz_mode == 4) {
        // MODE 4: Waveform (Oscilloscope style)
        if(model->is_running) {
            // Draw entropy as audio-style waveform
            int baseline_y = 32;
            
            for(int x = 0; x < 127; x++) {
                uint8_t byte1 = model->random_data[x];
                uint8_t byte2 = model->random_data[x + 1];
                
                // Map byte values to Y position
                int y1 = baseline_y + ((int8_t)byte1 / 4);  // -32 to +32
                int y2 = baseline_y + ((int8_t)byte2 / 4);
                
                // Clamp to screen
                y1 = MAX(0, MIN(63, y1));
                y2 = MAX(0, MIN(63, y2));
                
                // Draw line segment
                canvas_draw_line(canvas, x, y1, x + 1, y2);
            }
            
            // Draw baseline
            for(int x = 0; x < 128; x += 4) {
                canvas_draw_dot(canvas, x, baseline_y);
            }
            
            // Clean - no title overlay
        } else {
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str(canvas, 30, 30, "Start generator");
        }
    } else if(model->viz_mode == 5) {
        // MODE 5: Particle Field
        if(model->is_running) {
            // Draw entropy as particles moving in field
            for(int i = 0; i < 64; i++) {
                uint8_t byte = model->random_data[i * 2];
                uint8_t byte2 = model->random_data[i * 2 + 1];
                
                // Use bytes for X,Y coordinates
                int x = byte % 128;
                int y = byte2 % 64;
                
                // Draw particle
                canvas_draw_dot(canvas, x, y);
                
                // Add velocity lines based on next bytes
                if(i < 63) {
                    uint8_t dx = model->random_data[(i * 2 + 2) % 128];
                    uint8_t dy = model->random_data[(i * 2 + 3) % 128];
                    
                    int x2 = x + ((int8_t)(dx % 16) - 8) / 2;
                    int y2 = y + ((int8_t)(dy % 16) - 8) / 2;
                    
                    x2 = MAX(0, MIN(127, x2));
                    y2 = MAX(0, MIN(63, y2));
                    
                    // Draw motion trail
                    if((byte & 0x03) == 0) {
                        canvas_draw_line(canvas, x, y, x2, y2);
                    }
                }
            }
            
            // Clean - no title overlay
        } else {
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str(canvas, 30, 30, "Start generator");
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
        
        if(event->key == InputKeyOk || event->key == InputKeyRight) {
            if(app->state->is_running) {
                // Cycle forward through visualization modes
                FURI_LOG_I(TAG, "Next visualization mode");
                with_view_model(
                    app->visualization_view,
                    FlipperRngVisualizationModel* model,
                    {
                        uint8_t old_mode = model->viz_mode;
                        model->viz_mode = (model->viz_mode + 1) % 6;  // 6 visualization modes
                        // Reset walk position when switching modes
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
                FURI_LOG_I(TAG, "Visualization: Button pressed while stopped, going back");
                return false; // Return to menu when pressed while stopped
            }
        } else if(event->key == InputKeyLeft) {
            if(app->state->is_running) {
                // Cycle backward through visualization modes
                FURI_LOG_I(TAG, "Previous visualization mode");
                with_view_model(
                    app->visualization_view,
                    FlipperRngVisualizationModel* model,
                    {
                        uint8_t old_mode = model->viz_mode;
                        // Go backward (with wraparound)
                        if(model->viz_mode == 0) {
                            model->viz_mode = 5;  // Wrap to last mode
                        } else {
                            model->viz_mode = model->viz_mode - 1;
                        }
                        // Reset walk position when switching modes
                        if(model->viz_mode == 1) {
                            model->walk_x = 64;
                            model->walk_y = 32;
                        }
                        FURI_LOG_I(TAG, "Mode changed: %d -> %d", old_mode, model->viz_mode);
                    },
                    true
                );
                consumed = true;
            }
        }
    }
    
    return consumed;
}

void flipper_rng_visualization_update(FlipperRngApp* app, uint8_t* data, size_t length) {
    if(!app) {
        return;
    }
    
    // Update visualization view if it exists
    if(app->visualization_view) {
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
                model->bytes_generated = app->state->bytes_generated;
                model->data_pos = (model->data_pos + 1) % 128;
                
                // Don't update walk position here - let the draw callback handle it
                // This prevents the walk from drifting as data updates
            },
            true
        );
    }
    
    // Also update byte distribution view if it exists
    if(app->byte_distribution_view) {
        with_view_model(
            app->byte_distribution_view,
            FlipperRngVisualizationModel* model,
            {
                model->is_running = app->state->is_running;
                model->bytes_generated = app->state->bytes_generated;
                // Copy histogram data
                for(int i = 0; i < 16; i++) {
                    model->histogram[i] = app->state->byte_histogram[i];
                }
            },
            true
        );
    }
    
    // Also update source stats view if it exists
    if(app->source_stats_view) {
        with_view_model(
            app->source_stats_view,
            FlipperRngVisualizationModel* model,
            {
                model->is_running = app->state->is_running;
                model->bytes_generated = app->state->bytes_generated;
                model->bits_from_hw_rng = app->state->bits_from_hw_rng;
                model->bits_from_subghz_rssi = app->state->bits_from_subghz_rssi;
                model->bits_from_infrared = app->state->bits_from_infrared;
                
                // Set start time when generation starts
                if(model->is_running && model->start_time_ms == 0) {
                    model->start_time_ms = furi_get_tick();
                } else if(!model->is_running) {
                    model->start_time_ms = 0;  // Reset when stopped
                }
                
                // Calculate display values based on current mode
                // This happens only when the model updates, not on every draw
                if(model->show_bits_per_sec && model->is_running) {
                    uint32_t elapsed_ms = furi_get_tick() - model->start_time_ms;
                    float elapsed_sec = elapsed_ms / 1000.0f;
                    if(elapsed_sec < 0.1f) elapsed_sec = 0.1f;
                    
                    model->hw_display_value = (uint32_t)(model->bits_from_hw_rng / elapsed_sec);
                    model->rf_display_value = (uint32_t)(model->bits_from_subghz_rssi / elapsed_sec);
                    model->ir_display_value = (uint32_t)(model->bits_from_infrared / elapsed_sec);
                } else {
                    model->hw_display_value = model->bits_from_hw_rng;
                    model->rf_display_value = model->bits_from_subghz_rssi;
                    model->ir_display_value = model->bits_from_infrared;
                }
            },
            true
        );
    }
}

// Byte Distribution view callbacks
void flipper_rng_byte_distribution_draw_callback(Canvas* canvas, void* context) {
    FlipperRngVisualizationModel* model = context;
    
    // Clear canvas
    canvas_clear(canvas);
    
    // Draw header
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "Byte Distribution");
    
    if(model->is_running) {
        // Show total bytes analyzed on second line with human-readable format
        canvas_set_font(canvas, FontSecondary);
        char buffer[48];
        char bytes_str[24];
        format_bytes(bytes_str, sizeof(bytes_str), model->bytes_generated);
        snprintf(buffer, sizeof(buffer), "Bytes: %s", bytes_str);
        canvas_draw_str(canvas, 2, 20, buffer);
        
        // Calculate statistics
        // Note: histogram bins count nibbles (4-bit values), not bytes
        // Each byte generates 2 nibbles (high and low), so total samples = bytes * 2
        uint32_t total_nibbles = 0;
        for(int i = 0; i < 16; i++) {
            total_nibbles += model->histogram[i];
        }
        if(total_nibbles == 0) total_nibbles = 1; // Avoid division by zero
        
        // Expected value per bin (perfectly uniform distribution)
        float expected_per_bin = (float)total_nibbles / 16.0f;
        if(expected_per_bin < 1.0f) expected_per_bin = 1.0f;
        
        // Calculate chi-squared statistic for goodness of fit
        float chi_squared = 0.0f;
        for(int i = 0; i < 16; i++) {
            float diff = (float)model->histogram[i] - expected_per_bin;
            chi_squared += (diff * diff) / expected_per_bin;
        }
        
        // Find min and max for "zoomed" scaling (show small differences)
        uint32_t min_val = model->histogram[0];
        uint32_t max_val = model->histogram[0];
        for(int i = 1; i < 16; i++) {
            if(model->histogram[i] < min_val) min_val = model->histogram[i];
            if(model->histogram[i] > max_val) max_val = model->histogram[i];
        }
        
        // Use a zoomed range to amplify small differences
        uint32_t range = max_val - min_val;
        uint32_t zoom_threshold = (uint32_t)(expected_per_bin / 20.0f);
        if(zoom_threshold < 1) zoom_threshold = 1;
        
        if(range < zoom_threshold) {
            // Very uniform - use narrow range for zoom
            range = zoom_threshold;
        }
        if(range == 0) range = 1;
        
        // Draw histogram bars with zoomed scaling
        int bar_width = 7;
        int bar_spacing = 1;
        int max_height = 20;
        int base_y = 45;
        
        for(int i = 0; i < 16; i++) {
            // Calculate bar height relative to min (zoomed view)
            int32_t value_above_min = (int32_t)model->histogram[i] - (int32_t)min_val;
            int bar_height = (value_above_min * max_height) / range;
            if(bar_height < 0) bar_height = 0;
            if(bar_height > max_height) bar_height = max_height;
            
            int x = 2 + i * (bar_width + bar_spacing);
            
            // Draw bar
            if(bar_height > 0) {
                canvas_draw_box(canvas, x, base_y - bar_height, bar_width, bar_height);
            }
            
            // Draw expected line (baseline) - dashed
            if(i % 2 == 0) {
                canvas_draw_dot(canvas, x + bar_width/2, base_y - max_height/2);
            }
        }
        
        // Draw baseline
        canvas_draw_line(canvas, 0, base_y + 1, 127, base_y + 1);
        
        // Labels for bins
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 54, "0");
        canvas_draw_str(canvas, 58, 54, "7F");
        canvas_draw_str(canvas, 112, 54, "FF");
        
        // Quality metrics
        canvas_draw_str(canvas, 2, 62, "Quality:");
        
        // Chi-squared p-value interpretation (15 degrees of freedom)
        // Good if chi-squared < 25 (p > 0.05), Excellent if < 22 (p > 0.10)
        const char* quality = "Perfect";
        if(chi_squared > 30) quality = "Poor";
        else if(chi_squared > 25) quality = "Fair";
        else if(chi_squared > 22) quality = "Good";
        
        snprintf(buffer, sizeof(buffer), "%s (X2=%.1f)", quality, (double)chi_squared);
        canvas_draw_str(canvas, 42, 62, buffer);
    } else {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 20, 35, "Start generator");
        canvas_draw_str(canvas, 20, 45, "to see histogram");
    }
}

void flipper_rng_byte_distribution_enter_callback(void* context) {
    FlipperRngApp* app = context;
    FURI_LOG_I(TAG, "Entering byte distribution view");
    
    // Sync state from app when entering the view
    with_view_model(
        app->byte_distribution_view,
        FlipperRngVisualizationModel* model,
        {
            model->is_running = app->state->is_running;
            model->bytes_generated = app->state->bytes_generated;
            // Copy histogram data
            for(int i = 0; i < 16; i++) {
                model->histogram[i] = app->state->byte_histogram[i];
            }
            FURI_LOG_I(TAG, "Byte distribution state synced: running=%d, bytes=%lu", 
                      model->is_running, model->bytes_generated);
        },
        true
    );
}

bool flipper_rng_byte_distribution_input_callback(InputEvent* event, void* context) {
    UNUSED(context);  // Mark context as unused to avoid compiler warning
    bool consumed = false;
    
    if(event->type == InputTypePress) {
        switch(event->key) {
        case InputKeyBack:
            // Back button handled by previous callback (returns to menu)
            consumed = true;
            break;
        default:
            break;
        }
    }
    
    return consumed;
}

// Source Stats View - Shows real-time entropy contribution from each source
void flipper_rng_source_stats_draw_callback(Canvas* canvas, void* context) {
    FlipperRngVisualizationModel* model = context;
    
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "Entropy Sources");
    
    if(!model->is_running) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 20, 35, "Start generator");
        canvas_draw_str(canvas, 20, 45, "to see source stats");
        return;
    }
    
    canvas_set_font(canvas, FontSecondary);
    
    // Use cached display values from the model
    // These are only updated when the model itself updates
    const char* unit = model->show_bits_per_sec ? "bits/s" : "bits";
    
    // Calculate total for percentage calculation
    uint32_t total_bits = model->bits_from_hw_rng + 
                         model->bits_from_subghz_rssi + 
                         model->bits_from_infrared;
    
    if(total_bits == 0) total_bits = 1; // Avoid division by zero
    
    // Y positions and dimensions
    int y = 18;  // Start position for first source
    int bar_width = 124;  // Full screen width minus margins
    int bar_height = 4;   // Slightly taller bars
    int bar_x = 2;
    int spacing = 14;  // Space between each source section
    
    // Hardware RNG
    uint32_t hw_percent = (model->bits_from_hw_rng * 100) / total_bits;
    char buffer[48];
    snprintf(buffer, sizeof(buffer), "HW RNG: %lu %s", model->hw_display_value, unit);
    canvas_draw_str(canvas, 2, y, buffer);
    
    // Draw progress bar below text
    int bar_y = y + 1;
    canvas_draw_frame(canvas, bar_x, bar_y, bar_width, bar_height);
    int fill_width = (bar_width - 2) * hw_percent / 100;
    if(fill_width > 0 && fill_width < bar_width - 2) {
        canvas_draw_box(canvas, bar_x + 1, bar_y + 1, fill_width, bar_height - 2);
    }
    
    y += spacing;
    
    // SubGHz RSSI
    uint32_t rf_percent = (model->bits_from_subghz_rssi * 100) / total_bits;
    snprintf(buffer, sizeof(buffer), "SubGHz: %lu %s", model->rf_display_value, unit);
    canvas_draw_str(canvas, 2, y, buffer);
    
    bar_y = y + 1;
    canvas_draw_frame(canvas, bar_x, bar_y, bar_width, bar_height);
    fill_width = (bar_width - 2) * rf_percent / 100;
    if(fill_width > 0 && fill_width < bar_width - 2) {
        canvas_draw_box(canvas, bar_x + 1, bar_y + 1, fill_width, bar_height - 2);
    }
    
    y += spacing;
    
    // Infrared
    uint32_t ir_percent = (model->bits_from_infrared * 100) / total_bits;
    snprintf(buffer, sizeof(buffer), "Infrared: %lu %s", model->ir_display_value, unit);
    canvas_draw_str(canvas, 2, y, buffer);
    
    bar_y = y + 1;
    canvas_draw_frame(canvas, bar_x, bar_y, bar_width, bar_height);
    fill_width = (bar_width - 2) * ir_percent / 100;
    if(fill_width > 0 && fill_width < bar_width - 2) {
        canvas_draw_box(canvas, bar_x + 1, bar_y + 1, fill_width, bar_height - 2);
    }
}

bool flipper_rng_source_stats_input_callback(InputEvent* event, void* context) {
    FlipperRngApp* app = context;
    bool consumed = false;
    
    if(event->type == InputTypePress && event->key == InputKeyOk) {
        with_view_model(
            app->source_stats_view,
            FlipperRngVisualizationModel* model,
            {
                // Toggle between total bits and bits/sec display
                model->show_bits_per_sec = !model->show_bits_per_sec;
                
                // Recalculate display values for the new mode
                if(model->show_bits_per_sec && model->is_running) {
                    uint32_t elapsed_ms = furi_get_tick() - model->start_time_ms;
                    float elapsed_sec = elapsed_ms / 1000.0f;
                    if(elapsed_sec < 0.1f) elapsed_sec = 0.1f;
                    
                    model->hw_display_value = (uint32_t)(model->bits_from_hw_rng / elapsed_sec);
                    model->rf_display_value = (uint32_t)(model->bits_from_subghz_rssi / elapsed_sec);
                    model->ir_display_value = (uint32_t)(model->bits_from_infrared / elapsed_sec);
                } else {
                    model->hw_display_value = model->bits_from_hw_rng;
                    model->rf_display_value = model->bits_from_subghz_rssi;
                    model->ir_display_value = model->bits_from_infrared;
                }
            },
            true);
        consumed = true;
    }
    
    return consumed;
}

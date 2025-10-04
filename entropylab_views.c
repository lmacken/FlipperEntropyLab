#include "entropylab_views.h"
#include "entropylab_entropy.h"
#include <gui/elements.h>
#include <math.h>


#define TAG "FlipperRNG"

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
            
            
            // Bytes generated
            char buffer[32];
            snprintf(buffer, sizeof(buffer), "Bytes: %lu", model->bytes_generated);
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
                // Toggle visualization mode between 0 and 1
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
            }
        } else {
            FURI_LOG_I(TAG, "Visualization: OK pressed while stopped, going back");
            return false; // Return to menu when OK is pressed while stopped
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

// Test view callbacks
void flipper_rng_test_draw_callback(Canvas* canvas, void* context) {
    FlipperRngTestModel* model = context;
    
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "RNG Quality Test");
    
    canvas_set_font(canvas, FontSecondary);
    
    if(model->is_testing) {
        // Show progress bar
        canvas_draw_str(canvas, 2, 22, "Collecting entropy...");
        
        // Progress bar
        canvas_draw_frame(canvas, 2, 26, 124, 8);
        int progress_width = (int)(model->test_progress * 122);
        if(progress_width > 0) {
            canvas_draw_box(canvas, 3, 27, progress_width, 6);
        }
        
        // Show bytes collected
        char buffer[64];
        snprintf(buffer, sizeof(buffer), "Bytes: %u / %u", 
                 (unsigned)model->bytes_collected, (unsigned)model->bytes_needed);
        canvas_draw_str(canvas, 2, 44, buffer);
    } else if(model->test_complete) {
        // Show test results
        canvas_draw_str(canvas, 2, 22, "Test Complete!");
        
        // Overall score with visual indicator
        char score_str[32];
        snprintf(score_str, sizeof(score_str), "Overall: %.1f%%", (double)(model->overall_score * 100));
        canvas_draw_str(canvas, 2, 34, score_str);
        
        // Quality bar for overall score
        canvas_draw_frame(canvas, 60, 30, 66, 6);
        int score_width = (int)(model->overall_score * 64);
        canvas_draw_box(canvas, 61, 31, score_width, 4);
        
        // Individual test results
        canvas_set_font(canvas, FontSecondary);
        char result[64];
        
        snprintf(result, sizeof(result), "Chi²: %.0f%% (%lu, exp:255)", 
                 (double)(model->chi_square_result * 100), (unsigned long)model->actual_chi_square);
        canvas_draw_str(canvas, 2, 44, result);
        
        snprintf(result, sizeof(result), "Bit Freq: %.1f%%", (double)(model->bit_frequency_result * 100));
        canvas_draw_str(canvas, 2, 52, result);
        
        snprintf(result, sizeof(result), "Runs: %.1f%%", (double)(model->runs_test_result * 100));
        canvas_draw_str(canvas, 2, 60, result);
    } else {
        // Initial state - show size selection
        canvas_draw_str(canvas, 2, 20, "Select test size:");
        
        // Size options with selection indicator - simplified layout
        const char* sizes[] = {"4 KB", "8 KB", "16 KB"};
        const char* desc[] = {"Quick", "Standard", "Thorough"};
        
        for(int i = 0; i < 3; i++) {
            int y = 32 + i * 10;
            if(i == model->selected_size) {
                // Highlight selected option
                canvas_draw_box(canvas, 0, y - 7, 128, 9);
                canvas_set_color(canvas, ColorWhite);
            }
            
            // Draw size on the left, description on the right
            canvas_draw_str(canvas, 2, y, sizes[i]);
            canvas_draw_str(canvas, 45, y, "-");
            canvas_draw_str(canvas, 55, y, desc[i]);
            
            if(i == model->selected_size) {
                canvas_set_color(canvas, ColorBlack);
            }
        }
        
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 62, "Up/Down: Select, OK: Start");
    }
}

bool flipper_rng_test_input_callback(InputEvent* event, void* context) {
    FlipperRngApp* app = context;
    bool consumed = false;
    
    if(event->type == InputTypeShort) {
        if(event->key == InputKeyBack) {
            // Stop test if running and return to menu
            with_view_model(
                app->test_view,
                FlipperRngTestModel* model,
                {
                    model->is_testing = false;
                    model->test_complete = false;
                    model->bytes_collected = 0;
                    model->test_progress = 0.0f;
                    model->selected_size = 0; // Reset to 16KB
                },
                false
            );
            
            // Stop the test
            if(app->state->test_running) {
                app->state->test_running = false;
                
                // Stop the worker if we started it for the test
                if(app->state->test_started_worker) {
                    FURI_LOG_I(TAG, "User cancelled test - stopping worker thread");
                    app->state->is_running = false;
                    app->state->test_started_worker = false;
                }
                
                // Free test buffer
                if(app->state->test_buffer) {
                    free(app->state->test_buffer);
                    app->state->test_buffer = NULL;
                    app->state->test_buffer_size = 0;
                    app->state->test_buffer_pos = 0;
                }
            }
            
            view_dispatcher_switch_to_view(app->view_dispatcher, FlipperRngViewMenu);
            consumed = true;
        } else if(event->key == InputKeyUp) {
            // Move selection up
            with_view_model(
                app->test_view,
                FlipperRngTestModel* model,
                {
                    if(!model->is_testing && !model->test_complete) {
                        if(model->selected_size > 0) {
                            model->selected_size--;
                        }
                    }
                },
                true
            );
            consumed = true;
        } else if(event->key == InputKeyDown) {
            // Move selection down
            with_view_model(
                app->test_view,
                FlipperRngTestModel* model,
                {
                    if(!model->is_testing && !model->test_complete) {
                        if(model->selected_size < 2) {
                            model->selected_size++;
                        }
                    }
                },
                true
            );
            consumed = true;
        } else if(event->key == InputKeyOk) {
            // Get selected size
            size_t test_size = 4096; // Default 4KB (safer for limited RAM)
            
            with_view_model(
                app->test_view,
                FlipperRngTestModel* model,
                {
                    // Determine size based on selection - reduced sizes for stability
                    switch(model->selected_size) {
                        case 0: test_size = 4096; break;     // 4KB - Quick test
                        case 1: test_size = 8192; break;     // 8KB - Standard test  
                        case 2: test_size = 16384; break;    // 16KB - Thorough test
                        default: test_size = 4096; break;    // Fallback to 4KB
                    }
                    
                    if(!model->is_testing) {
                        // Start new test
                        model->is_testing = true;
                        model->test_complete = false;
                        model->bytes_collected = 0;
                        model->bytes_needed = test_size;
                        model->test_progress = 0.0f;
                        model->overall_score = 0.0f;
                        model->chi_square_result = 0.0f;
                        model->bit_frequency_result = 0.0f;
                        model->runs_test_result = 0.0f;
                        model->actual_chi_square = 0;
                    }
                },
                true
            );
            
            // Make sure any previous test is fully stopped
            app->state->test_running = false;
            
            // Wait a bit for worker to stop writing to test buffer
            furi_delay_ms(10);
            
            // Allocate test buffer and start collecting
            if(app->state->test_buffer) {
                free(app->state->test_buffer);
                app->state->test_buffer = NULL;
            }
            
            // Safer memory allocation with size limits
            if(test_size > 131072) {  // Limit to 128KB max to prevent memory issues
                FURI_LOG_E(TAG, "Test size %zu too large, limiting to 128KB", test_size);
                test_size = 131072;
            }
            
            app->state->test_buffer = malloc(test_size);
            if(!app->state->test_buffer) {
                FURI_LOG_E(TAG, "Failed to allocate test buffer of size %zu", test_size);
                FURI_LOG_E(TAG, "Insufficient memory for test. Try smaller test size or restart app.");
                
                // Reset test state on allocation failure
                app->state->test_running = false;
                app->state->test_buffer_size = 0;
                app->state->test_buffer_pos = 0;
                
                // Update UI to show error
                with_view_model(
                    app->test_view,
                    FlipperRngTestModel* model,
                    {
                        model->is_testing = false;
                        model->test_complete = false;
                        model->bytes_collected = 0;
                        model->test_progress = 0.0f;
                        snprintf(model->result_text, sizeof(model->result_text), "Memory allocation failed");
                    },
                    true
                );
                return false;
            }
            
            // Clear the buffer to prevent reading uninitialized memory
            memset(app->state->test_buffer, 0, test_size);
            
            app->state->test_buffer_size = test_size;
            app->state->test_buffer_pos = 0;
            app->state->test_running = true;
            
            // Test will use the main generator thread which should already be running
            // If not running, the test view's enter callback will start it
            FURI_LOG_I(TAG, "Test configured, generator should be running for entropy collection");
            
            consumed = true;
        }
    }
    
    return consumed;
}

// Test view enter callback - auto-start generator for realistic testing
void flipper_rng_test_enter_callback(void* context) {
    FlipperRngApp* app = context;
    
    FURI_LOG_I(TAG, "Entering Test Quality view");
    
    // Auto-start the main generator if not already running
    // This ensures test uses real entropy collection conditions
    if(!app->state->is_running) {
        FURI_LOG_I(TAG, "Auto-starting generator for Test Quality...");
        
        // Set LED to blinking green during test entropy collection
        flipper_rng_set_led_generating(app);
        
        // Use the same logic as the main "Start Generator" button
        // Ensure worker thread is stopped first
        if(furi_thread_get_state(app->worker_thread) != FuriThreadStateStopped) {
            FURI_LOG_I(TAG, "Waiting for worker thread to stop...");
            app->state->is_running = false;
            furi_thread_join(app->worker_thread);
        }
        
        // Initialize UART if needed (for consistency with main generator)
        if(app->state->output_mode == OutputModeUART && !app->state->serial_handle) {
            app->state->serial_handle = furi_hal_serial_control_acquire(FuriHalSerialIdUsart);
            if(app->state->serial_handle) {
                furi_hal_serial_init(app->state->serial_handle, 115200);
                FURI_LOG_I(TAG, "UART initialized for test");
            }
        }
        
        // Reset counters for fresh start
        app->state->bytes_generated = 0;
        app->state->samples_collected = 0;
        app->state->bits_from_hw_rng = 0;
        app->state->bits_from_subghz_rssi = 0;
        app->state->bits_from_infrared = 0;
        memset(app->state->byte_histogram, 0, sizeof(app->state->byte_histogram));
        
        // Start the main generator worker thread
        app->state->is_running = true;
        furi_thread_start(app->worker_thread);
        
        FURI_LOG_I(TAG, "Generator auto-started for Test Quality");
    } else {
        FURI_LOG_I(TAG, "Generator already running for Test Quality");
    }
}

// Test view exit callback - stop generator if we started it
void flipper_rng_test_exit_callback(void* context) {
    FlipperRngApp* app = context;
    
    FURI_LOG_I(TAG, "Exiting Test Quality view");
    
    // Stop any running test
    app->state->test_running = false;
    
    // Clean up test buffer
    if(app->state->test_buffer) {
        free(app->state->test_buffer);
        app->state->test_buffer = NULL;
        app->state->test_buffer_size = 0;
        app->state->test_buffer_pos = 0;
    }
    
    // Stop the generator (it will be auto-started by other views if needed)
    if(app->state->is_running) {
        FURI_LOG_I(TAG, "Auto-stopping generator after Test Quality");
        app->state->is_running = false;
        
        // Set LED back to red when stopping test entropy collection
        flipper_rng_set_led_stopped(app);
        
        // Clean up UART if we initialized it
        if(app->state->serial_handle) {
            furi_hal_serial_deinit(app->state->serial_handle);
            furi_hal_serial_control_release(app->state->serial_handle);
            app->state->serial_handle = NULL;
        }
    }
}

void flipper_rng_test_update(FlipperRngApp* app, const uint8_t* data, size_t length) {
    if(!app->state->test_running || !app->state->test_buffer) {
        return;
    }
    
    // Copy data to test buffer
    size_t copy_len = length;
    if(app->state->test_buffer_pos + copy_len > app->state->test_buffer_size) {
        copy_len = app->state->test_buffer_size - app->state->test_buffer_pos;
    }
    
    if(copy_len > 0) {
        memcpy(app->state->test_buffer + app->state->test_buffer_pos, data, copy_len);
        app->state->test_buffer_pos += copy_len;
    }
    
    // Update progress
    with_view_model(
        app->test_view,
        FlipperRngTestModel* model,
        {
            model->bytes_collected = app->state->test_buffer_pos;
            model->test_progress = (float)app->state->test_buffer_pos / (float)app->state->test_buffer_size;
            
            // Check if collection is complete
            if(app->state->test_buffer_pos >= app->state->test_buffer_size) {
                model->is_testing = false;
                model->test_complete = true;
                
                // Run statistical tests with safety checks
                FURI_LOG_I(TAG, "Running statistical tests on %zu bytes", app->state->test_buffer_size);
                
                // Chi-square test for byte distribution
                uint32_t byte_counts[256] = {0};
                for(size_t i = 0; i < app->state->test_buffer_size && i < 65536; i++) { // Bounds check
                    if(app->state->test_buffer && i < app->state->test_buffer_size) {
                        byte_counts[app->state->test_buffer[i]]++;
                    }
                }
                
                float expected = (float)app->state->test_buffer_size / 256.0f;
                float chi_square = 0.0f;
                for(int i = 0; i < 256; i++) {
                    float diff = (float)byte_counts[i] - expected;
                    chi_square += (diff * diff) / expected;
                }
                
                // Store actual value
                model->actual_chi_square = (uint32_t)chi_square;
                
                // Chi-square test with proper statistical critical values
                // df = 255 (256 byte values - 1)
                // Critical values from chi-square distribution table
                
                FURI_LOG_I(TAG, "Chi-square result: %.2f (df=255, expected~255)", (double)chi_square);
                
                // Chi-square interpretation balanced for practical entropy assessment
                // Accounts for small sample effects while maintaining statistical validity
                if(chi_square >= 200.9f && chi_square <= 311.6f) {
                    // Within 99% confidence interval - excellent randomness
                    model->chi_square_result = 0.99f;
                    FURI_LOG_I(TAG, "Chi-square: EXCELLENT (99%% confidence)");
                } else if(chi_square >= 190.0f && chi_square <= 330.0f) {
                    // Slightly wider than 95% - very good randomness
                    model->chi_square_result = 0.90f;
                    FURI_LOG_I(TAG, "Chi-square: VERY GOOD (extended 95%% bounds)");
                } else if(chi_square >= 170.0f && chi_square <= 360.0f) {
                    // Practical bounds for small samples - good randomness
                    model->chi_square_result = 0.80f;
                    FURI_LOG_I(TAG, "Chi-square: GOOD (practical bounds for small samples)");
                } else if(chi_square >= 140.0f && chi_square <= 400.0f) {
                    // Wide bounds - acceptable for entropy generation
                    model->chi_square_result = 0.65f;
                    FURI_LOG_I(TAG, "Chi-square: ACCEPTABLE (%.2f - usable entropy)", (double)chi_square);
                } else {
                    // Outside reasonable bounds - may indicate bias
                    model->chi_square_result = 0.40f;
                    FURI_LOG_W(TAG, "Chi-square: CONCERNING (%.2f - investigate entropy sources)", (double)chi_square);
                }
                
                // Bit frequency test with safety checks
                uint32_t ones = 0;
                for(size_t i = 0; i < app->state->test_buffer_size && i < 65536; i++) { // Bounds check
                    if(app->state->test_buffer && i < app->state->test_buffer_size) {
                        uint8_t byte = app->state->test_buffer[i];
                        for(int b = 0; b < 8; b++) {
                            if(byte & (1 << b)) ones++;
                        }
                    }
                }
                uint32_t total_bits = app->state->test_buffer_size * 8;
                float bit_ratio = (float)ones / (float)total_bits;
                
                FURI_LOG_I(TAG, "Bit frequency: %lu ones / %lu total = %.4f%% (expect ~50%%)", 
                          ones, total_bits, (double)(bit_ratio * 100.0f));
                
                // Bit frequency test with proper statistical thresholds
                // For large samples, should be very close to 50%
                float bit_deviation = fabsf(bit_ratio - 0.5f);
                
                if(bit_deviation < 0.005f) {        // Within 0.5% - excellent
                    model->bit_frequency_result = 0.99f;
                    FURI_LOG_I(TAG, "Bit frequency: EXCELLENT (%.3f%% deviation)", (double)(bit_deviation * 100));
                } else if(bit_deviation < 0.01f) {   // Within 1% - very good
                    model->bit_frequency_result = 0.95f;
                    FURI_LOG_I(TAG, "Bit frequency: VERY GOOD (%.3f%% deviation)", (double)(bit_deviation * 100));
                } else if(bit_deviation < 0.02f) {   // Within 2% - good
                    model->bit_frequency_result = 0.90f;
                    FURI_LOG_I(TAG, "Bit frequency: GOOD (%.3f%% deviation)", (double)(bit_deviation * 100));
                } else if(bit_deviation < 0.05f) {   // Within 5% - acceptable
                    model->bit_frequency_result = 0.70f;
                    FURI_LOG_W(TAG, "Bit frequency: ACCEPTABLE (%.3f%% deviation)", (double)(bit_deviation * 100));
                } else {                             // >5% deviation - poor
                    model->bit_frequency_result = 0.30f;
                    FURI_LOG_W(TAG, "Bit frequency: POOR (%.3f%% deviation - bias detected)", (double)(bit_deviation * 100));
                }
                
                // Runs test (sequences of same bits) with safety checks
                uint32_t runs = 0;
                bool last_bit = false;
                bool first_bit = true;
                for(size_t i = 0; i < app->state->test_buffer_size && i < 65536; i++) { // Bounds check
                    if(app->state->test_buffer && i < app->state->test_buffer_size) {
                        uint8_t byte = app->state->test_buffer[i];
                        for(int b = 0; b < 8; b++) {
                            bool bit = (byte >> b) & 1;
                            if(first_bit) {
                                last_bit = bit;
                                first_bit = false;
                            } else if(bit != last_bit) {
                                runs++;
                                last_bit = bit;
                            }
                        }
                    }
                }
                
                // Expected runs for random data is approximately total_bits/2
                uint32_t expected_runs = total_bits / 2;
                float runs_ratio = 0.0f;
                if(expected_runs > 0 && total_bits > 0) {
                    runs_ratio = (float)runs / (float)expected_runs;
                } else {
                    FURI_LOG_W(TAG, "Invalid test data: total_bits=%lu, expected_runs=%lu", total_bits, expected_runs);
                }
                
                FURI_LOG_I(TAG, "Runs test: %lu runs / %lu expected = %.4f ratio (expect ~1.0)", 
                          runs, expected_runs, (double)runs_ratio);
                
                // Runs test with improved statistical interpretation
                // Good randomness should have runs ratio close to 1.0
                float runs_deviation = fabsf(runs_ratio - 1.0f);
                
                if(runs_deviation < 0.03f) {         // Within 3% - excellent
                    model->runs_test_result = 0.99f;
                    FURI_LOG_I(TAG, "Runs test: EXCELLENT (%.2f%% deviation)", (double)(runs_deviation * 100));
                } else if(runs_deviation < 0.05f) {  // Within 5% - very good
                    model->runs_test_result = 0.95f;
                    FURI_LOG_I(TAG, "Runs test: VERY GOOD (%.2f%% deviation)", (double)(runs_deviation * 100));
                } else if(runs_deviation < 0.10f) {  // Within 10% - good
                    model->runs_test_result = 0.90f;
                    FURI_LOG_I(TAG, "Runs test: GOOD (%.2f%% deviation)", (double)(runs_deviation * 100));
                } else if(runs_deviation < 0.20f) {  // Within 20% - acceptable
                    model->runs_test_result = 0.70f;
                    FURI_LOG_W(TAG, "Runs test: ACCEPTABLE (%.2f%% deviation)", (double)(runs_deviation * 100));
                } else {                             // >20% deviation - poor
                    model->runs_test_result = 0.30f;
                    FURI_LOG_W(TAG, "Runs test: POOR (%.2f%% deviation - pattern detected)", (double)(runs_deviation * 100));
                }
                
                // Calculate overall score with weighted average
                // Chi-square is most important for detecting bias
                model->overall_score = (model->chi_square_result * 0.5f + 
                                       model->bit_frequency_result * 0.3f + 
                                       model->runs_test_result * 0.2f);
                
                FURI_LOG_I(TAG, "Overall quality score: %.1f%% (Chi²: %.1f%%, Bit: %.1f%%, Runs: %.1f%%)", 
                          (double)(model->overall_score * 100),
                          (double)(model->chi_square_result * 100),
                          (double)(model->bit_frequency_result * 100),
                          (double)(model->runs_test_result * 100));
                
                // Log final assessment
                if(model->overall_score >= 0.95f) {
                    FURI_LOG_I(TAG, "FINAL ASSESSMENT: EXCELLENT randomness quality");
                } else if(model->overall_score >= 0.90f) {
                    FURI_LOG_I(TAG, "FINAL ASSESSMENT: VERY GOOD randomness quality");
                } else if(model->overall_score >= 0.80f) {
                    FURI_LOG_I(TAG, "FINAL ASSESSMENT: GOOD randomness quality");
                } else if(model->overall_score >= 0.70f) {
                    FURI_LOG_W(TAG, "FINAL ASSESSMENT: ACCEPTABLE randomness quality");
                } else {
                    FURI_LOG_W(TAG, "FINAL ASSESSMENT: POOR randomness quality - investigate entropy sources");
                }
                
                // Stop the test
                app->state->test_running = false;
                
                // Stop the worker if we started it for the test
                if(app->state->test_started_worker) {
                    FURI_LOG_I(TAG, "Stopping worker thread that was started for test");
                    app->state->is_running = false;
                    app->state->test_started_worker = false;
                }
                
                // Free test buffer after completion
                if(app->state->test_buffer) {
                    free(app->state->test_buffer);
                    app->state->test_buffer = NULL;
                    app->state->test_buffer_size = 0;
                    app->state->test_buffer_pos = 0;
                }
            }
        },
        true
    );
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
        // Show total bytes analyzed on second line
        canvas_set_font(canvas, FontSecondary);
        char buffer[48];
        snprintf(buffer, sizeof(buffer), "Bytes: %lu", model->bytes_generated);
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

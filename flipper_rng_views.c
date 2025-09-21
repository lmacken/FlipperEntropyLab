#include "flipper_rng_views.h"
#include "flipper_rng_entropy.h"
#include <gui/elements.h>
#include <math.h>

#define TAG "FlipperRNG"

// Configuration options
static const char* entropy_source_names[] = {
    "All",
    "HW RNG",
    "ADC+HW",
    "RF+HW",
    "IR+HW",
    "Env Only",
};

static const char* output_mode_names[] = {
    "USB",
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

static const uint32_t visual_refresh_values[] = {
    100, 200, 500, 1000,
};

static const uint32_t poll_interval_values[] = {
    1, 5, 10, 50, 100, 500,
};

static const uint32_t entropy_source_values[] = {
    EntropySourceAll,                                                              // All 6 sources
    EntropySourceHardwareRNG,                                                      // HW RNG only
    EntropySourceHardwareRNG | EntropySourceADC,                                  // ADC+HW
    EntropySourceHardwareRNG | EntropySourceSubGhzRSSI,                          // RF+HW
    EntropySourceHardwareRNG | EntropySourceInfraredNoise,                       // IR+HW
    EntropySourceBatteryVoltage | EntropySourceTemperature | EntropySourceADC,   // Env Only
};

void flipper_rng_source_changed(VariableItem* item) {
    FlipperRngApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    
    switch(index) {
    case 0: // All high-quality sources
        app->state->entropy_sources = EntropySourceAll;
        break;
    case 1: // HW RNG only
        app->state->entropy_sources = EntropySourceHardwareRNG;
        break;
    case 2: // ADC + HW RNG
        app->state->entropy_sources = EntropySourceHardwareRNG | EntropySourceADC;
        break;
    case 3: // RF + HW RNG
        app->state->entropy_sources = EntropySourceHardwareRNG | EntropySourceSubGhzRSSI;
        break;
    case 4: // IR + HW RNG
        app->state->entropy_sources = EntropySourceHardwareRNG | EntropySourceInfraredNoise;
        break;
    case 5: // Environmental only (no HW RNG)
        app->state->entropy_sources = EntropySourceBatteryVoltage | EntropySourceTemperature | EntropySourceADC;
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
        canvas_draw_str(canvas, 2, 10, "FlipperRNG v1.0");
        
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
    } else if(model->viz_mode == 2) {
        // MODE 2: Histogram visualization
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 2, 8, "Byte Distribution");
        
        canvas_set_font(canvas, FontSecondary);
        
        if(model->is_running) {
            // Find max value for scaling
            uint32_t max_val = 1;
            for(int i = 0; i < 16; i++) {
                if(model->histogram[i] > max_val) {
                    max_val = model->histogram[i];
                }
            }
            
            // Draw histogram bars
            int bar_width = 7;
            int bar_spacing = 1;
            int max_height = 40;
            int base_y = 55;
            
            for(int i = 0; i < 16; i++) {
                int bar_height = (model->histogram[i] * max_height) / max_val;
                if(bar_height > 0) {
                    int x = 2 + i * (bar_width + bar_spacing);
                    // Draw bar
                    canvas_draw_box(canvas, x, base_y - bar_height, bar_width, bar_height);
                }
            }
            
            // Draw axis
            canvas_draw_line(canvas, 0, base_y + 1, 127, base_y + 1);
            
            // Labels for bins
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str(canvas, 2, 63, "0");
            canvas_draw_str(canvas, 60, 63, "7F");
            canvas_draw_str(canvas, 115, 63, "FF");
            
            // Show total bytes analyzed
            char buffer[32];
            snprintf(buffer, sizeof(buffer), "Bytes: %lu", model->bytes_generated);
            canvas_draw_str(canvas, 70, 8, buffer);
        } else {
            canvas_draw_str(canvas, 20, 30, "Start generator");
            canvas_draw_str(canvas, 20, 40, "to see histogram");
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
        } else if(event->key == InputKeyUp) {
            // Switch to histogram mode (mode 2)
            FURI_LOG_I(TAG, "Up button pressed - showing histogram");
            with_view_model(
                app->visualization_view,
                FlipperRngVisualizationModel* model,
                {
                    model->viz_mode = 2; // Histogram mode
                    // Copy histogram data
                    for(int i = 0; i < 16; i++) {
                        model->histogram[i] = app->state->byte_histogram[i];
                    }
                },
                true
            );
            consumed = true;
        } else {
            FURI_LOG_I(TAG, "Visualization: OK pressed while stopped, going back");
            return false; // Return to menu when OK is pressed while stopped
        }
    }
    
    return consumed;
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
            model->bytes_generated = app->state->bytes_generated;
            model->data_pos = (model->data_pos + 1) % 128;
            
            // Don't update walk position here - let the draw callback handle it
            // This prevents the walk from drifting as data updates
        },
        true
    );
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
            
            app->state->test_buffer = malloc(test_size);
            if(!app->state->test_buffer) {
                FURI_LOG_E(TAG, "Failed to allocate test buffer of size %zu", test_size);
                FURI_LOG_E(TAG, "Insufficient memory for test. Try smaller test size.");
                
                // Update UI to show error
                with_view_model(
                    app->test_view,
                    FlipperRngTestModel* model,
                    {
                        model->is_testing = false;
                        model->test_complete = false;
                        model->bytes_collected = 0;
                        model->test_progress = 0.0f;
                    },
                    true
                );
                return false;
            }
            
            app->state->test_buffer_size = test_size;
            app->state->test_buffer_pos = 0;
            app->state->test_running = true;
            
            // Track if we need to start the worker for the test
            static bool test_started_worker = false;
            test_started_worker = false;
            
            // Start the worker if not running
            if(!app->state->is_running) {
                FURI_LOG_I(TAG, "Starting worker thread for test");
                app->state->is_running = true;
                test_started_worker = true;
                if(furi_thread_get_state(app->worker_thread) == FuriThreadStateStopped) {
                    furi_thread_start(app->worker_thread);
                }
            } else {
                FURI_LOG_I(TAG, "Using existing worker thread for test");
            }
            
            // Store whether we started the worker (for cleanup later)
            app->state->test_started_worker = test_started_worker;
            
            consumed = true;
        }
    }
    
    return consumed;
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

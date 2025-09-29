#pragma once

#include "entropylab.h"
#include <gui/view.h>
#include <gui/modules/variable_item_list.h>

// Visualization model
typedef struct {
    uint8_t random_data[128];
    size_t data_pos;
    bool is_running;
    uint32_t bytes_generated;
    uint8_t viz_mode;  // 0 = original, 1 = full screen walk, 2 = histogram
    uint8_t walk_x;
    uint8_t walk_y;
    uint32_t histogram[16];  // Histogram data for 16 bins
    // Per-source bit counters for stats view
    uint32_t bits_from_hw_rng;
    uint32_t bits_from_subghz_rssi;
    uint32_t bits_from_infrared;
    // For bits/sec calculation
    bool show_bits_per_sec;  // Toggle between total bits and bits/sec
    uint32_t start_time_ms;   // When generation started
    // Cached display values (updated only when model updates)
    uint32_t hw_display_value;
    uint32_t rf_display_value;
    uint32_t ir_display_value;
} FlipperRngVisualizationModel;

// Test model
typedef struct {
    bool is_testing;
    size_t bytes_collected;
    size_t bytes_needed;
    float test_progress;
    
    // Test configuration
    uint8_t selected_size;  // 0=16KB, 1=32KB, 2=64KB, 3=128KB
    
    // Test results
    bool test_complete;
    float chi_square_result;
    float bit_frequency_result;
    float runs_test_result;
    float overall_score;
    uint32_t actual_chi_square;  // Store actual value for display
    char result_text[256];
} FlipperRngTestModel;

// Configuration callbacks
void flipper_rng_setup_config_view(FlipperRngApp* app);

// Source selection callback
void flipper_rng_source_changed(VariableItem* item);

// Output mode callback  
void flipper_rng_output_mode_changed(VariableItem* item);

// Poll interval callback
void flipper_rng_poll_interval_changed(VariableItem* item);

// Visual refresh rate callback
void flipper_rng_visual_refresh_changed(VariableItem* item);

// Mixing mode callback
void flipper_rng_mixing_mode_changed(VariableItem* item);

// Wordlist selection callback
void flipper_rng_wordlist_changed(VariableItem* item);

// External entropy callback

// Visualization callbacks are declared in flipper_rng.h
void flipper_rng_visualization_update(FlipperRngApp* app, uint8_t* data, size_t length);

// Test view callbacks
void flipper_rng_test_draw_callback(Canvas* canvas, void* context);
bool flipper_rng_test_input_callback(InputEvent* event, void* context);
void flipper_rng_test_enter_callback(void* context);
void flipper_rng_test_exit_callback(void* context);
void flipper_rng_test_update(FlipperRngApp* app, const uint8_t* data, size_t length);

// Byte distribution view callbacks
void flipper_rng_byte_distribution_draw_callback(Canvas* canvas, void* context);
bool flipper_rng_byte_distribution_input_callback(InputEvent* event, void* context);

// Source stats view callbacks
void flipper_rng_source_stats_draw_callback(Canvas* canvas, void* context);
bool flipper_rng_source_stats_input_callback(InputEvent* event, void* context);

// Passphrase view callbacks and functions
void flipper_rng_passphrase_draw_callback(Canvas* canvas, void* context);
bool flipper_rng_passphrase_input_callback(InputEvent* event, void* context);
void flipper_rng_passphrase_enter_callback(void* context);
void flipper_rng_passphrase_exit_callback(void* context);
View* flipper_rng_passphrase_view_alloc(FlipperRngApp* app);
void flipper_rng_passphrase_view_free(View* view);

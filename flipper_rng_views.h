#pragma once

#include "flipper_rng.h"
#include <gui/view.h>
#include <gui/modules/variable_item_list.h>

// Visualization model
typedef struct {
    uint8_t random_data[128];
    size_t data_pos;
    bool is_running;
    float entropy_quality;
    uint32_t bytes_generated;
    uint8_t viz_mode;  // 0 = original, 1 = full screen walk
    uint8_t walk_x;
    uint8_t walk_y;
} FlipperRngVisualizationModel;

// Configuration callbacks
void flipper_rng_setup_config_view(FlipperRngApp* app);

// Source selection callback
void flipper_rng_source_changed(VariableItem* item);

// Output mode callback  
void flipper_rng_output_mode_changed(VariableItem* item);

// Poll interval callback
void flipper_rng_poll_interval_changed(VariableItem* item);

// Visualization callbacks are declared in flipper_rng.h
void flipper_rng_visualization_update(FlipperRngApp* app, uint8_t* data, size_t length);

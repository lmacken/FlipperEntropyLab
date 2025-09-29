#include "entropylab.h"
#include "entropylab_about.h"
#include <gui/elements.h>
#include <string.h>
#include <stdio.h>

#define TAG "FlipperRNG"

typedef struct {
    // Simplified - no QR code functionality
    bool unused; // Keep struct non-empty
} FlipperRngAboutModel;

static void flipper_rng_about_draw_callback(Canvas* canvas, void* context) {
    FlipperRngAboutModel* model = context;
    UNUSED(model);
    
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    
    // Simple centered title for v1.0 release
    canvas_set_font(canvas, FontPrimary);
    char title[32];
    snprintf(title, sizeof(title), "Entropy Lab v%s", FLIPPER_RNG_VERSION);
    canvas_draw_str_aligned(canvas, 64, 10, AlignCenter, AlignTop, title);
    
    // Draw bordered info section (moved down to align)
    canvas_set_font(canvas, FontSecondary);
    
    // Draw border around info section
    canvas_draw_frame(canvas, 2, 24, 124, 36);
    
    // Centered text inside the bordered section (adjusted for new frame position)
    canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignTop, "High-quality entropy");
    canvas_draw_str_aligned(canvas, 64, 38, AlignCenter, AlignTop, "from HW RNG, RF & IR");
    canvas_draw_str_aligned(canvas, 64, 50, AlignCenter, AlignTop, "Created by Luke Macken");
}

static bool flipper_rng_about_input_callback(InputEvent* event, void* context) {
    UNUSED(context);
    UNUSED(event);
    
    // No input handling needed for simplified About screen
    return false;
}

View* flipper_rng_about_view_alloc(void) {
    View* view = view_alloc();
    view_allocate_model(view, ViewModelTypeLocking, sizeof(FlipperRngAboutModel));
    view_set_context(view, view);
    view_set_draw_callback(view, flipper_rng_about_draw_callback);
    view_set_input_callback(view, flipper_rng_about_input_callback);
    
    return view;
}

void flipper_rng_about_view_free(View* view) {
    furi_assert(view);
    view_free(view);
}

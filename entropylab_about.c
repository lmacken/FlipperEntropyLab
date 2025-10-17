#include "entropylab.h"
#include "entropylab_about.h"
#include <gui/elements.h>
#include <string.h>
#include <stdio.h>

#define TAG "EntropyLab"

typedef struct {
    // Simplified - no QR code functionality
    bool unused; // Keep struct non-empty
} FlipperRngAboutModel;

static void flipper_rng_about_draw_callback(Canvas* canvas, void* context) {
    FlipperRngAboutModel* model = context;
    UNUSED(model);
    
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    
    // Title
    canvas_set_font(canvas, FontPrimary);
    char title[32];
    snprintf(title, sizeof(title), "Entropy Lab v%s", FLIPPER_RNG_VERSION);
    canvas_draw_str_aligned(canvas, 64, 4, AlignCenter, AlignTop, title);
    
    // Tagline
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 16, AlignCenter, AlignTop, "Chaos-powered randomness!");
    canvas_draw_str_aligned(canvas, 64, 24, AlignCenter, AlignTop, "RF noise + IR + HW RNG");
    
    // Warning box
    canvas_draw_frame(canvas, 2, 34, 124, 18);
    canvas_draw_str_aligned(canvas, 64, 37, AlignCenter, AlignTop, "EXPERIMENTAL SOFTWARE");
    canvas_draw_str_aligned(canvas, 64, 45, AlignCenter, AlignTop, "Use at your own risk!");
    
    // Footer
    canvas_draw_str_aligned(canvas, 64, 56, AlignCenter, AlignTop, "By Luke Macken");
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

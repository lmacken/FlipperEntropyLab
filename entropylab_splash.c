#include "entropylab_splash.h"
#include <furi.h>
#include <gui/canvas.h>
#include <gui/view.h>
#include <gui/elements.h>

#define TAG "EntropyLab_Splash"
#define SPLASH_DURATION_MS 3000
#define ANIMATION_TICK_MS 50
#define MAX_PARTICLES 20
#define ANTENNA_X 64
#define ANTENNA_Y 8

typedef struct {
    float x;
    float y;
    float vx;  // velocity x
    float vy;  // velocity y
    uint8_t life;  // particle lifetime
    bool active;
} RandomWalkParticle;

typedef struct {
    uint32_t start_time;
    uint32_t current_time;
    uint8_t frame_counter;
    RandomWalkParticle particles[MAX_PARTICLES];
    bool animation_done;
} FlipperRngSplashModel;

struct FlipperRngSplash {
    View* view;
    FuriTimer* timer;
};

// Simple Flipper character ASCII art
static void draw_flipper_character(Canvas* canvas, uint8_t frame) {
    // Draw a simple Flipper with antenna
    canvas_set_font(canvas, FontSecondary);
    
    // Body (simplified)
    canvas_draw_str(canvas, 52, 30, "___");
    canvas_draw_str(canvas, 50, 38, "(o.o)");
    canvas_draw_str(canvas, 50, 46, " ) ) ");
    canvas_draw_str(canvas, 50, 54, "(___)");
    
    // Antenna with animation
    if(frame % 4 < 2) {
        canvas_draw_line(canvas, ANTENNA_X, 22, ANTENNA_X, ANTENNA_Y);
        canvas_draw_circle(canvas, ANTENNA_X, ANTENNA_Y - 2, 2);
    } else {
        canvas_draw_line(canvas, ANTENNA_X, 22, ANTENNA_X - 1, ANTENNA_Y);
        canvas_draw_circle(canvas, ANTENNA_X - 1, ANTENNA_Y - 2, 2);
    }
}

static void update_particles(FlipperRngSplashModel* model) {
    // Spawn new particle occasionally
    if(model->frame_counter % 5 == 0) {
        for(int i = 0; i < MAX_PARTICLES; i++) {
            if(!model->particles[i].active) {
                model->particles[i].x = ANTENNA_X;
                model->particles[i].y = ANTENNA_Y;
                // Random walk velocities
                model->particles[i].vx = (float)((rand() % 100) - 50) / 25.0f;
                model->particles[i].vy = (float)((rand() % 100) - 80) / 30.0f;
                model->particles[i].life = 30 + (rand() % 20);
                model->particles[i].active = true;
                break;
            }
        }
    }
    
    // Update existing particles
    for(int i = 0; i < MAX_PARTICLES; i++) {
        if(model->particles[i].active) {
            RandomWalkParticle* p = &model->particles[i];
            
            // Random walk - add small random changes to velocity
            p->vx += (float)((rand() % 100) - 50) / 200.0f;
            p->vy += (float)((rand() % 100) - 50) / 200.0f;
            
            // Apply velocity
            p->x += p->vx;
            p->y += p->vy;
            
            // Apply slight gravity/drift
            p->vy += 0.05f;
            
            // Decrease life
            p->life--;
            
            // Deactivate if dead or out of bounds
            if(p->life <= 0 || p->x < 0 || p->x > 128 || p->y < 0 || p->y > 64) {
                p->active = false;
            }
        }
    }
}

static void flipper_rng_splash_draw_callback(Canvas* canvas, void* context) {
    FlipperRngSplashModel* model = context;
    
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    
    // Draw title
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, "FlipperRNG");
    
    // Draw Flipper character
    draw_flipper_character(canvas, model->frame_counter);
    
    // Draw random walk particles
    for(int i = 0; i < MAX_PARTICLES; i++) {
        if(model->particles[i].active) {
            RandomWalkParticle* p = &model->particles[i];
            
            // Draw particle with fading effect based on life
            if(p->life > 20) {
                // Full brightness
                canvas_draw_dot(canvas, (int)p->x, (int)p->y);
                // Draw a small trail
                if(i % 2 == 0) {
                    canvas_draw_dot(canvas, (int)(p->x - p->vx), (int)(p->y - p->vy));
                }
            } else if(p->life > 10) {
                // Medium brightness (draw every other frame)
                if(model->frame_counter % 2 == 0) {
                    canvas_draw_dot(canvas, (int)p->x, (int)p->y);
                }
            } else {
                // Low brightness (draw every third frame)
                if(model->frame_counter % 3 == 0) {
                    canvas_draw_dot(canvas, (int)p->x, (int)p->y);
                }
            }
        }
    }
    
    // Draw loading text with animation
    canvas_set_font(canvas, FontSecondary);
    const char* loading_text = "Initializing entropy...";
    int dots = (model->frame_counter / 10) % 4;
    char loading_with_dots[32];
    snprintf(loading_with_dots, sizeof(loading_with_dots), "%s%.*s", 
             loading_text, dots, "...");
    canvas_draw_str_aligned(canvas, 64, 60, AlignCenter, AlignBottom, loading_with_dots);
}

static void flipper_rng_splash_timer_callback(void* context) {
    FlipperRngSplash* splash = context;
    
    with_view_model(
        splash->view,
        FlipperRngSplashModel* model,
        {
            model->current_time = furi_get_tick() - model->start_time;
            model->frame_counter++;
            
            // Update particle physics
            update_particles(model);
            
            // Check if animation is done
            if(model->current_time >= SPLASH_DURATION_MS) {
                model->animation_done = true;
            }
        },
        true
    );
}

static void flipper_rng_splash_enter_callback(void* context) {
    FlipperRngSplash* splash = context;
    
    with_view_model(
        splash->view,
        FlipperRngSplashModel* model,
        {
            model->start_time = furi_get_tick();
            model->current_time = 0;
            model->frame_counter = 0;
            model->animation_done = false;
            
            // Initialize particles as inactive
            for(int i = 0; i < MAX_PARTICLES; i++) {
                model->particles[i].active = false;
            }
        },
        true
    );
    
    furi_timer_start(splash->timer, ANIMATION_TICK_MS);
}

static void flipper_rng_splash_exit_callback(void* context) {
    FlipperRngSplash* splash = context;
    furi_timer_stop(splash->timer);
}

FlipperRngSplash* flipper_rng_splash_alloc(void) {
    FlipperRngSplash* splash = malloc(sizeof(FlipperRngSplash));
    
    splash->view = view_alloc();
    view_set_context(splash->view, splash);
    view_allocate_model(splash->view, ViewModelTypeLocking, sizeof(FlipperRngSplashModel));
    view_set_draw_callback(splash->view, flipper_rng_splash_draw_callback);
    view_set_enter_callback(splash->view, flipper_rng_splash_enter_callback);
    view_set_exit_callback(splash->view, flipper_rng_splash_exit_callback);
    
    splash->timer = furi_timer_alloc(
        flipper_rng_splash_timer_callback,
        FuriTimerTypePeriodic,
        splash
    );
    
    return splash;
}

void flipper_rng_splash_free(FlipperRngSplash* splash) {
    furi_timer_free(splash->timer);
    view_free(splash->view);
    free(splash);
}

View* flipper_rng_splash_get_view(FlipperRngSplash* splash) {
    return splash->view;
}

void flipper_rng_splash_start(FlipperRngSplash* splash) {
    furi_timer_start(splash->timer, ANIMATION_TICK_MS);
}

void flipper_rng_splash_stop(FlipperRngSplash* splash) {
    furi_timer_stop(splash->timer);
}

bool flipper_rng_splash_is_done(FlipperRngSplash* splash) {
    bool done = false;
    with_view_model(
        splash->view,
        FlipperRngSplashModel* model,
        {
            done = model->animation_done;
        },
        false
    );
    return done;
}

#pragma once

#include <gui/view.h>

typedef struct FlipperRngSplash FlipperRngSplash;

FlipperRngSplash* flipper_rng_splash_alloc(void);
void flipper_rng_splash_free(FlipperRngSplash* splash);
View* flipper_rng_splash_get_view(FlipperRngSplash* splash);
void flipper_rng_splash_start(FlipperRngSplash* splash);
void flipper_rng_splash_stop(FlipperRngSplash* splash);
bool flipper_rng_splash_is_done(FlipperRngSplash* splash);

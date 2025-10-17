#include "entropylab.h"
#include "entropylab_views.h"
#include "entropylab_passphrase.h"
#include "entropylab_passphrase_sd.h"
#include "entropylab_entropy.h"
#include "entropylab_secure.h"
#include "entropylab_log.h"
#include <gui/elements.h>
#include <string.h>
#include <furi.h>

#define TAG "EntropyLab-PassphraseView"

// Cooldown period between passphrase generations (milliseconds)
// Ensures entropy pool has time to refresh and prevents rapid-fire attacks
#define PASSPHRASE_GENERATION_COOLDOWN_MS 100

// Forward declarations for entropy worker management
static void flipper_rng_passphrase_start_entropy_worker(FlipperRngApp* app);
static void flipper_rng_passphrase_stop_entropy_worker(FlipperRngApp* app);

// Worker thread context for async index building
typedef struct {
    FlipperRngApp* app;
    PassphraseSDContext* sd_context;
} IndexBuildWorkerContext;

// Passphrase Generator view model
typedef struct {
    char passphrase[512];  // Current passphrase - increased for up to 12 words
    uint8_t num_words;     // Number of words to generate
    float entropy_bits;    // Entropy bits for current configuration
    bool is_generating;    // Whether we're currently generating
    uint32_t generation_count;  // How many passphrases we've generated
    PassphraseListType list_type;  // Which wordlist to use
    bool sd_available;     // Whether SD card wordlists are available
    PassphraseSDContext* sd_context;  // SD card context
    bool is_loading;       // Whether we're loading a wordlist
    float load_progress;   // Loading progress (0.0 to 1.0)
    char load_status[64];  // Loading status message
    bool started_worker;   // Whether this view started the entropy worker
    FuriThread* index_worker_thread;  // Handle to index building thread (for cleanup)
} FlipperRngPassphraseModel;

// Progress callback for index building
static void index_build_progress_callback(float progress, void* context) {
    FlipperRngApp* app = (FlipperRngApp*)context;
    
    with_view_model(
        app->diceware_view,
        FlipperRngPassphraseModel* model,
        {
            model->load_progress = progress;
            snprintf(model->load_status, sizeof(model->load_status), "%.0f%% complete", (double)(progress * 100.0f));
        },
        true
    );
}

// Worker thread function for building index
static int32_t index_build_worker(void* context) {
    IndexBuildWorkerContext* worker_ctx = (IndexBuildWorkerContext*)context;
    FlipperRngApp* app = worker_ctx->app;
    PassphraseSDContext* sd_context = worker_ctx->sd_context;
    
    LOG_D(TAG, "Starting async index building...");
    
    // Build the index with progress updates
    bool success = flipper_rng_passphrase_sd_build_index(sd_context, index_build_progress_callback, app);
    
    // Update the model when done
    with_view_model(
        app->diceware_view,
        FlipperRngPassphraseModel* model,
        {
            model->is_loading = false;
            if(success) {
                snprintf(model->load_status, sizeof(model->load_status), "Ready! %d words indexed", sd_context->word_count);
                LOG_D(TAG, "Index built successfully");
            } else {
                snprintf(model->load_status, sizeof(model->load_status), "Index failed - using fallback");
                LOG_W(TAG, "Index building failed, will use slower access");
            }
        },
        true
    );
    
    // Now that wordlist is ready, start background entropy collection
    LOG_D(TAG, "Wordlist ready, starting background entropy collection...");
    flipper_rng_passphrase_start_entropy_worker(app);
    
    // Free the worker context
    free(worker_ctx);
    LOG_D(TAG, "Index build worker context freed");
    
    return 0;
}

// Start entropy worker for continuous background entropy collection
static void flipper_rng_passphrase_start_entropy_worker(FlipperRngApp* app) {
    // Only start if not already running
    if(app->state->is_running) {
        LOG_D(TAG, "Entropy worker already running (started elsewhere)");
        // Mark that we didn't start it
        with_view_model(
            app->diceware_view,
            FlipperRngPassphraseModel* model,
            { model->started_worker = false; },
            false
        );
        return;
    }
    
    LOG_D(TAG, "Starting background entropy collection for passphrase generation...");
    
    // Set LED to blinking green during entropy collection
    flipper_rng_set_led_generating(app);
    
    // Ensure worker thread is stopped before starting
    if(furi_thread_get_state(app->worker_thread) != FuriThreadStateStopped) {
        LOG_D(TAG, "Waiting for previous worker to stop...");
        app->state->is_running = false;
        furi_thread_join(app->worker_thread);
    }
    
    // Reset counters for fresh start
    app->state->bytes_generated = 0;
    app->state->samples_collected = 0;
    app->state->bits_from_hw_rng = 0;
    app->state->bits_from_subghz_rssi = 0;
    app->state->bits_from_infrared = 0;
    memset(app->state->byte_histogram, 0, sizeof(app->state->byte_histogram));
    
    // Start the worker thread for background entropy collection
    app->state->is_running = true;
    furi_thread_start(app->worker_thread);
    
    // Mark that WE started the worker
    with_view_model(
        app->diceware_view,
        FlipperRngPassphraseModel* model,
        { model->started_worker = true; },
        false
    );
    
    // Start IR worker if IR entropy is enabled
    // This is handled by the main app, but we need to access the IR worker functions
    // For now, let's just log that we'd start it
    LOG_D(TAG, "Background entropy worker started for passphrase generation");
}

// Stop entropy worker when leaving passphrase view
static void flipper_rng_passphrase_stop_entropy_worker(FlipperRngApp* app) {
    // Check if we started the worker
    bool should_stop = false;
    with_view_model(
        app->diceware_view,
        FlipperRngPassphraseModel* model,
        { should_stop = model->started_worker; },
        false
    );
    
    if(!should_stop) {
        LOG_D(TAG, "Entropy worker was started elsewhere, not stopping it");
        return;
    }
    
    if(!app->state->is_running) {
        LOG_D(TAG, "Entropy worker not running");
        return;
    }
    
    LOG_D(TAG, "Stopping background entropy collection (we started it)...");
    app->state->is_running = false;
    
    // Set LED back to red when stopping entropy collection
    flipper_rng_set_led_stopped(app);
    
    // Don't block GUI thread with join - let worker exit on its own
    // The worker will exit cleanly when is_running becomes false
    LOG_D(TAG, "Background entropy worker stop requested");
}

// Draw callback for diceware view
void flipper_rng_passphrase_draw_callback(Canvas* canvas, void* context) {
    FlipperRngPassphraseModel* model = context;
    
    canvas_clear(canvas);
    
    // Show loading screen if loading
    if(model->is_loading) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 10, AlignCenter, AlignTop, "Loading Wordlist");
        
        // Draw progress bar
        int bar_width = 100;
        int bar_height = 8;
        int bar_x = (128 - bar_width) / 2;
        int bar_y = 28;
        
        // Draw progress bar outline
        canvas_draw_frame(canvas, bar_x, bar_y, bar_width, bar_height);
        
        // Draw progress bar fill
        int fill_width = (int)(bar_width * model->load_progress);
        if(fill_width > 0) {
            canvas_draw_box(canvas, bar_x, bar_y, fill_width, bar_height);
        }
        
        // Draw status message
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 45, AlignCenter, AlignTop, model->load_status);
        
        return;
    }
    
    // Title
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 1, AlignCenter, AlignTop, "Entropy Lab - Passphrase");
    
    // Check if SD wordlist is available
    if(!model->sd_available || !model->sd_context || !model->sd_context->is_loaded) {
        // Show error message if no wordlist
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignTop, "No wordlist found!");
        canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignTop, "Wordlists should be");
        canvas_draw_str_aligned(canvas, 64, 42, AlignCenter, AlignTop, "bundled with the app");
        canvas_draw_str_aligned(canvas, 64, 52, AlignCenter, AlignTop, "in /apps_data/entropylab/");
        return;
    }
    
    // Draw wordlist info
    canvas_set_font(canvas, FontSecondary);
    char info_str[64];
    uint16_t list_size = model->sd_context->word_count;
    
    snprintf(info_str, sizeof(info_str), "%d list | %d words | %.0f bits", 
             list_size, model->num_words, (double)model->entropy_bits);
    canvas_draw_str_aligned(canvas, 64, 12, AlignCenter, AlignTop, info_str);
    
    // Draw a separator line
    canvas_draw_line(canvas, 0, 20, 127, 20);
    
    // Draw the passphrase with adaptive layout
    if(strlen(model->passphrase) > 0) {
        const char* phrase = model->passphrase;
        int phrase_len = strlen(phrase);
        int chars_processed = 0;
        
        // Determine layout based on passphrase length
        int max_lines, line_height, start_y, chars_per_line;
        
        if(model->num_words <= 6) {
            // Short passphrases: larger font, fewer lines
            canvas_set_font(canvas, FontSecondary);
            max_lines = 3;
            line_height = 9;
            start_y = 30;
            chars_per_line = 25;
        } else if(model->num_words <= 9) {
            // Medium passphrases: normal font, more lines
            canvas_set_font(canvas, FontSecondary);
            max_lines = 4;
            line_height = 8;
            start_y = 28;
            chars_per_line = 25;
        } else {
            // Long passphrases: smaller font, tight spacing
            canvas_set_font(canvas, FontSecondary);
            max_lines = 6;
            line_height = 7;
            start_y = 24;
            chars_per_line = 28;
        }
        
        int y_pos = start_y;
        
        // Display lines with word-boundary wrapping
        for(int line_num = 0; line_num < max_lines && chars_processed < phrase_len && y_pos < 64; line_num++) {
            int line_start = chars_processed;
            int line_end = line_start;
            int last_space = -1;
            
            // Find the best break point within character limit
            while(line_end < phrase_len && (line_end - line_start) < chars_per_line) {
                if(phrase[line_end] == ' ') {
                    last_space = line_end;
                }
                line_end++;
            }
            
            // Break at word boundary if possible
            if(last_space > line_start && line_end < phrase_len) {
                line_end = last_space;
            }
            
            // Extract and draw this line
            int line_len = line_end - line_start;
            if(line_len > 0) {
                char line[32];
                if(line_len > 31) line_len = 31;
                strncpy(line, phrase + line_start, line_len);
                line[line_len] = '\0';
                
                canvas_draw_str_aligned(canvas, 64, y_pos, AlignCenter, AlignTop, line);
                y_pos += line_height;
            } else {
                // Safety: if no progress made, advance at least 1 char to prevent infinite loop
                line_end = line_start + 1;
            }
            
            chars_processed = line_end;
            // Skip the space if we broke at one
            if(chars_processed < phrase_len && phrase[chars_processed] == ' ') {
                chars_processed++;
            }
        }
        
        // If there's still text that didn't fit, show truncation indicator
        if(chars_processed < phrase_len && y_pos >= 64) {
            canvas_draw_str_aligned(canvas, 64, 56, AlignCenter, AlignTop, "...");
        }
    } else {
        canvas_draw_str_aligned(canvas, 64, 38, AlignCenter, AlignTop, "Press OK to generate");
    }
}

// Input callback for diceware view
bool flipper_rng_passphrase_input_callback(InputEvent* event, void* context) {
    FlipperRngApp* app = context;
    bool consumed = false;
    
    if(event->type == InputTypePress) {
        consumed = true;
        
        switch(event->key) {
            case InputKeyOk:
                // Generate new passphrase (SD card only)
                with_view_model(
                    app->diceware_view,
                    FlipperRngPassphraseModel* model,
                    {
                        // Check if entropy is ready (minimum collection time elapsed)
                        if(!app->state->entropy_ready) {
                            // Show warning - not enough entropy collected yet
                            LOG_W(TAG, "Entropy not ready yet, please wait for minimum collection time");
                            snprintf(model->passphrase, sizeof(model->passphrase), 
                                    "Please wait...\nCollecting entropy");
                            break;
                        }
                        
                        // Only generate if wordlist is loaded and not currently loading/indexing/generating
                        if(model->sd_context && model->sd_context->is_loaded && !model->is_loading && !model->is_generating) {
                            model->is_generating = true;
                            
                            // Securely wipe old passphrase before generating new one
                            secure_wipe(model->passphrase, sizeof(model->passphrase));
                            
                            // Entropy worker should already be running in background
                            LOG_D(TAG, "Generating passphrase with continuously refreshed entropy pool");
                            
                            flipper_rng_passphrase_generate_sd(
                                app->state,
                                model->sd_context,
                                model->passphrase,
                                sizeof(model->passphrase),
                                model->num_words
                            );
                            
                            model->generation_count++;
                            model->is_generating = false;
                        }
                    },
                    true
                );
                break;
                
            case InputKeyLeft:
                // Decrease word count
                with_view_model(
                    app->diceware_view,
                    FlipperRngPassphraseModel* model,
                    {
                        if(model->num_words > PASSPHRASE_MIN_WORDS) {
                            model->num_words--;
                            model->entropy_bits = flipper_rng_passphrase_entropy_bits(model->num_words);
                            // Clear passphrase when changing word count
                            memset(model->passphrase, 0, sizeof(model->passphrase));
                        }
                    },
                    true
                );
                break;
                
            case InputKeyRight:
                // Increase word count
                with_view_model(
                    app->diceware_view,
                    FlipperRngPassphraseModel* model,
                    {
                        if(model->num_words < PASSPHRASE_MAX_WORDS) {
                            model->num_words++;
                            model->entropy_bits = flipper_rng_passphrase_entropy_bits(model->num_words);
                            // Clear passphrase when changing word count
                            memset(model->passphrase, 0, sizeof(model->passphrase));
                        }
                    },
                    true
                );
                break;
                
            // Remove Up/Down key handling - no wordlist switching
                
            case InputKeyBack:
                // Return to menu
                view_dispatcher_switch_to_view(app->view_dispatcher, FlipperRngViewMenu);
                break;
                
            default:
                consumed = false;
                break;
        }
    }
    
    return consumed;
}

// Enter callback - called when view is shown
void flipper_rng_passphrase_enter_callback(void* context) {
    FlipperRngApp* app = context;
    
    with_view_model(
        app->diceware_view,
        FlipperRngPassphraseModel* model,
        {
            // Check if we need to load/index the wordlist
            bool needs_loading = !model->sd_context->is_loaded;
            bool needs_indexing = !flipper_rng_passphrase_sd_is_indexed(model->sd_context);
            
            if(model->sd_context && (needs_loading || needs_indexing)) {
                // Check if wordlist exists
                if(flipper_rng_passphrase_sd_exists(model->sd_context, PassphraseListEFFLong)) {
                    // Load/reload the file if needed
                    if(needs_loading) {
                        LOG_D(TAG, "Wordlist not loaded, attempting to load...");
                        if(!flipper_rng_passphrase_sd_load(model->sd_context, PassphraseListEFFLong)) {
                            LOG_E(TAG, "Failed to load wordlist file!");
                            return;
                        }
                        LOG_D(TAG, "Wordlist file opened successfully");
                        model->sd_available = true;
                        model->list_type = PassphraseListEFFLong;
                        model->entropy_bits = flipper_rng_passphrase_sd_entropy_bits(model->list_type, model->num_words);
                    }
                    
                    // Build index if needed
                    if(needs_indexing) {
                        LOG_D(TAG, "Index not built, starting async build...");
                        model->is_loading = true;
                        model->load_progress = 0.0f;
                        snprintf(model->load_status, sizeof(model->load_status), "Preparing wordlist...");
                        
                        // Create worker context
                        IndexBuildWorkerContext* worker_ctx = malloc(sizeof(IndexBuildWorkerContext));
                        worker_ctx->app = app;
                        worker_ctx->sd_context = model->sd_context;
                        
                        // Start worker thread
                        model->index_worker_thread = furi_thread_alloc_ex(
                            "IndexBuilder",
                            2048,  // Stack size
                            index_build_worker,
                            worker_ctx
                        );
                        
                        furi_thread_start(model->index_worker_thread);
                        
                        LOG_D(TAG, "Started async index building thread");
                    } else {
                        LOG_D(TAG, "Index already built, ready to generate");
                    }
                } else {
                    LOG_E(TAG, "Wordlist file does not exist!");
                }
            } else if(model->sd_context && model->sd_context->is_loaded && flipper_rng_passphrase_sd_is_indexed(model->sd_context)) {
                LOG_D(TAG, "Wordlist already loaded and indexed");
            }
        },
        true
    );
    
    // Note: Entropy worker will be started automatically after wordlist loading completes
    // If wordlist is already loaded and indexed, start entropy worker now
    with_view_model(
        app->diceware_view,
        FlipperRngPassphraseModel* model,
        {
            if(model->sd_context && model->sd_context->is_loaded && 
               flipper_rng_passphrase_sd_is_indexed(model->sd_context) && 
               !model->is_loading) {
                LOG_D(TAG, "Wordlist already ready, starting background entropy collection...");
                flipper_rng_passphrase_start_entropy_worker(app);
            }
        },
        true
    );
}

// Exit callback - called when leaving the view
void flipper_rng_passphrase_exit_callback(void* context) {
    FlipperRngApp* app = context;
    
    LOG_D(TAG, "Exiting passphrase generator, stopping background entropy collection");
    
    // Securely wipe passphrase from memory when leaving view
    with_view_model(
        app->diceware_view,
        FlipperRngPassphraseModel* model,
        {
            secure_wipe(model->passphrase, sizeof(model->passphrase));
        },
        false
    );
    
    // Stop background entropy collection when leaving passphrase generator
    flipper_rng_passphrase_stop_entropy_worker(app);
}

// Allocate diceware view
View* flipper_rng_passphrase_view_alloc(FlipperRngApp* app) {
    View* view = view_alloc();
    
    view_allocate_model(view, ViewModelTypeLocking, sizeof(FlipperRngPassphraseModel));
    view_set_context(view, app);
    view_set_draw_callback(view, flipper_rng_passphrase_draw_callback);
    view_set_input_callback(view, flipper_rng_passphrase_input_callback);
    view_set_enter_callback(view, flipper_rng_passphrase_enter_callback);
    view_set_exit_callback(view, flipper_rng_passphrase_exit_callback);
    
    // Initialize model
    with_view_model(
        view,
        FlipperRngPassphraseModel* model,
        {
            memset(model->passphrase, 0, sizeof(model->passphrase));
            model->num_words = PASSPHRASE_DEFAULT_WORDS;
            model->entropy_bits = flipper_rng_passphrase_entropy_bits(model->num_words);
            model->is_generating = false;
            model->generation_count = 0;
            model->list_type = PassphraseListEFFLong;
            
            // Initialize SD context but DON'T load wordlist yet
            model->sd_context = flipper_rng_passphrase_sd_alloc();
            model->sd_available = false;
            model->is_loading = false;
            model->index_worker_thread = NULL;
        },
        true
    );
    
    return view;
}

// Free diceware view
void flipper_rng_passphrase_view_free(View* view) {
    // Securely wipe and free SD context
    with_view_model(
        view,
        FlipperRngPassphraseModel* model,
        {
            // Wait for index worker thread to finish if it's still running
            if(model->index_worker_thread) {
                FuriThreadState state = furi_thread_get_state(model->index_worker_thread);
                if(state != FuriThreadStateStopped) {
                    LOG_D(TAG, "Waiting for index worker thread to finish...");
                    furi_thread_join(model->index_worker_thread);
                }
                furi_thread_free(model->index_worker_thread);
                model->index_worker_thread = NULL;
                LOG_D(TAG, "Index worker thread cleaned up");
            }
            
            // Securely wipe passphrase before freeing
            secure_wipe(model->passphrase, sizeof(model->passphrase));
            
            if(model->sd_context) {
                // Wipe current_word in SD context
                secure_wipe(model->sd_context->current_word, sizeof(model->sd_context->current_word));
                flipper_rng_passphrase_sd_free(model->sd_context);
                model->sd_context = NULL;
            }
        },
        false
    );
    
    view_free(view);
}

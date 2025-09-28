#include "flipper_rng.h"
#include "flipper_rng_views.h"
#include "flipper_rng_passphrase.h"
#include "flipper_rng_passphrase_sd.h"
#include <gui/elements.h>
#include <string.h>
#include <furi.h>

#define TAG "FlipperRNG-PassphraseView"

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
} FlipperRngPassphraseModel;

// Progress callback for index building
static void index_build_progress_callback(float progress, void* context) {
    FlipperRngApp* app = (FlipperRngApp*)context;
    
    with_view_model(
        app->diceware_view,
        FlipperRngPassphraseModel* model,
        {
            model->load_progress = progress;
            snprintf(model->load_status, sizeof(model->load_status), "Building index... %.0f%%", (double)(progress * 100.0f));
        },
        true
    );
}

// Worker thread function for building index
static int32_t index_build_worker(void* context) {
    IndexBuildWorkerContext* worker_ctx = (IndexBuildWorkerContext*)context;
    FlipperRngApp* app = worker_ctx->app;
    PassphraseSDContext* sd_context = worker_ctx->sd_context;
    
    FURI_LOG_I(TAG, "Starting async index building...");
    
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
                FURI_LOG_I(TAG, "Index built successfully");
            } else {
                snprintf(model->load_status, sizeof(model->load_status), "Index failed - using fallback");
                FURI_LOG_W(TAG, "Index building failed, will use slower access");
            }
        },
        true
    );
    
    return 0;
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
    canvas_draw_str_aligned(canvas, 64, 1, AlignCenter, AlignTop, "Passphrase Generator");
    
    // Check if SD wordlist is available
    if(!model->sd_available || !model->sd_context || !model->sd_context->is_loaded) {
        // Show error message if no wordlist
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignTop, "No wordlist found!");
        canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignTop, "Please place");
        canvas_draw_str_aligned(canvas, 64, 42, AlignCenter, AlignTop, "eff_large_wordlist.txt");
        canvas_draw_str_aligned(canvas, 64, 52, AlignCenter, AlignTop, "in /apps/Tools/FlipperRNG/");
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
            chars_per_line = 28;  // Fit more chars per line
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
    
    if(event->type == InputTypePress || event->type == InputTypeRepeat) {
        consumed = true;
        
        switch(event->key) {
            case InputKeyOk:
                // Generate new passphrase (SD card only)
                with_view_model(
                    app->diceware_view,
                    FlipperRngPassphraseModel* model,
                    {
                        // Only generate if wordlist is loaded and not currently loading/indexing
                        if(model->sd_context && model->sd_context->is_loaded && !model->is_loading) {
                            model->is_generating = true;
                            
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
            // Try to load wordlist if not already loaded
            if(model->sd_context && !model->sd_context->is_loaded) {
                // Check if wordlist exists
                if(flipper_rng_passphrase_sd_exists(model->sd_context, PassphraseListEFFLong)) {
                    // Load it (just opens the file)
                    if(flipper_rng_passphrase_sd_load(model->sd_context, PassphraseListEFFLong)) {
                        model->sd_available = true;
                        model->list_type = PassphraseListEFFLong;
                        model->entropy_bits = flipper_rng_passphrase_sd_entropy_bits(model->list_type, model->num_words);
                        
                        // Start async index building if not already indexed
                        if(!flipper_rng_passphrase_sd_is_indexed(model->sd_context)) {
                            model->is_loading = true;
                            model->load_progress = 0.0f;
                            snprintf(model->load_status, sizeof(model->load_status), "Preparing wordlist...");
                            
                            // Create worker context
                            IndexBuildWorkerContext* worker_ctx = malloc(sizeof(IndexBuildWorkerContext));
                            worker_ctx->app = app;
                            worker_ctx->sd_context = model->sd_context;
                            
                            // Start worker thread
                            FuriThread* worker_thread = furi_thread_alloc_ex(
                                "IndexBuilder",
                                2048,  // Stack size
                                index_build_worker,
                                worker_ctx
                            );
                            furi_thread_start(worker_thread);
                            
                            FURI_LOG_I(TAG, "Started async index building thread");
                        }
                    }
                }
            }
        },
        true
    );
}

// Allocate diceware view
View* flipper_rng_passphrase_view_alloc(FlipperRngApp* app) {
    View* view = view_alloc();
    
    view_allocate_model(view, ViewModelTypeLocking, sizeof(FlipperRngPassphraseModel));
    view_set_context(view, app);
    view_set_draw_callback(view, flipper_rng_passphrase_draw_callback);
    view_set_input_callback(view, flipper_rng_passphrase_input_callback);
    view_set_enter_callback(view, flipper_rng_passphrase_enter_callback);
    
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
            model->list_type = PassphraseListEmbedded;
            
            // Initialize SD context but DON'T load wordlist yet
            model->sd_context = flipper_rng_passphrase_sd_alloc();
            model->sd_available = false;
            model->is_loading = false;
        },
        true
    );
    
    return view;
}

// Free diceware view
void flipper_rng_passphrase_view_free(View* view) {
    // Free SD context
    with_view_model(
        view,
        FlipperRngPassphraseModel* model,
        {
            if(model->sd_context) {
                flipper_rng_passphrase_sd_free(model->sd_context);
                model->sd_context = NULL;
            }
        },
        false
    );
    
    view_free(view);
}

#include "flipper_rng.h"
#include "flipper_rng_entropy.h"
#include "flipper_rng_views.h"
#include <furi_hal_random.h>
#include <furi_hal_adc.h>
#include <furi_hal_power.h>
#include <furi_hal_serial.h>
#include <furi_hal_usb_cdc.h>
#include <power/power_service/power.h>
#include <toolbox/stream/stream.h>
#include <toolbox/stream/file_stream.h>

#define TAG "FlipperRNG"

// LED status control functions
static void flipper_rng_set_led_stopped(FlipperRngApp* app) {
    // Stop blinking first, then set solid red
    notification_message(app->notifications, &sequence_blink_stop);
    notification_message(app->notifications, &sequence_set_only_red_255);
    FURI_LOG_I(TAG, "LED set to SOLID RED (stopped)");
}

static void flipper_rng_set_led_generating(FlipperRngApp* app) {
    // Use predefined blinking green sequence
    notification_message(app->notifications, &sequence_blink_start_green);
    FURI_LOG_I(TAG, "LED set to BLINKING GREEN (generating)");
}

static void flipper_rng_set_led_off(FlipperRngApp* app) {
    // Use predefined blink stop sequence
    notification_message(app->notifications, &sequence_blink_stop);
    FURI_LOG_I(TAG, "LED turned OFF");
}


// Menu items
typedef enum {
    FlipperRngMenuStart,
    FlipperRngMenuStop,
    FlipperRngMenuConfig,
    FlipperRngMenuVisualization,
    FlipperRngMenuStats,
    FlipperRngMenuAbout,
    FlipperRngMenuQuit,
} FlipperRngMenuItem;

static void flipper_rng_menu_callback(void* context, uint32_t index) {
    FlipperRngApp* app = context;
    
    switch(index) {
    case FlipperRngMenuStart:
        if(!app->state->is_running) {
            FURI_LOG_I(TAG, "Starting worker thread...");
            
            // Make sure thread is not running
            if(furi_thread_get_state(app->worker_thread) != FuriThreadStateStopped) {
                FURI_LOG_W(TAG, "Worker thread still running, waiting...");
                furi_thread_join(app->worker_thread);
            }
            
            app->state->is_running = true;
            furi_thread_start(app->worker_thread);
            flipper_rng_set_led_generating(app);  // Set LED to green
            FURI_LOG_I(TAG, "Worker thread started from menu");
        }
        break;
        
    case FlipperRngMenuStop:
        if(app->state->is_running) {
            FURI_LOG_I(TAG, "Stopping worker thread...");
            app->state->is_running = false;
            flipper_rng_set_led_stopped(app);  // Set LED to red
            // Don't block GUI thread with join - let worker exit on its own
        }
        break;
        
    case FlipperRngMenuConfig:
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipperRngViewConfig);
        break;
        
    case FlipperRngMenuVisualization:
        FURI_LOG_I(TAG, "Switching to visualization view");
        // Initialize visualization model with current state
        with_view_model(
            app->visualization_view,
            FlipperRngVisualizationModel* model,
            {
                model->is_running = app->state->is_running;
                model->entropy_quality = app->state->entropy_quality;
                model->bytes_generated = app->state->bytes_generated;
                model->viz_mode = 0;  // Start with mode 0
                model->walk_x = 64;   // Center X
                model->walk_y = 32;   // Center Y
                // Initialize with some random data if running
                if(app->state->is_running) {
                    for(int i = 0; i < 128; i += 4) {
                        uint32_t rand_val = furi_hal_random_get();
                        model->random_data[i] = (rand_val >> 0) & 0xFF;
                        if(i + 1 < 128) model->random_data[i + 1] = (rand_val >> 8) & 0xFF;
                        if(i + 2 < 128) model->random_data[i + 2] = (rand_val >> 16) & 0xFF;
                        if(i + 3 < 128) model->random_data[i + 3] = (rand_val >> 24) & 0xFF;
                    }
                }
            },
            true
        );
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipperRngViewVisualization);
        break;
        
    case FlipperRngMenuStats:
        {
            furi_string_reset(app->text_box_store);
            furi_string_printf(
                app->text_box_store,
                "FlipperRNG Statistics\n\n"
                "Bytes Generated: %lu\n"
                "Samples Collected: %lu\n"
                "Entropy Quality: %.2f%%\n"
                "Pool Position: %zu/%d\n"
                "Active Sources: %d\n",
                app->state->bytes_generated,
                app->state->samples_collected,
                (double)(app->state->entropy_quality * 100.0f),
                app->state->entropy_pool_pos,
                RNG_POOL_SIZE,
                __builtin_popcount(app->state->entropy_sources)
            );
            text_box_set_text(app->text_box, furi_string_get_cstr(app->text_box_store));
            view_dispatcher_switch_to_view(app->view_dispatcher, FlipperRngViewOutput);
        }
        break;
        
    case FlipperRngMenuAbout:
        furi_string_reset(app->text_box_store);
        furi_string_printf(
            app->text_box_store,
            "FlipperRNG v%s\n\n"
            "High-quality entropy\n"
            "generator using multiple\n"
            "hardware sources.\n\n"
            "Sources:\n"
            "- Hardware RNG\n"
            "- ADC noise\n"
            "- Timing jitter\n"
            "- Button timing\n"
            "- CPU jitter\n"
            "- Battery voltage\n"
            "- Temperature\n",
            FLIPPER_RNG_VERSION
        );
        text_box_set_text(app->text_box, furi_string_get_cstr(app->text_box_store));
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipperRngViewOutput);
        break;
        
    case FlipperRngMenuQuit:
        FURI_LOG_I(TAG, "Quit selected, stopping view dispatcher");
        // Stop worker if running
        if(app->state->is_running) {
            app->state->is_running = false;
            // Give worker time to stop
            furi_delay_ms(50);
        }
        // Switch to a special "exit" view which will cause the dispatcher to stop
        view_dispatcher_switch_to_view(app->view_dispatcher, VIEW_NONE);
        break;
    }
}

static uint32_t flipper_rng_exit_callback(void* context) {
    UNUSED(context);
    FURI_LOG_I(TAG, "Exit callback triggered");
    return VIEW_NONE;
}

static uint32_t flipper_rng_back_callback(void* context) {
    UNUSED(context);
    FURI_LOG_I(TAG, "Back callback triggered, returning to menu");
    return FlipperRngViewMenu;
}

FlipperRngApp* flipper_rng_app_alloc(void) {
    FURI_LOG_I(TAG, "Allocating FlipperRNG app...");
    FlipperRngApp* app = malloc(sizeof(FlipperRngApp));
    if(!app) {
        FURI_LOG_E(TAG, "Failed to malloc app");
        return NULL;
    }
    
    // Initialize state
    FURI_LOG_I(TAG, "Allocating state...");
    app->state = malloc(sizeof(FlipperRngState));
    if(!app->state) {
        FURI_LOG_E(TAG, "Failed to malloc state");
        free(app);
        return NULL;
    }
    app->state->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->state->entropy_sources = EntropySourceAll;
    app->state->output_mode = OutputModeUSB;
    app->state->poll_interval_ms = 10;
    app->state->is_running = false;
    app->state->entropy_pool_pos = 0;
    app->state->bytes_generated = 0;
    app->state->samples_collected = 0;
    app->state->entropy_quality = 0.0f;
    app->state->adc_handle = NULL;
    app->state->serial_handle = NULL;
    
    // Clear entropy pool with initial random data
    furi_hal_random_fill_buf(app->state->entropy_pool, RNG_POOL_SIZE);
    
    // Initialize GUI
    FURI_LOG_I(TAG, "Opening GUI record...");
    app->gui = furi_record_open(RECORD_GUI);
    if(!app->gui) {
        FURI_LOG_E(TAG, "Failed to open GUI record");
        free(app->state);
        free(app);
        return NULL;
    }
    
    FURI_LOG_I(TAG, "Opening notification record...");
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    
    // View dispatcher
    FURI_LOG_I(TAG, "Allocating view dispatcher...");
    app->view_dispatcher = view_dispatcher_alloc();
    if(!app->view_dispatcher) {
        FURI_LOG_E(TAG, "Failed to allocate view dispatcher");
        furi_record_close(RECORD_GUI);
        furi_record_close(RECORD_NOTIFICATION);
        free(app->state);
        free(app);
        return NULL;
    }
    // view_dispatcher_enable_queue is deprecated, not needed for newer SDK
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    
    // Main menu
    app->submenu = submenu_alloc();
    submenu_set_header(app->submenu, "FlipperRNG v1.0");
    submenu_add_item(app->submenu, "Start Generator", FlipperRngMenuStart, flipper_rng_menu_callback, app);
    submenu_add_item(app->submenu, "Stop Generator", FlipperRngMenuStop, flipper_rng_menu_callback, app);
    submenu_add_item(app->submenu, "Configuration", FlipperRngMenuConfig, flipper_rng_menu_callback, app);
    submenu_add_item(app->submenu, "Visualization", FlipperRngMenuVisualization, flipper_rng_menu_callback, app);
    submenu_add_item(app->submenu, "Statistics", FlipperRngMenuStats, flipper_rng_menu_callback, app);
    submenu_add_item(app->submenu, "About", FlipperRngMenuAbout, flipper_rng_menu_callback, app);
    submenu_add_item(app->submenu, "Quit", FlipperRngMenuQuit, flipper_rng_menu_callback, app);
    
    View* submenu_view = submenu_get_view(app->submenu);
    // Set exit callback for back button on main menu
    view_set_previous_callback(submenu_view, flipper_rng_exit_callback);
    view_dispatcher_add_view(app->view_dispatcher, FlipperRngViewMenu, submenu_view);
    
    // Configuration view
    app->variable_item_list = variable_item_list_alloc();
    flipper_rng_setup_config_view(app);
    View* variable_item_list_view = variable_item_list_get_view(app->variable_item_list);
    view_set_previous_callback(variable_item_list_view, flipper_rng_back_callback);
    view_dispatcher_add_view(app->view_dispatcher, FlipperRngViewConfig, variable_item_list_view);
    
    // Text box for output
    app->text_box = text_box_alloc();
    app->text_box_store = furi_string_alloc();
    text_box_set_font(app->text_box, TextBoxFontText);
    View* text_box_view = text_box_get_view(app->text_box);
    view_set_previous_callback(text_box_view, flipper_rng_back_callback);
    view_dispatcher_add_view(app->view_dispatcher, FlipperRngViewOutput, text_box_view);
    
    // Visualization view
    app->visualization_view = view_alloc();
    view_set_context(app->visualization_view, app);
    view_allocate_model(app->visualization_view, ViewModelTypeLocking, sizeof(FlipperRngVisualizationModel));
    view_set_draw_callback(app->visualization_view, flipper_rng_visualization_draw_callback);
    view_set_input_callback(app->visualization_view, flipper_rng_visualization_input_callback);
    view_set_previous_callback(app->visualization_view, flipper_rng_back_callback);
    view_dispatcher_add_view(app->view_dispatcher, FlipperRngViewVisualization, app->visualization_view);
    
    // Create worker thread
    app->worker_thread = furi_thread_alloc();
    furi_thread_set_name(app->worker_thread, "FlipperRngWorker");
    furi_thread_set_stack_size(app->worker_thread, 2048);
    furi_thread_set_callback(app->worker_thread, flipper_rng_worker_thread);
    furi_thread_set_context(app->worker_thread, app);
    
    // Switch to main menu
    FURI_LOG_I(TAG, "Switching to main menu view...");
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipperRngViewMenu);
    
    // Set initial LED status (red = stopped) - do this last when everything is ready
    FURI_LOG_I(TAG, "Setting initial LED state to RED (stopped)...");
    flipper_rng_set_led_stopped(app);
    
    FURI_LOG_I(TAG, "App allocation complete");
    return app;
}

void flipper_rng_app_free(FlipperRngApp* app) {
    furi_assert(app);
    
    // Stop worker if running
    if(app->state->is_running) {
        app->state->is_running = false;
        furi_thread_join(app->worker_thread);
    }
    
    // Turn off all LEDs before exiting
    flipper_rng_set_led_off(app);
    
    // Free worker thread
    furi_thread_free(app->worker_thread);
    
    // Free views
    view_dispatcher_remove_view(app->view_dispatcher, FlipperRngViewMenu);
    view_dispatcher_remove_view(app->view_dispatcher, FlipperRngViewConfig);
    view_dispatcher_remove_view(app->view_dispatcher, FlipperRngViewOutput);
    view_dispatcher_remove_view(app->view_dispatcher, FlipperRngViewVisualization);
    
    submenu_free(app->submenu);
    variable_item_list_free(app->variable_item_list);
    text_box_free(app->text_box);
    furi_string_free(app->text_box_store);
    view_free(app->visualization_view);
    
    view_dispatcher_free(app->view_dispatcher);
    
    // Close records
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    
    // Free hardware handles
    if(app->state->adc_handle) {
        furi_hal_adc_release(app->state->adc_handle);
    }
    if(app->state->serial_handle) {
        furi_hal_serial_control_release(app->state->serial_handle);
    }
    
    // Free state
    furi_mutex_free(app->state->mutex);
    free(app->state);
    free(app);
}

int32_t flipper_rng_app(void* p) {
    UNUSED(p);
    
    FURI_LOG_I(TAG, "FlipperRNG starting...");
    
    FlipperRngApp* app = flipper_rng_app_alloc();
    if(!app) {
        FURI_LOG_E(TAG, "Failed to allocate app");
        return -1;
    }
    
    FURI_LOG_I(TAG, "App allocated, starting view dispatcher");
    view_dispatcher_run(app->view_dispatcher);
    
    FURI_LOG_I(TAG, "View dispatcher exited, cleaning up");
    flipper_rng_app_free(app);
    
    FURI_LOG_I(TAG, "FlipperRNG exited cleanly");
    return 0;
}

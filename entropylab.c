#include "entropylab.h"
#include "entropylab_entropy.h"
#include "entropylab_views.h"
#include "entropylab_about.h"
#include "entropylab_donate.h"
#include "entropylab_splash.h"
#include "entropylab_hw_accel.h"
#include <furi_hal_random.h>
#include <furi_hal_adc.h>
#include <furi_hal_power.h>
#include <furi_hal_serial.h>
#include <furi_hal.h>
#include <furi_hal_light.h>
#include <power/power_service/power.h>
#include <toolbox/stream/stream.h>
#include <toolbox/stream/file_stream.h>
#include <infrared.h>

#define TAG "FlipperRNG"


// LED status control functions
void flipper_rng_set_led_stopped(FlipperRngApp* app) {
    // Stop blinking first, then set solid red
    notification_message(app->notifications, &sequence_blink_stop);
    notification_message(app->notifications, &sequence_set_only_red_255);
    FURI_LOG_I(TAG, "LED set to SOLID RED (stopped)");
}

void flipper_rng_set_led_generating(FlipperRngApp* app) {
    // Use predefined blinking green sequence
    notification_message(app->notifications, &sequence_blink_start_green);
    FURI_LOG_I(TAG, "LED set to BLINKING GREEN (generating)");
}

void flipper_rng_set_led_off(FlipperRngApp* app) {
    // Use predefined blink stop sequence
    notification_message(app->notifications, &sequence_blink_stop);
    FURI_LOG_I(TAG, "LED turned OFF");
}

// Start IR worker for entropy collection
static void flipper_rng_start_ir_worker(FlipperRngApp* app) {
    if(app->ir_worker) {
        FURI_LOG_W(TAG, "IR worker already running");
        return;
    }
    
    // Check if IR entropy is enabled
    if(!(app->state->entropy_sources & EntropySourceInfraredNoise)) {
        FURI_LOG_D(TAG, "IR entropy not enabled, skipping IR worker start");
        return;
    }
    
    FURI_LOG_I(TAG, "Starting IR worker for entropy collection...");
    app->ir_worker = infrared_worker_alloc();
    if(app->ir_worker) {
        // Configure for both decoded and raw signal capture
        infrared_worker_rx_enable_signal_decoding(app->ir_worker, true);
        infrared_worker_rx_enable_blink_on_receiving(app->ir_worker, false);
        
        // Set up callback - pass app state so we can access entropy pool
        infrared_worker_rx_set_received_signal_callback(
            app->ir_worker, flipper_rng_ir_callback, app->state);
        
        // Start IR reception
        infrared_worker_rx_start(app->ir_worker);
        FURI_LOG_I(TAG, "IR worker started for entropy collection");
    } else {
        FURI_LOG_E(TAG, "Failed to allocate IR worker");
    }
}

// Stop IR worker
static void flipper_rng_stop_ir_worker(FlipperRngApp* app) {
    if(!app->ir_worker) {
        return;
    }
    
    FURI_LOG_I(TAG, "Stopping IR worker...");
    infrared_worker_rx_stop(app->ir_worker);
    infrared_worker_free(app->ir_worker);
    app->ir_worker = NULL;
    FURI_LOG_I(TAG, "IR worker stopped");
}

// Persistent IR callback - runs continuously, accumulates entropy
void flipper_rng_ir_callback(void* ctx, InfraredWorkerSignal* signal) {
    FlipperRngState* state = (FlipperRngState*)ctx;
    if(!state) return;
    
    // Quick blue LED flash
    furi_hal_light_set(LightBlue, 100);
    
    const uint32_t* timings = NULL;
    size_t timings_cnt = 0;
    uint32_t local_entropy = DWT->CYCCNT;  // Start with CPU cycles
    
    // Check if signal is decoded or raw
    if(infrared_worker_signal_is_decoded(signal)) {
        const InfraredMessage* message = infrared_worker_get_decoded_signal(signal);
        if(message) {
            // Mix protocol data into entropy
            local_entropy ^= message->protocol;
            local_entropy ^= (message->address << 8);
            local_entropy ^= (message->command << 16);
            local_entropy ^= (message->repeat ? 0xAAAAAAAA : 0x55555555);
            
            FURI_LOG_D(TAG, "IR decoded: proto=%d, addr=0x%lX, cmd=0x%lX", 
                      message->protocol, message->address, message->command);
            
            // Add to entropy pool directly (8 bits of entropy)
            flipper_rng_add_entropy(state, local_entropy, 8);
            state->bits_from_infrared += 8;
        }
    } else {
        // Get raw signal timings
        infrared_worker_get_raw_signal(signal, &timings, &timings_cnt);
        
        if(timings_cnt > 0) {
            // Mix timing values into entropy
            for(size_t i = 0; i < timings_cnt && i < 32; i++) {
                local_entropy = (local_entropy << 3) ^ (local_entropy >> 29) ^ timings[i];
                local_entropy += i * 0x9E3779B9;  // Golden ratio for mixing
            }
            
            FURI_LOG_D(TAG, "IR raw: %zu samples", timings_cnt);
            
            // Add to entropy pool (more bits for raw signals)
            uint8_t entropy_bits = (timings_cnt > 16) ? 16 : 8;
            flipper_rng_add_entropy(state, local_entropy, entropy_bits);
            state->bits_from_infrared += entropy_bits;
        }
    }
    
    // Turn off blue LED
    furi_hal_light_set(LightBlue, 0);
}



// Menu items
typedef enum {
    FlipperRngMenuToggle,  // Combined Start/Stop
    FlipperRngMenuConfig,
    FlipperRngMenuVisualization,
    FlipperRngMenuByteDistribution,  // New: Byte Distribution
    FlipperRngMenuSourceStats,        // New: Source comparison
    FlipperRngMenuTest,
    FlipperRngMenuDiceware,           // New: Passphrase generator
    FlipperRngMenuAbout,
    FlipperRngMenuDonate,             // New: Donation QR code
} FlipperRngMenuItem;

// Forward declarations

void flipper_rng_menu_callback(void* context, uint32_t index) {
    FlipperRngApp* app = context;
    
    switch(index) {
    case FlipperRngMenuToggle:
        if(!app->state->is_running) {
            // Start the generator
            FURI_LOG_I(TAG, "Start Generator selected, is_running=%d", app->state->is_running);
            
            // ALWAYS stop and clean up first, regardless of is_running flag
            FURI_LOG_I(TAG, "Force stopping any existing worker thread...");
            app->state->is_running = false;
        
        
        // Wait for thread to actually stop if it's running
        if(furi_thread_get_state(app->worker_thread) != FuriThreadStateStopped) {
            FURI_LOG_I(TAG, "Waiting for worker thread to stop...");
            furi_thread_join(app->worker_thread);
            FURI_LOG_I(TAG, "Worker thread stopped");
        }
        
        // Now start with current settings
        FURI_LOG_I(TAG, "Starting worker thread with current settings...");
        
        if(app->state->output_mode == OutputModeUART) {
            FURI_LOG_I(TAG, "Initializing UART for output...");
            app->state->serial_handle = furi_hal_serial_control_acquire(FuriHalSerialIdUsart);
            if(app->state->serial_handle) {
                furi_hal_serial_init(app->state->serial_handle, 115200);
                FURI_LOG_I(TAG, "UART initialized at 115200 baud");
            } else {
                FURI_LOG_E(TAG, "Failed to acquire UART");
            }
        }
        
        // Make sure thread is not running
        if(furi_thread_get_state(app->worker_thread) != FuriThreadStateStopped) {
            FURI_LOG_W(TAG, "Worker thread still running, waiting...");
            furi_thread_join(app->worker_thread);
        }
        
        // Reset counters for fresh start
        app->state->bytes_generated = 0;
        app->state->samples_collected = 0;
        app->state->bits_from_hw_rng = 0;
        app->state->bits_from_subghz_rssi = 0;
        app->state->bits_from_infrared = 0;
        memset(app->state->byte_histogram, 0, sizeof(app->state->byte_histogram));
        
        // Start the worker thread
            app->state->is_running = true;
            furi_thread_start(app->worker_thread);
            flipper_rng_set_led_generating(app);  // Set LED to green
            
            // Start IR worker if IR entropy is enabled
            flipper_rng_start_ir_worker(app);
            
            // Update menu text to "Stop Generator"
            submenu_change_item_label(app->submenu, FlipperRngMenuToggle, "Stop Generator");
            
            FURI_LOG_I(TAG, "Worker thread started from menu, is_running=%d", app->state->is_running);
        } else {
            // Stop the generator
            FURI_LOG_I(TAG, "Stopping worker thread...");
            app->state->is_running = false;
            
            // Stop IR worker
            flipper_rng_stop_ir_worker(app);
            
            // Release UART if it was acquired
            if(app->state->serial_handle) {
                furi_hal_serial_deinit(app->state->serial_handle);
                furi_hal_serial_control_release(app->state->serial_handle);
                app->state->serial_handle = NULL;
                FURI_LOG_I(TAG, "UART released");
            }
            
            flipper_rng_set_led_stopped(app);  // Set LED to red
            
            // Update menu text back to "Start Generator"
            submenu_change_item_label(app->submenu, FlipperRngMenuToggle, "Start Generator");
            
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
        
    case FlipperRngMenuByteDistribution:
        FURI_LOG_I(TAG, "Switching to byte distribution view");
        // Initialize byte distribution model with current state
        with_view_model(
            app->byte_distribution_view,
            FlipperRngVisualizationModel* model,
            {
                model->is_running = app->state->is_running;
                model->bytes_generated = app->state->bytes_generated;
                model->viz_mode = 2;  // Histogram mode
                // Copy histogram data
                for(int i = 0; i < 16; i++) {
                    model->histogram[i] = app->state->byte_histogram[i];
                }
            },
            true
        );
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipperRngViewByteDistribution);
        break;
        
    case FlipperRngMenuSourceStats:
        FURI_LOG_I(TAG, "Source stats selected");
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipperRngViewSourceStats);
        break;
        
    case FlipperRngMenuTest:
        FURI_LOG_I(TAG, "Test RNG Quality selected");
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipperRngViewTest);
        break;
        
    case FlipperRngMenuDiceware:
        FURI_LOG_I(TAG, "Passphrase Generator selected");
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipperRngViewDiceware);
        break;
        
    case FlipperRngMenuAbout:
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipperRngViewAbout);
        break;
        
    case FlipperRngMenuDonate:
        FURI_LOG_I(TAG, "Donate selected");
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipperRngViewDonate);
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
    app->state->output_mode = OutputModeUART;  // Default to UART
    app->state->mixing_mode = MixingModeHardware;  // Default to HW AES
    app->state->wordlist_type = PassphraseListEFFLong;  // Default to EFF wordlist
    app->state->poll_interval_ms = 1;  // Maximum performance - 1ms polling
    app->state->visual_refresh_ms = 500;  // Smooth, easy-to-watch visualization
    app->state->is_running = false;
    app->state->entropy_pool_pos = 0;
    app->state->bytes_generated = 0;
    
    // Initialize hardware acceleration
    flipper_rng_hw_accel_init();
    app->state->samples_collected = 0;
    app->state->bits_from_hw_rng = 0;
    app->state->bits_from_subghz_rssi = 0;
    app->state->bits_from_infrared = 0;
    memset(app->state->byte_histogram, 0, sizeof(app->state->byte_histogram));
    app->state->entropy_rate = 0.0f;
    app->state->adc_handle = NULL;
    app->state->serial_handle = NULL;
    app->state->test_buffer = NULL;
    app->state->test_buffer_size = 0;
    app->state->test_buffer_pos = 0;
    app->state->test_running = false;
    app->state->test_started_worker = false;
    app->state->test_result = 0.0f;
    
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
    
    // Main submenu
    app->submenu = submenu_alloc();
    submenu_set_header(app->submenu, "Entropy Lab v1.0");
    submenu_add_item(app->submenu, "Start Generator", FlipperRngMenuToggle, flipper_rng_menu_callback, app);
    submenu_add_item(app->submenu, "Config", FlipperRngMenuConfig, flipper_rng_menu_callback, app);
    submenu_add_item(app->submenu, "Visualize", FlipperRngMenuVisualization, flipper_rng_menu_callback, app);
    submenu_add_item(app->submenu, "Distribution", FlipperRngMenuByteDistribution, flipper_rng_menu_callback, app);
    submenu_add_item(app->submenu, "Sources", FlipperRngMenuSourceStats, flipper_rng_menu_callback, app);
    submenu_add_item(app->submenu, "Test Quality", FlipperRngMenuTest, flipper_rng_menu_callback, app);
    submenu_add_item(app->submenu, "Passphrase Generator", FlipperRngMenuDiceware, flipper_rng_menu_callback, app);
    submenu_add_item(app->submenu, "About", FlipperRngMenuAbout, flipper_rng_menu_callback, app);
    submenu_add_item(app->submenu, "Donate", FlipperRngMenuDonate, flipper_rng_menu_callback, app);
    
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
    
    // Byte Distribution view
    app->byte_distribution_view = view_alloc();
    view_set_context(app->byte_distribution_view, app);
    view_allocate_model(app->byte_distribution_view, ViewModelTypeLocking, sizeof(FlipperRngVisualizationModel));
    view_set_draw_callback(app->byte_distribution_view, flipper_rng_byte_distribution_draw_callback);
    view_set_input_callback(app->byte_distribution_view, flipper_rng_byte_distribution_input_callback);
    view_set_enter_callback(app->byte_distribution_view, flipper_rng_byte_distribution_enter_callback);
    view_set_previous_callback(app->byte_distribution_view, flipper_rng_back_callback);
    view_dispatcher_add_view(app->view_dispatcher, FlipperRngViewByteDistribution, app->byte_distribution_view);
    
    // Source stats view
    app->source_stats_view = view_alloc();
    view_set_context(app->source_stats_view, app);
    view_allocate_model(app->source_stats_view, ViewModelTypeLocking, sizeof(FlipperRngVisualizationModel));
    view_set_draw_callback(app->source_stats_view, flipper_rng_source_stats_draw_callback);
    view_set_input_callback(app->source_stats_view, flipper_rng_source_stats_input_callback);
    view_set_previous_callback(app->source_stats_view, flipper_rng_back_callback);
    view_dispatcher_add_view(app->view_dispatcher, FlipperRngViewSourceStats, app->source_stats_view);
    
    // Test view
    app->test_view = view_alloc();
    view_set_context(app->test_view, app);
    view_allocate_model(app->test_view, ViewModelTypeLocking, sizeof(FlipperRngTestModel));
    view_set_draw_callback(app->test_view, flipper_rng_test_draw_callback);
    view_set_input_callback(app->test_view, flipper_rng_test_input_callback);
    view_set_enter_callback(app->test_view, flipper_rng_test_enter_callback);
    view_set_exit_callback(app->test_view, flipper_rng_test_exit_callback);
    view_set_previous_callback(app->test_view, flipper_rng_back_callback);
    view_dispatcher_add_view(app->view_dispatcher, FlipperRngViewTest, app->test_view);
    
    // Passphrase generator view
    app->diceware_view = flipper_rng_passphrase_view_alloc(app);
    view_set_previous_callback(app->diceware_view, flipper_rng_back_callback);
    view_dispatcher_add_view(app->view_dispatcher, FlipperRngViewDiceware, app->diceware_view);
    
    // About view (simplified)
    app->about_view = flipper_rng_about_view_alloc();
    view_set_previous_callback(app->about_view, flipper_rng_back_callback);
    view_dispatcher_add_view(app->view_dispatcher, FlipperRngViewAbout, app->about_view);
    
    // Donate view with QR code
    app->donate_view = flipper_rng_donate_view_alloc();
    view_set_previous_callback(app->donate_view, flipper_rng_back_callback);
    view_dispatcher_add_view(app->view_dispatcher, FlipperRngViewDonate, app->donate_view);
    
    // Splash screen
    app->splash = flipper_rng_splash_alloc();
    View* splash_view = flipper_rng_splash_get_view(app->splash);
    view_dispatcher_add_view(app->view_dispatcher, FlipperRngViewSplash, splash_view);
    
    // Create worker thread
    app->worker_thread = furi_thread_alloc();
    furi_thread_set_name(app->worker_thread, "FlipperRngWorker");
    furi_thread_set_stack_size(app->worker_thread, 4096);  // Increased from 2048 to prevent stack overflow
    furi_thread_set_callback(app->worker_thread, flipper_rng_worker_thread);
    furi_thread_set_context(app->worker_thread, app);
    
    // Initialize IR worker (but don't start it yet)
    app->ir_worker = NULL;  // Will be allocated when generation starts
    
    // Start with main menu (splash screen temporarily disabled)
    FURI_LOG_I(TAG, "Starting at main menu...");
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
    
    // Stop and free IR worker if it exists
    flipper_rng_stop_ir_worker(app);
    
    // Deinitialize hardware acceleration
    flipper_rng_hw_accel_deinit();
    
    // Turn off all LEDs before exiting
    flipper_rng_set_led_off(app);
    
    // USB cleanup removed for stability
    
    // Free worker thread
    furi_thread_free(app->worker_thread);
    
    // Free views
    view_dispatcher_remove_view(app->view_dispatcher, FlipperRngViewMenu);
    view_dispatcher_remove_view(app->view_dispatcher, FlipperRngViewConfig);
    view_dispatcher_remove_view(app->view_dispatcher, FlipperRngViewOutput);
    view_dispatcher_remove_view(app->view_dispatcher, FlipperRngViewVisualization);
    view_dispatcher_remove_view(app->view_dispatcher, FlipperRngViewByteDistribution);
    view_dispatcher_remove_view(app->view_dispatcher, FlipperRngViewSourceStats);
    view_dispatcher_remove_view(app->view_dispatcher, FlipperRngViewTest);
    view_dispatcher_remove_view(app->view_dispatcher, FlipperRngViewDiceware);
    view_dispatcher_remove_view(app->view_dispatcher, FlipperRngViewAbout);
    view_dispatcher_remove_view(app->view_dispatcher, FlipperRngViewDonate);
    view_dispatcher_remove_view(app->view_dispatcher, FlipperRngViewSplash);
    
    submenu_free(app->submenu);
    variable_item_list_free(app->variable_item_list);
    text_box_free(app->text_box);
    furi_string_free(app->text_box_store);
    view_free(app->visualization_view);
    view_free(app->byte_distribution_view);
    view_free(app->source_stats_view);
    view_free(app->test_view);
    flipper_rng_passphrase_view_free(app->diceware_view);
    flipper_rng_about_view_free(app->about_view);
    flipper_rng_donate_view_free(app->donate_view);
    flipper_rng_splash_free(app->splash);
    
    view_dispatcher_free(app->view_dispatcher);
    
    // Close records
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    
    // Free hardware handles
    if(app->state->adc_handle) {
        furi_hal_adc_release(app->state->adc_handle);
    }
    if(app->state->serial_handle) {
        furi_hal_serial_deinit(app->state->serial_handle);
        furi_hal_serial_control_release(app->state->serial_handle);
    }
    
    
    // Free test buffer if allocated
    if(app->state->test_buffer) {
        free(app->state->test_buffer);
        app->state->test_buffer = NULL;
    }
    
    // Free state
    furi_mutex_free(app->state->mutex);
    free(app->state);
    free(app);
}

static void flipper_rng_splash_check_timer(void* context) {
    FlipperRngApp* app = context;
    
    // Check if splash is done
    if(flipper_rng_splash_is_done(app->splash)) {
        // Stop checking
        furi_timer_stop(app->splash_timer);
        
        // Switch to main menu
        FURI_LOG_I(TAG, "Splash done, switching to menu");
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipperRngViewMenu);
    }
}

int32_t entropylab_app(void* p) {
    UNUSED(p);
    
    FURI_LOG_I(TAG, "FlipperRNG starting...");
    
    FlipperRngApp* app = flipper_rng_app_alloc();
    if(!app) {
        FURI_LOG_E(TAG, "Failed to allocate app");
        return -1;
    }
    
    // Create timer to check when splash is done
    app->splash_timer = furi_timer_alloc(
        flipper_rng_splash_check_timer,
        FuriTimerTypePeriodic,
        app
    );
    furi_timer_start(app->splash_timer, 100); // Check every 100ms
    
    FURI_LOG_I(TAG, "App allocated, starting view dispatcher");
    view_dispatcher_run(app->view_dispatcher);
    
    // Clean up splash timer
    if(app->splash_timer) {
        furi_timer_stop(app->splash_timer);
        furi_timer_free(app->splash_timer);
    }
    
    FURI_LOG_I(TAG, "View dispatcher exited, cleaning up");
    flipper_rng_app_free(app);
    
    FURI_LOG_I(TAG, "FlipperRNG exited cleanly");
    return 0;
}

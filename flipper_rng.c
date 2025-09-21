#include "flipper_rng.h"
#include "flipper_rng_entropy.h"
#include "flipper_rng_views.h"
#include <furi_hal_random.h>
#include <furi_hal_adc.h>
#include <furi_hal_power.h>
#include <furi_hal_serial.h>
#include <furi_hal_usb_cdc.h>
#include <furi_hal_usb.h>
#include <cli/cli_vcp.h>
#include <furi_hal.h>
#include <power/power_service/power.h>
#include <toolbox/stream/stream.h>
#include <toolbox/stream/file_stream.h>

#define TAG "FlipperRNG"

// Minimal CDC callbacks for interface 1
static void flipper_rng_cdc_tx_complete(void* context) {
    // Log when TX completes to verify callbacks are working
    UNUSED(context);
    FURI_LOG_D(TAG, "CDC TX complete callback fired");
}

static void flipper_rng_cdc_rx(void* context) {
    // We don't receive data, only send
    UNUSED(context);
}

static void flipper_rng_cdc_state(void* context, uint8_t state) {
    UNUSED(context);
    FURI_LOG_I(TAG, "CDC state changed: 0x%02X", state);
}

static void flipper_rng_cdc_ctrl_line(void* context, uint8_t state) {
    UNUSED(context);
    UNUSED(state);
}

static void flipper_rng_cdc_line_config(void* context, struct usb_cdc_line_coding* config) {
    UNUSED(context);
    UNUSED(config);
}

static const CdcCallbacks flipper_rng_cdc_cb = {
    flipper_rng_cdc_tx_complete,
    flipper_rng_cdc_rx,
    flipper_rng_cdc_state,
    flipper_rng_cdc_ctrl_line,
    flipper_rng_cdc_line_config,
};

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
    FlipperRngMenuToggle,  // Combined Start/Stop
    FlipperRngMenuConfig,
    FlipperRngMenuVisualization,
    FlipperRngMenuStats,
    FlipperRngMenuTest,
    FlipperRngMenuAbout,
    FlipperRngMenuQuit,
} FlipperRngMenuItem;

static void flipper_rng_menu_callback(void* context, uint32_t index) {
    FlipperRngApp* app = context;
    
    switch(index) {
    case FlipperRngMenuToggle:
        if(!app->state->is_running) {
            // Start the generator
            FURI_LOG_I(TAG, "Start Generator selected, is_running=%d", app->state->is_running);
            
            // ALWAYS stop and clean up first, regardless of is_running flag
            FURI_LOG_I(TAG, "Force stopping any existing worker thread...");
            app->state->is_running = false;
        
        // Clean up any existing USB configuration
        if(app->state->output_mode == OutputModeUSB) {
            furi_hal_cdc_set_callbacks(1, NULL, NULL);
            FURI_LOG_I(TAG, "Cleared existing USB callbacks");
        }
        
        // Wait for thread to actually stop if it's running
        if(furi_thread_get_state(app->worker_thread) != FuriThreadStateStopped) {
            FURI_LOG_I(TAG, "Waiting for worker thread to stop...");
            furi_thread_join(app->worker_thread);
            FURI_LOG_I(TAG, "Worker thread stopped");
        }
        
        // Now start with current settings
        FURI_LOG_I(TAG, "Starting worker thread with current settings...");
        
        // Initialize output interface based on mode
        if(app->state->output_mode == OutputModeUART && !app->state->serial_handle) {
            FURI_LOG_I(TAG, "Initializing UART for output...");
            app->state->serial_handle = furi_hal_serial_control_acquire(FuriHalSerialIdUsart);
            if(app->state->serial_handle) {
                furi_hal_serial_init(app->state->serial_handle, 115200);
                FURI_LOG_I(TAG, "UART initialized at 115200 baud");
            } else {
                FURI_LOG_E(TAG, "Failed to acquire UART");
            }
        } else if(app->state->output_mode == OutputModeUSB) {
            FURI_LOG_I(TAG, "Setting up USB CDC interface 1 (like GPIO USB UART bridge)...");
            
            // Get CLI VCP handle
            CliVcp* cli_vcp = furi_record_open(RECORD_CLI_VCP);
            
            // Disable CLI VCP temporarily
            cli_vcp_disable(cli_vcp);
            
            // Switch to dual CDC mode - use reinit to force the change
            furi_hal_usb_unlock();
            
            // Get current config to check if we're already in dual mode
            FuriHalUsbInterface* current = furi_hal_usb_get_config();
            FURI_LOG_I(TAG, "Current USB config: %p, dual: %p, single: %p", 
                       current, &usb_cdc_dual, &usb_cdc_single);
            
            // If not in dual mode, force it
            if(current != &usb_cdc_dual) {
                FURI_LOG_I(TAG, "Not in dual mode, forcing USB reinit...");
                
                // Force USB to reinitialize with dual CDC mode
                furi_hal_usb_disable();
                furi_delay_ms(100);
                
                if(furi_hal_usb_set_config(&usb_cdc_dual, NULL)) {
                    FURI_LOG_I(TAG, "USB config set to dual CDC");
                } else {
                    FURI_LOG_W(TAG, "USB config change failed, trying reinit");
                    furi_hal_usb_reinit();
                    furi_delay_ms(100);
                    furi_hal_usb_set_config(&usb_cdc_dual, NULL);
                }
                
                furi_hal_usb_enable();
                FURI_LOG_I(TAG, "USB re-enabled");
            } else {
                FURI_LOG_I(TAG, "Already in dual CDC mode");
            }
            
            // Re-enable CLI VCP on interface 0  
            cli_vcp_enable(cli_vcp);
            FURI_LOG_I(TAG, "CLI VCP re-enabled on interface 0");
            
            // CRITICAL: Set callbacks for interface 1 to make it functional
            furi_hal_cdc_set_callbacks(1, (CdcCallbacks*)&flipper_rng_cdc_cb, app);
            FURI_LOG_I(TAG, "CDC callbacks set for interface 1");
            
            // Give USB more time to settle and enumerate both interfaces
            FURI_LOG_I(TAG, "Waiting for USB enumeration...");
            furi_delay_ms(1000);
            
            // Verify the configuration took effect
            FuriHalUsbInterface* new_config = furi_hal_usb_get_config();
            if(new_config == &usb_cdc_dual) {
                FURI_LOG_I(TAG, "USB CDC dual mode confirmed - interface 1 ready");
            } else {
                FURI_LOG_E(TAG, "USB config still not dual! Got %p, expected %p", 
                           new_config, &usb_cdc_dual);
            }
            
            furi_record_close(RECORD_CLI_VCP);
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
        app->state->bits_from_adc = 0;
        app->state->bits_from_timing = 0;
        app->state->bits_from_cpu_jitter = 0;
        app->state->bits_from_battery = 0;
        app->state->bits_from_temperature = 0;
        app->state->bits_from_button = 0;
        app->state->bits_from_subghz_rssi = 0;
        app->state->bits_from_nfc_field = 0;
        memset(app->state->byte_histogram, 0, sizeof(app->state->byte_histogram));
        
        // Start the worker thread
            app->state->is_running = true;
            furi_thread_start(app->worker_thread);
            flipper_rng_set_led_generating(app);  // Set LED to green
            FURI_LOG_I(TAG, "Worker thread started from menu, is_running=%d", app->state->is_running);
        } else {
            // Stop the generator
            FURI_LOG_I(TAG, "Stopping worker thread...");
            app->state->is_running = false;
            
            // Clean up USB if it was used
            if(app->state->output_mode == OutputModeUSB) {
                // Clear callbacks for interface 1
                furi_hal_cdc_set_callbacks(1, NULL, NULL);
                FURI_LOG_I(TAG, "USB CDC callbacks cleared");
            }
            
            // Release UART if it was acquired
            if(app->state->serial_handle) {
                furi_hal_serial_deinit(app->state->serial_handle);
                furi_hal_serial_control_release(app->state->serial_handle);
                app->state->serial_handle = NULL;
                FURI_LOG_I(TAG, "UART released");
            }
            
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
        
    case FlipperRngMenuTest:
        FURI_LOG_I(TAG, "Test RNG Quality selected");
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipperRngViewTest);
        break;
        
    case FlipperRngMenuStats:
        {
            furi_string_reset(app->text_box_store);
            
            // Calculate total entropy bits
            uint32_t total_bits = app->state->bits_from_hw_rng + 
                                  app->state->bits_from_adc + 
                                  app->state->bits_from_timing + 
                                  app->state->bits_from_cpu_jitter + 
                                  app->state->bits_from_battery + 
                                  app->state->bits_from_temperature + 
                                  app->state->bits_from_button + 
                                  app->state->bits_from_subghz_rssi + 
                                  app->state->bits_from_nfc_field;
            
            // Build the statistics display
            furi_string_printf(
                app->text_box_store,
                "=== RNG Statistics ===\n"
                "Output: %lu bytes\n"
                "Rate: %d bits/sec\n\n"
                "--- Entropy Sources ---\n",
                app->state->bytes_generated,
                (int)app->state->entropy_rate
            );
            
            // Show each source with bits collected
            if(app->state->entropy_sources & EntropySourceHardwareRNG) {
                furi_string_cat_printf(app->text_box_store, 
                    "HW RNG: %lu bits\n", app->state->bits_from_hw_rng);
            }
            if(app->state->entropy_sources & EntropySourceADC) {
                furi_string_cat_printf(app->text_box_store, 
                    "ADC: %lu bits\n", app->state->bits_from_adc);
            }
            if(app->state->entropy_sources & EntropySourceTiming) {
                furi_string_cat_printf(app->text_box_store, 
                    "Timing: %lu bits\n", app->state->bits_from_timing);
            }
            if(app->state->entropy_sources & EntropySourceCPUJitter) {
                furi_string_cat_printf(app->text_box_store, 
                    "CPU: %lu bits\n", app->state->bits_from_cpu_jitter);
            }
            if(app->state->entropy_sources & EntropySourceBatteryVoltage) {
                furi_string_cat_printf(app->text_box_store, 
                    "Battery: %lu bits\n", app->state->bits_from_battery);
            }
            if(app->state->entropy_sources & EntropySourceTemperature) {
                furi_string_cat_printf(app->text_box_store, 
                    "Temp: %lu bits\n", app->state->bits_from_temperature);
            }
            if(app->state->entropy_sources & EntropySourceSubGhzRSSI) {
                furi_string_cat_printf(app->text_box_store, 
                    "SubGHz: %lu bits\n", app->state->bits_from_subghz_rssi);
            }
            if(app->state->entropy_sources & EntropySourceNFCField) {
                furi_string_cat_printf(app->text_box_store, 
                    "NFC: %lu bits\n", app->state->bits_from_nfc_field);
            }
            
            furi_string_cat_printf(app->text_box_store, 
                "\nTotal: %lu bits\n"
                "Pool: %zu/%d\n",
                total_bits,
                app->state->entropy_pool_pos,
                RNG_POOL_SIZE
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
            "Author: Luke Macken\n"
            "<luke@phorex.org>\n",
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
    app->state->output_mode = OutputModeVisualization;
    app->state->poll_interval_ms = 10;
    app->state->is_running = false;
    app->state->entropy_pool_pos = 0;
    app->state->bytes_generated = 0;
    app->state->samples_collected = 0;
    app->state->bits_from_hw_rng = 0;
    app->state->bits_from_adc = 0;
    app->state->bits_from_timing = 0;
    app->state->bits_from_cpu_jitter = 0;
    app->state->bits_from_battery = 0;
    app->state->bits_from_temperature = 0;
    app->state->bits_from_button = 0;
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
    
    // Main menu
    app->submenu = submenu_alloc();
    submenu_set_header(app->submenu, "FlipperRNG v1.0");
    submenu_add_item(app->submenu, "Start/Stop Generator", FlipperRngMenuToggle, flipper_rng_menu_callback, app);
    submenu_add_item(app->submenu, "Configuration", FlipperRngMenuConfig, flipper_rng_menu_callback, app);
    submenu_add_item(app->submenu, "Visualization", FlipperRngMenuVisualization, flipper_rng_menu_callback, app);
    submenu_add_item(app->submenu, "Statistics", FlipperRngMenuStats, flipper_rng_menu_callback, app);
    submenu_add_item(app->submenu, "Test RNG Quality", FlipperRngMenuTest, flipper_rng_menu_callback, app);
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
    
    // Test view
    app->test_view = view_alloc();
    view_set_context(app->test_view, app);
    view_allocate_model(app->test_view, ViewModelTypeLocking, sizeof(FlipperRngTestModel));
    view_set_draw_callback(app->test_view, flipper_rng_test_draw_callback);
    view_set_input_callback(app->test_view, flipper_rng_test_input_callback);
    view_set_previous_callback(app->test_view, flipper_rng_back_callback);
    view_dispatcher_add_view(app->view_dispatcher, FlipperRngViewTest, app->test_view);
    
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
    
    // USB cleanup removed for stability
    
    // Free worker thread
    furi_thread_free(app->worker_thread);
    
    // Free views
    view_dispatcher_remove_view(app->view_dispatcher, FlipperRngViewMenu);
    view_dispatcher_remove_view(app->view_dispatcher, FlipperRngViewConfig);
    view_dispatcher_remove_view(app->view_dispatcher, FlipperRngViewOutput);
    view_dispatcher_remove_view(app->view_dispatcher, FlipperRngViewVisualization);
    view_dispatcher_remove_view(app->view_dispatcher, FlipperRngViewTest);
    
    submenu_free(app->submenu);
    variable_item_list_free(app->variable_item_list);
    text_box_free(app->text_box);
    furi_string_free(app->text_box_store);
    view_free(app->visualization_view);
    view_free(app->test_view);
    
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

#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/text_box.h>
#include <notification/notification_messages.h>
#include <infrared_worker.h>
#include <storage/storage.h>
#include "entropylab_passphrase_sd.h"

#define FLIPPER_RNG_VERSION "1.0"
#define RNG_BUFFER_SIZE 256
#define RNG_POOL_SIZE 4096
#define RNG_OUTPUT_CHUNK_SIZE 64

// Entropy source flags - High-quality sources only
typedef enum {
    EntropySourceHardwareRNG = (1 << 0),        // STM32WB55 TRNG - HIGHEST QUALITY (32 bits)
    EntropySourceSubGhzRSSI = (1 << 1),         // RF atmospheric noise - HIGH QUALITY (10 bits)
    EntropySourceInfraredNoise = (1 << 2),      // IR ambient noise - HIGH QUALITY (8 bits)
    EntropySourceAll = 0x07,                    // All high-quality sources
} EntropySource;

// Output mode - Visualization is always available, not an exclusive output mode
typedef enum {
    OutputModeNone,     // No output (visualization only)
    OutputModeUART,
    OutputModeFile,
} OutputMode;

// Mixing mode for entropy pool
typedef enum {
    MixingModeHardware,  // Force hardware AES only
    MixingModeSoftware,  // Force software XOR mixing only
} MixingMode;

// View IDs
typedef enum {
    FlipperRngViewSplash,            // Splash screen
    FlipperRngViewMenu,
    FlipperRngViewConfig,
    FlipperRngViewOutput,
    FlipperRngViewVisualization,
    FlipperRngViewByteDistribution,  // New: Byte Distribution
    FlipperRngViewSourceStats,       // New: Entropy source comparison
    FlipperRngViewDiceware,          // New: Passphrase generator
    FlipperRngViewAbout,             // About view (simplified)
    FlipperRngViewDonate,            // New: Donation QR code view
} FlipperRngView;

// Application state
#define FLIPPER_RNG_STATE_DEFINED
typedef struct {
    FuriMutex* mutex;
    uint32_t entropy_sources;
    OutputMode output_mode;
    MixingMode mixing_mode;
    PassphraseListType wordlist_type;  // Selected wordlist for passphrase generation
    uint32_t poll_interval_ms;
    uint32_t visual_refresh_ms;  // Configurable visualization refresh rate
    bool is_running;
    uint8_t entropy_pool[RNG_POOL_SIZE];
    size_t entropy_pool_pos;
    uint32_t bytes_generated;
    
    
    // Hardware handles
    FuriHalAdcHandle* adc_handle;
    FuriHalSerialHandle* serial_handle;
    
    // Statistics
    uint32_t samples_collected;
    uint32_t last_entropy_bits;
    uint32_t start_time;  // Track when generation started
    float entropy_rate;   // Bits per second
    
    // Histogram data for byte distribution (0-255)
    uint32_t byte_histogram[16];  // 16 bins for visualization
    
    // Per-source bit counters (high-quality sources only)
    uint32_t bits_from_hw_rng;
    uint32_t bits_from_subghz_rssi;
    uint32_t bits_from_infrared;
} FlipperRngState;

// Forward declaration
typedef struct FuriHalUsbInterface FuriHalUsbInterface;

// Main application structure
typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    Submenu* submenu;
    VariableItemList* variable_item_list;
    TextBox* text_box;
    FuriString* text_box_store;
    NotificationApp* notifications;
    
    FlipperRngState* state;
    FuriThread* worker_thread;
    FuriTimer* splash_timer;  // Timer for splash screen transition
    
    
    // Views
    void* splash;  // Splash screen
    View* visualization_view;
    View* byte_distribution_view;  // New: Byte Distribution view
    View* source_stats_view;       // New: Entropy source stats view
    View* diceware_view;           // New: Passphrase generator view
    View* about_view;              // About view (simplified)
    View* donate_view;             // New: Donation QR code view
    
    // Persistent IR worker for continuous collection
    InfraredWorker* ir_worker;
} FlipperRngApp;

// Function prototypes
FlipperRngApp* flipper_rng_app_alloc(void);
void flipper_rng_app_free(FlipperRngApp* app);

// LED status control functions
void flipper_rng_set_led_stopped(FlipperRngApp* app);
void flipper_rng_set_led_generating(FlipperRngApp* app);
void flipper_rng_set_led_off(FlipperRngApp* app);

// IR callback for persistent worker
void flipper_rng_ir_callback(void* ctx, InfraredWorkerSignal* signal);

// Entropy collection functions - High-quality, environment-independent sources
void flipper_rng_collect_hardware_rng(FlipperRngState* state);
void flipper_rng_collect_adc_entropy(FlipperRngState* state);
void flipper_rng_collect_battery_entropy(FlipperRngState* state);
void flipper_rng_collect_temperature_entropy(FlipperRngState* state);
void flipper_rng_collect_subghz_rssi_entropy(FlipperRngState* state);
void flipper_rng_collect_infrared_entropy(FlipperRngState* state);

// Entropy mixing and output
void flipper_rng_mix_pool(FlipperRngState* state);
uint8_t flipper_rng_extract_byte(FlipperRngState* state);
void flipper_rng_output_bytes(FlipperRngState* state, uint8_t* buffer, size_t length);

// Worker thread
int32_t flipper_rng_worker_thread(void* context);


// View callbacks
void flipper_rng_visualization_draw_callback(Canvas* canvas, void* context);
bool flipper_rng_visualization_input_callback(InputEvent* event, void* context);

// View callbacks are in flipper_rng_views.h

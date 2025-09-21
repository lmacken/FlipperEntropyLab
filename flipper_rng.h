#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/text_box.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>

#define FLIPPER_RNG_VERSION "1.0"
#define RNG_BUFFER_SIZE 256
#define RNG_POOL_SIZE 4096
#define RNG_OUTPUT_CHUNK_SIZE 64

// Entropy source flags
typedef enum {
    EntropySourceHardwareRNG = (1 << 0),
    EntropySourceADC = (1 << 1),
    EntropySourceTiming = (1 << 2),
    EntropySourceButtonTiming = (1 << 3),
    EntropySourceCPUJitter = (1 << 4),
    EntropySourceBatteryVoltage = (1 << 5),
    EntropySourceTemperature = (1 << 6),
    EntropySourceSubGhzRSSI = (1 << 7),
    EntropySourceNFCField = (1 << 8),
    EntropySourceInfraredNoise = (1 << 9),
    EntropySourceAll = 0x3FF,
} EntropySource;

// Output mode
typedef enum {
    OutputModeUSB,
    OutputModeUART,
    OutputModeVisualization,
    OutputModeFile,
} OutputMode;

// View IDs
typedef enum {
    FlipperRngViewMenu,
    FlipperRngViewConfig,
    FlipperRngViewOutput,
    FlipperRngViewVisualization,
    FlipperRngViewTest,
} FlipperRngView;

// Application state
typedef struct {
    FuriMutex* mutex;
    uint32_t entropy_sources;
    OutputMode output_mode;
    uint32_t poll_interval_ms;
    bool is_running;
    uint8_t entropy_pool[RNG_POOL_SIZE];
    size_t entropy_pool_pos;
    uint32_t bytes_generated;
    
    // Test data
    uint8_t* test_buffer;
    size_t test_buffer_size;
    size_t test_buffer_pos;
    bool test_running;
    bool test_started_worker;  // Track if test started the worker
    float test_result;
    
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
    
    // Per-source bit counters (track bits collected from each source)
    uint32_t bits_from_hw_rng;
    uint32_t bits_from_adc;
    uint32_t bits_from_timing;
    uint32_t bits_from_cpu_jitter;
    uint32_t bits_from_battery;
    uint32_t bits_from_temperature;
    uint32_t bits_from_button;
    uint32_t bits_from_subghz_rssi;
    uint32_t bits_from_nfc_field;
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
    
    
    // Views
    View* visualization_view;
    View* test_view;
} FlipperRngApp;

// Function prototypes
FlipperRngApp* flipper_rng_app_alloc(void);
void flipper_rng_app_free(FlipperRngApp* app);

// Entropy collection functions
void flipper_rng_collect_hardware_rng(FlipperRngState* state);
void flipper_rng_collect_adc_entropy(FlipperRngState* state);
void flipper_rng_collect_timing_entropy(FlipperRngState* state);
void flipper_rng_collect_cpu_jitter(FlipperRngState* state);
void flipper_rng_collect_battery_entropy(FlipperRngState* state);
void flipper_rng_collect_temperature_entropy(FlipperRngState* state);
void flipper_rng_collect_subghz_rssi_entropy(FlipperRngState* state);
void flipper_rng_collect_nfc_field_entropy(FlipperRngState* state);
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

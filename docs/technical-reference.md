# FlipperRNG Technical Reference

## Overview

This document provides detailed technical information for developers working with or extending the FlipperRNG entropy collection system.

## Table of Contents

1. [Build System Integration](#build-system-integration)
2. [Data Structures](#data-structures)
3. [API Reference](#api-reference)
4. [Testing Framework](#testing-framework)
5. [Performance Characteristics](#performance-characteristics)
6. [Security Considerations](#security-considerations)
7. [Troubleshooting](#troubleshooting)

## Build System Integration

### Application Manifest

The FlipperRNG application is defined in [`application.fam`](../application.fam):

```python
App(
    appid="flipper_rng",
    name="FlipperRNG",
    apptype=FlipperAppType.EXTERNAL,
    entry_point="flipper_rng_app",
    stack_size=4 * 1024,
    fap_category="Tools",
    fap_description="Multi-source hardware random number generator",
    fap_author="Your Name",
    fap_version="1.0",
    fap_icon="flipper_rng.png",
)
```

### Dependencies

The application requires the following Flipper Zero firmware APIs:
- `furi` - Core OS services
- `furi_hal` - Hardware abstraction layer
- `gui` - Graphical user interface
- `notification` - LED/vibration notifications
- `storage` - File system access

## Data Structures

### FlipperRngState

Main application state structure defined in [`flipper_rng.h`](../flipper_rng.h#L48-L87):

```c
typedef struct {
    FuriMutex* mutex;                    // Thread synchronization
    uint32_t entropy_sources;           // Enabled source bitmask
    OutputMode output_mode;             // Output destination
    uint32_t poll_interval_ms;          // Collection timing
    bool is_running;                    // Worker thread control
    
    // Entropy pool
    uint8_t entropy_pool[RNG_POOL_SIZE]; // 4096-byte circular buffer
    size_t entropy_pool_pos;            // Current pool position
    uint32_t bytes_generated;           // Total output bytes
    
    // Test framework
    uint8_t* test_buffer;               // Test data buffer
    size_t test_buffer_size;            // Test buffer size
    size_t test_buffer_pos;             // Test buffer position
    bool test_running;                  // Test active flag
    bool test_started_worker;           // Test worker control
    float test_result;                  // Test result score
    
    // Hardware handles
    FuriHalAdcHandle* adc_handle;       // ADC peripheral handle
    FuriHalSerialHandle* serial_handle; // UART handle
    
    // Statistics
    uint32_t samples_collected;         // Collection iterations
    uint32_t last_entropy_bits;         // Last entropy estimate
    uint32_t start_time;                // Generation start time
    float entropy_rate;                 // Bits per second
    
    // Visualization data
    uint32_t byte_histogram[16];        // Byte distribution (16 bins)
    
    // Per-source statistics
    uint32_t bits_from_hw_rng;          // Hardware RNG bits
    uint32_t bits_from_adc;             // ADC noise bits
    uint32_t bits_from_timing;          // Timing jitter bits
    uint32_t bits_from_cpu_jitter;      // CPU jitter bits
    uint32_t bits_from_battery;         // Battery noise bits
    uint32_t bits_from_temperature;     // Temperature noise bits
    uint32_t bits_from_button;          // Button timing bits
} FlipperRngState;
```

### FlipperRngApp

Main application structure defined in [`flipper_rng.h`](../flipper_rng.h#L93-L109):

```c
typedef struct {
    Gui* gui;                           // GUI handle
    ViewDispatcher* view_dispatcher;    // View management
    Submenu* submenu;                   // Main menu
    VariableItemList* variable_item_list; // Configuration menu
    TextBox* text_box;                  // Text display
    FuriString* text_box_store;         // Text storage
    NotificationApp* notifications;     // LED/vibration
    
    FlipperRngState* state;             // Application state
    FuriThread* worker_thread;          // Background worker
    
    // Custom views
    View* visualization_view;           // Real-time visualization
    View* test_view;                    // Statistical testing
} FlipperRngApp;
```

### VonNeumannExtractor

Debiasing structure defined in [`flipper_rng_entropy.h`](../flipper_rng_entropy.h#L25-L29):

```c
typedef struct {
    uint8_t prev_bit;                   // Previous bit value
    bool has_prev;                      // Previous bit valid flag
} VonNeumannExtractor;
```

## API Reference

### Core Entropy Functions

#### Initialization
```c
void flipper_rng_init_entropy_sources(FlipperRngState* state);
void flipper_rng_deinit_entropy_sources(FlipperRngState* state);
```
- **Purpose**: Initialize/cleanup hardware resources
- **Location**: [`flipper_rng_entropy.c:6-39`](../flipper_rng_entropy.c#L6-L39)
- **Thread Safety**: Not thread-safe, call from main thread only

#### Hardware Collection
```c
uint32_t flipper_rng_get_hardware_random(void);
uint32_t flipper_rng_get_adc_noise(FuriHalAdcHandle* handle);
uint32_t flipper_rng_get_timing_jitter(void);
uint32_t flipper_rng_get_cpu_jitter(void);
uint32_t flipper_rng_get_battery_noise(void);
uint32_t flipper_rng_get_temperature_noise(void);
```
- **Purpose**: Collect entropy from individual hardware sources
- **Returns**: 32-bit entropy value
- **Thread Safety**: Safe to call from worker thread
- **Performance**: Varies by source (see performance section)

#### Pool Management
```c
void flipper_rng_add_entropy(FlipperRngState* state, uint32_t entropy, uint8_t bits);
void flipper_rng_mix_entropy_pool(FlipperRngState* state);
uint8_t flipper_rng_extract_random_byte(FlipperRngState* state);
```
- **Purpose**: Manage entropy pool and extract random data
- **Thread Safety**: Protected by mutex
- **Location**: [`flipper_rng_entropy.c:114-192`](../flipper_rng_entropy.c#L114-L192)

#### Von Neumann Debiasing
```c
void von_neumann_init(VonNeumannExtractor* extractor);
bool von_neumann_extract(VonNeumannExtractor* extractor, uint8_t input_bit, uint8_t* output_bit);
```
- **Purpose**: Remove bias from bit sequences
- **Algorithm**: Classic Von Neumann debiasing (01→0, 10→1, 00/11→discard)
- **Thread Safety**: Not thread-safe per extractor instance

### Worker Thread Functions

```c
int32_t flipper_rng_worker_thread(void* context);
```
- **Purpose**: Main entropy collection loop
- **Context**: FlipperRngApp pointer
- **Lifecycle**: Started/stopped via FuriThread API
- **Location**: [`flipper_rng_worker.c:16-248`](../flipper_rng_worker.c#L16-L248)

### View Functions

```c
void flipper_rng_visualization_draw_callback(Canvas* canvas, void* context);
bool flipper_rng_visualization_input_callback(InputEvent* event, void* context);
```
- **Purpose**: Handle GUI rendering and input
- **Thread Safety**: Called from GUI thread only
- **Location**: [`flipper_rng_views.c`](../flipper_rng_views.c)

## Testing Framework

### Statistical Testing

The application includes basic statistical tests for output quality:

#### Chi-Square Test
- **Purpose**: Test for uniform byte distribution
- **Implementation**: [`flipper_rng_views.c`](../flipper_rng_views.c) (test view functions)
- **Expected**: Chi-square value close to 255 for uniform distribution
- **Sample Size**: Configurable test buffer size

#### Frequency Analysis
- **Purpose**: Analyze bit frequency distribution
- **Visualization**: Real-time histogram display
- **Bins**: 16 bins covering byte values 0-255

### External Testing

For comprehensive testing, use external tools:

#### NIST Statistical Test Suite
```bash
# Generate test data
./flipper_rng_app > random_data.bin

# Run NIST tests
./assess 1000000 < random_data.bin
```

#### Dieharder Test Suite
```bash
# Test with dieharder (referenced in uart_long_test.sh:31)
dieharder -g 200 -f random_data.bin -a
```

#### ENT Test
```bash
# Basic entropy analysis
ent random_data.bin
```

## Performance Characteristics

### Collection Rates

| Source | Time per Sample | Max Rate (samples/sec) | Notes |
|--------|----------------|------------------------|-------|
| Hardware RNG | ~1μs | ~1,000,000 | Limited by HAL calls |
| ADC Noise | ~300μs | ~3,300 | 4 samples + delays |
| Timing Jitter | ~0.1μs | ~10,000,000 | DWT register access |
| CPU Jitter | ~2μs | ~500,000 | 100-iteration loop |
| Battery | ~1ms | ~1,000 | Power IC communication |
| Temperature | ~1ms | ~1,000 | Power IC communication |

### Memory Usage

- **Entropy Pool**: 4,096 bytes
- **Test Buffer**: Variable (default ~64KB)
- **Histogram**: 16 × 4 = 64 bytes
- **Application State**: ~200 bytes
- **Stack Usage**: ~4KB (configured in application.fam)

### Throughput

With default configuration and all sources enabled:
- **Theoretical Maximum**: ~50 bits/iteration
- **Practical Rate**: Depends on poll_interval_ms setting
- **Output Bandwidth**: 
  - USB CDC: Up to ~1 MB/s
  - UART: Up to ~115.2 Kbps (configurable)
  - File: Limited by SD card speed

## Security Considerations

### Entropy Pool Security

1. **Pool Size**: 4096 bytes provides good mixing capacity
2. **LFSR Mixing**: Provides diffusion but not cryptographic strength
3. **Position Tracking**: Prevents state recovery from partial output
4. **Multi-Source**: Defense against single-source failures

### Recommendations

1. **Primary Source**: Hardware RNG should be the primary entropy source
2. **Source Diversity**: Enable multiple sources for robustness
3. **Post-Processing**: Consider additional cryptographic post-processing for critical applications
4. **Testing**: Regular statistical testing of output
5. **Monitoring**: Monitor entropy rate and source health

### Potential Vulnerabilities

1. **Predictable Sources**: Some sources (temperature, battery) change slowly
2. **Environmental Factors**: Physical environment may affect some sources
3. **Power Analysis**: Side-channel attacks on power consumption
4. **Timing Attacks**: Timing-based entropy may be partially predictable

## Troubleshooting

### Common Issues

#### ADC Initialization Fails
```
Failed to acquire ADC handle, disabling ADC entropy
```
**Solution**: Check ADC availability, may be used by other applications

#### UART Output Issues
```
UART not initialized
```
**Solution**: Check GPIO pin configuration and UART initialization

#### Low Entropy Rate
**Symptoms**: Low bits/second in statistics
**Causes**: 
- High poll_interval_ms setting
- Disabled entropy sources
- Hardware issues

**Solutions**:
- Reduce poll interval
- Enable more entropy sources
- Check hardware functionality

#### Test Failures
**Symptoms**: Poor statistical test results
**Causes**:
- Insufficient sample size
- Biased sources
- Implementation bugs

**Solutions**:
- Increase test buffer size
- Check individual source quality
- Verify mixing algorithm

### Debug Information

Enable debug logging by checking worker thread output:
```c
FURI_LOG_I(TAG, "Worker running: cycle=%lu, bytes=%lu", counter, bytes_generated);
```

Monitor entropy source contributions:
```c
FURI_LOG_I(TAG, "HW_RNG: %lu, ADC: %lu, Timing: %lu bits", 
           state->bits_from_hw_rng, state->bits_from_adc, state->bits_from_timing);
```

### Performance Optimization

1. **Poll Interval**: Balance between throughput and power consumption
2. **Source Selection**: Disable slow sources if not needed
3. **Buffer Sizes**: Adjust output buffer size for target bandwidth
4. **Mixing Frequency**: Adjust mixing interval based on entropy rate

### Hardware Diagnostics

Test individual entropy sources:
```c
// Test hardware RNG
uint32_t hw_test = furi_hal_random_get();
FURI_LOG_I(TAG, "Hardware RNG: 0x%08lX", hw_test);

// Test ADC
if(adc_handle) {
    uint16_t adc_test = furi_hal_adc_read(adc_handle, FuriHalAdcChannelVREFINT);
    FURI_LOG_I(TAG, "ADC VREF: %u", adc_test);
}

// Test timing
uint32_t time1 = DWT->CYCCNT;
furi_delay_us(1);
uint32_t time2 = DWT->CYCCNT;
FURI_LOG_I(TAG, "Timing delta: %lu", time2 - time1);
```

---

*Technical Reference for FlipperRNG v1.0*  
*Last updated: December 2024*

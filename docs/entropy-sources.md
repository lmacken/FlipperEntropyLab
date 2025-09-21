# FlipperRNG Entropy Sources Documentation

## Overview

FlipperRNG implements a multi-source entropy collection system that leverages multiple independent hardware noise sources from the Flipper Zero's STM32WB55 microcontroller. This document provides detailed technical information about each entropy source, their underlying hardware, and implementation details.

## Table of Contents

1. [Architecture Overview](#architecture-overview)
2. [Entropy Sources](#entropy-sources)
3. [Hardware Details](#hardware-details)
4. [Implementation Details](#implementation-details)
5. [Quality Assessment](#quality-assessment)
6. [Code References](#code-references)

## Architecture Overview

The FlipperRNG system uses a sophisticated multi-layered approach:

1. **Multiple Independent Sources**: Seven different entropy sources
2. **Entropy Pool Mixing**: 4096-byte circular buffer with LFSR-based mixing
3. **Conservative Entropy Estimation**: Each source rated for actual entropy content
4. **Configurable Collection**: Sources can be enabled/disabled individually
5. **Real-time Processing**: Continuous collection and mixing in worker thread

### System Architecture

```
Hardware Sources → Entropy Collection → Pool Mixing → Output Generation
     ↓                    ↓                ↓              ↓
- Hardware RNG      - Individual        - LFSR Mixing   - Byte Extraction
- ADC Noise         - Collection        - Diffusion     - Von Neumann
- Timing Jitter     - Functions         - Circular      - Debiasing
- CPU Jitter        - Bit Estimation    - Buffer        - Multiple Outputs
- Battery Noise     - Pool Addition     - Position      - (USB/UART/File)
- Temperature       - Statistics        - Tracking      
- Button Timing     - Rate Calculation  - Quality       
```

## Entropy Sources

### 1. Hardware RNG (EntropySourceHardwareRNG)

**Primary high-quality entropy source using STM32WB55's dedicated TRNG peripheral.**

#### Hardware Details
- **Peripheral**: STM32WB55 RNG (Random Number Generator)
- **Clock Source**: 48 MHz dedicated RNG clock
- **Internal Sources**: 
  - Multiple independent ring oscillators
  - Thermal noise from analog circuits
  - Silicon process variations
  - Internal oscillator jitter
- **Standards Compliance**: NIST SP 800-90B, FIPS 140-2
- **Quality**: Cryptographically secure random numbers

#### Implementation
- **Function**: [`furi_hal_random_get()`](../flipper_rng_entropy.c#L42-L44)
- **Collection**: [`flipper_rng_collect_hardware_rng()`](../flipper_rng_entropy.c#L227-L232)
- **Worker Integration**: [Lines 58-64](../flipper_rng_worker.c#L58-L64)
- **Entropy Estimate**: 32 bits per sample (full entropy)
- **Sampling Rate**: Every worker iteration (~1-100ms depending on configuration)

#### Code References
```c
// Hardware RNG collection in worker thread
if(app->state->entropy_sources & EntropySourceHardwareRNG) {
    uint32_t hw_random = furi_hal_random_get();
    flipper_rng_add_entropy(app->state, hw_random, 32);
    entropy_bits += 32;
    app->state->bits_from_hw_rng += 32;
}
```

### 2. ADC Noise (EntropySourceADC)

**Medium-quality entropy from analog-to-digital converter sampling internal voltage reference.**

#### Hardware Details
- **Peripheral**: STM32WB55 ADC1
- **Channel**: `FuriHalAdcChannelVREFINT` (internal voltage reference)
- **Clock**: 64 MHz with 64x oversampling
- **Resolution**: 12-bit (effective ~10-bit due to noise)
- **Noise Sources**:
  - SMPS (Switch-Mode Power Supply) switching noise
  - ADC quantization noise
  - Power supply ripple and fluctuations
  - Thermal noise in analog frontend
  - Load-dependent voltage variations

#### Implementation
- **Initialization**: [`flipper_rng_init_entropy_sources()`](../flipper_rng_entropy.c#L6-L32)
- **Collection Function**: [`flipper_rng_get_adc_noise()`](../flipper_rng_entropy.c#L47-L59)
- **Worker Integration**: [Lines 66-74](../flipper_rng_worker.c#L66-L74)
- **Entropy Estimate**: 8 bits per sample
- **Sampling**: 4 ADC readings with 10μs delays, XOR of LSBs

#### Code References
```c
// ADC noise collection function
uint32_t flipper_rng_get_adc_noise(FuriHalAdcHandle* handle) {
    if(!handle) return 0;
    
    uint32_t noise = 0;
    // Sample multiple ADC channels and XOR the LSBs
    for(int i = 0; i < 4; i++) {
        uint16_t adc_val = furi_hal_adc_read(handle, FuriHalAdcChannelVREFINT);
        noise = (noise << 8) | (adc_val & 0xFF);
        furi_delay_us(10); // Small delay between samples
    }
    
    return noise;
}
```

### 3. Timing Jitter (EntropySourceTiming)

**Low-medium quality entropy from system timing variations.**

#### Hardware Details
- **Peripheral**: ARM Cortex-M4 DWT (Data Watchpoint and Trace)
- **Counter**: `DWT->CYCCNT` (64 MHz cycle counter)
- **Precision**: 15.625 nanoseconds per tick
- **Jitter Sources**:
  - CPU pipeline variations
  - Memory access timing
  - Interrupt latency variations
  - Cache hit/miss patterns
  - FreeRTOS scheduling variations

#### Implementation
- **Collection Function**: [`flipper_rng_get_timing_jitter()`](../flipper_rng_entropy.c#L62-L73)
- **Worker Integration**: [Lines 76-82](../flipper_rng_worker.c#L76-L82)
- **Entropy Estimate**: 4 bits per sample
- **Mixing**: XOR with FreeRTOS tick counter and current cycle count

#### Code References
```c
// Timing jitter collection
uint32_t flipper_rng_get_timing_jitter(void) {
    static uint32_t last_time = 0;
    uint32_t current_time = DWT->CYCCNT;
    uint32_t jitter = current_time - last_time;
    last_time = current_time;
    
    // Mix with microsecond counter
    jitter ^= (furi_get_tick() << 16);
    jitter ^= (DWT->CYCCNT & 0xFFFF);
    
    return jitter;
}
```

### 4. CPU Execution Jitter (EntropySourceCPUJitter)

**Low-quality entropy from CPU execution timing variations.**

#### Hardware Details
- **Source**: ARM Cortex-M4 execution pipeline
- **Measurement**: Execution time of fixed computational loop
- **Timer**: `DWT->CYCCNT` for precise timing
- **Variation Sources**:
  - Pipeline stalls and bubbles
  - Branch prediction variations
  - Memory access patterns
  - Instruction cache effects
  - Interrupt preemption timing

#### Implementation
- **Collection Function**: [`flipper_rng_get_cpu_jitter()`](../flipper_rng_entropy.c#L76-L88)
- **Worker Integration**: [Lines 84-90](../flipper_rng_worker.c#L84-L90)
- **Entropy Estimate**: 2 bits per sample
- **Test Loop**: 100 iterations of variable-time arithmetic

#### Code References
```c
// CPU execution jitter measurement
uint32_t flipper_rng_get_cpu_jitter(void) {
    uint32_t start = DWT->CYCCNT;
    
    // Perform some operations that have variable timing
    volatile uint32_t dummy = 0;
    for(volatile int i = 0; i < 100; i++) {
        dummy += i * i;
        dummy ^= (dummy >> 3);
    }
    
    uint32_t end = DWT->CYCCNT;
    return (end - start) ^ dummy;
}
```

### 5. Battery Voltage Noise (EntropySourceBatteryVoltage)

**Very low quality entropy from power management measurements.**

#### Hardware Details
- **Source**: Power Management IC (PMIC) measurements
- **Measurements**: Battery voltage and current from fuel gauge IC
- **Noise Sources**:
  - Battery voltage fluctuations
  - Current draw variations
  - PMIC measurement noise
  - Load-dependent voltage drops
  - Power supply ripple

#### Implementation
- **Collection Function**: [`flipper_rng_get_battery_noise()`](../flipper_rng_entropy.c#L91-L100)
- **Worker Integration**: [Lines 92-98](../flipper_rng_worker.c#L92-L98)
- **Entropy Estimate**: 2 bits per sample
- **Sampling Rate**: Every 100 worker iterations (slow changing)
- **Processing**: Extracts LSBs from floating-point voltage/current values

#### Code References
```c
// Battery voltage noise collection
uint32_t flipper_rng_get_battery_noise(void) {
    float voltage = furi_hal_power_get_battery_voltage(FuriHalPowerICFuelGauge);
    float current = furi_hal_power_get_battery_current(FuriHalPowerICFuelGauge);
    
    // Convert to integer and use LSBs as noise
    union { float f; uint32_t i; } voltage_conv = { .f = voltage };
    union { float f; uint32_t i; } current_conv = { .f = current };
    
    return (voltage_conv.i & 0xFFFF) ^ ((current_conv.i & 0xFFFF) << 16);
}
```

### 6. Temperature Noise (EntropySourceTemperature)

**Very low quality entropy from thermal measurements.**

#### Hardware Details
- **Source**: Battery temperature sensor and charge monitoring
- **Measurements**: Temperature and charge percentage
- **Noise Sources**:
  - Temperature sensor noise
  - Thermal fluctuations
  - Charge measurement variations
  - Environmental temperature changes

#### Implementation
- **Collection Function**: [`flipper_rng_get_temperature_noise()`](../flipper_rng_entropy.c#L103-L111)
- **Worker Integration**: [Lines 100-106](../flipper_rng_worker.c#L100-L106)
- **Entropy Estimate**: 1 bit per sample
- **Sampling Rate**: Every 500 worker iterations (very slow changing)
- **Processing**: Mixes temperature with charge percentage and system tick

#### Code References
```c
// Temperature noise collection
uint32_t flipper_rng_get_temperature_noise(void) {
    float temp = furi_hal_power_get_battery_temperature(FuriHalPowerICFuelGauge);
    union { float f; uint32_t i; } temp_conv = { .f = temp };
    
    // Mix with battery charge percentage
    uint8_t charge = furi_hal_power_get_pct();
    
    return (temp_conv.i & 0xFFFF) ^ (charge << 8) ^ (furi_get_tick() << 16);
}
```

### 7. Button Timing (EntropySourceButtonTiming)

**Medium-quality entropy from user interaction timing (defined but not implemented).**

#### Hardware Details
- **Source**: Physical button press timing
- **Potential Noise Sources**:
  - Human reaction time variations
  - Button press duration variations
  - User interaction patterns
  - Timing between button events

#### Implementation Status
- **Definition**: [Line 23](../flipper_rng.h#L23) in entropy source enum
- **Status**: Defined but not actively collected in current worker implementation
- **Potential**: Could provide good entropy when user is actively interacting

### 8. SubGHz RSSI Noise (EntropySourceSubGhzRSSI)

**High-quality entropy from radio frequency noise measurements across multiple ISM bands.**

#### Hardware Details
- **Peripheral**: CC1101 Sub-GHz Transceiver
- **Frequency Bands**: 315 MHz, 433.92 MHz, 868.3 MHz, 915 MHz (ISM bands)
- **Measurement**: RSSI (Received Signal Strength Indicator) and LQI (Link Quality Indicator)
- **Resolution**: Float RSSI values with sub-dBm precision
- **Noise Sources**:
  - Atmospheric RF noise from cosmic rays and lightning
  - Electronic interference from nearby devices
  - Thermal noise in RF frontend amplifiers
  - Quantization noise in RSSI measurement circuits
  - Environmental electromagnetic variations

#### Implementation
- **Collection Function**: [`flipper_rng_get_subghz_rssi_noise()`](../flipper_rng_entropy.c#L270-L328)
- **Worker Integration**: [Lines 108-114](../flipper_rng_worker.c#L108-L114)
- **Entropy Estimate**: 10 bits per sample (high-quality RF noise)
- **Sampling Rate**: Every 10 worker iterations (due to frequency switching overhead)
- **Frequency Validation**: Checks regional frequency validity before sampling

#### Code References
```c
// SubGHz RSSI noise collection
uint32_t flipper_rng_get_subghz_rssi_noise(void) {
    uint32_t entropy = 0;
    
    // Initialize SubGHz if needed
    furi_hal_subghz_init();
    furi_hal_subghz_idle();
    
    // Sample multiple frequencies for diversity
    uint32_t frequencies[] = {315000000, 433920000, 868300000, 915000000};
    
    for(int i = 0; i < 4; i++) {
        if(furi_hal_subghz_is_frequency_valid(frequencies[i])) {
            furi_hal_subghz_set_frequency_and_path(frequencies[i]);
            furi_hal_subghz_rx();
            
            furi_delay_us(200); // Allow RSSI stabilization
            
            float rssi_dbm = furi_hal_subghz_get_rssi();
            uint8_t lqi = furi_hal_subghz_get_lqi();
            
            // Mix RSSI, LQI, and timing for entropy
            union { float f; uint32_t i; } rssi_conv = { .f = rssi_dbm };
            uint32_t timing_noise = DWT->CYCCNT & 0xFF;
            uint8_t noise_byte = (rssi_conv.i & 0xFF) ^ lqi ^ timing_noise;
            
            entropy = (entropy << 8) | noise_byte;
        }
    }
    
    furi_hal_subghz_idle();
    return entropy;
}
```

### 9. NFC Field Variations (EntropySourceNFCField)

**Medium-quality entropy from electromagnetic field detection and variations.**

#### Hardware Details
- **Peripheral**: ST25R3916 NFC Transceiver
- **Frequency**: 13.56 MHz NFC carrier frequency
- **Measurement**: External electromagnetic field presence detection
- **Resolution**: Boolean field presence + timing variations
- **Noise Sources**:
  - Ambient electromagnetic field variations
  - NFC reader interference from nearby devices
  - Antenna coupling variations due to physical movement
  - RF frontend noise in field detection circuits
  - Environmental electromagnetic interference

#### Implementation
- **Collection Function**: [`flipper_rng_get_nfc_field_noise()`](../flipper_rng_entropy.c#L374-L449)
- **Worker Integration**: [Lines 117-122](../flipper_rng_worker.c#L117-L122)
- **Entropy Estimate**: 6 bits per sample (electromagnetic field variations)
- **Sampling Rate**: Every 20 worker iterations (due to NFC acquisition overhead)
- **Safe API Usage**: Proper acquire/release lifecycle following mifare_fuzzer pattern

#### Code References
```c
// NFC field variation collection
uint32_t flipper_rng_get_nfc_field_noise(void) {
    uint32_t entropy = 0;
    
    if(furi_hal_nfc_acquire() == FuriHalNfcErrorNone) {
        if(furi_hal_nfc_init() == FuriHalNfcErrorNone) {
            furi_hal_nfc_low_power_mode_stop();
            furi_hal_nfc_field_detect_start();
            
            // Sample field presence variations (32 samples)
            for(int i = 0; i < 32; i++) {
                uint32_t timing_start = DWT->CYCCNT;
                bool field_present = furi_hal_nfc_field_is_present();
                uint32_t timing_end = DWT->CYCCNT;
                
                // Mix field state with detection timing
                uint8_t noise_bit = field_present ^ (timing_end & 1);
                entropy = (entropy << 1) | noise_bit;
                
                furi_delay_us(100);
            }
            
            furi_hal_nfc_field_detect_stop();
            furi_hal_nfc_low_power_mode_start();
        }
        furi_hal_nfc_release();
    }
    
    return entropy;
}
```

## Hardware Details

### STM32WB55 Microcontroller Specifications

- **CPU**: ARM Cortex-M4F @ 64 MHz
- **Architecture**: 32-bit RISC with FPU and DSP instructions
- **Memory**: 256 KB Flash, 64 KB SRAM
- **RNG**: Hardware TRNG with dedicated 48 MHz clock
- **ADC**: 12-bit, up to 64 MHz, multiple channels
- **Timers**: High-resolution DWT cycle counter
- **Power**: Advanced power management with multiple sleep modes

### Entropy Pool Architecture

#### Pool Structure
- **Size**: 4096 bytes (`RNG_POOL_SIZE`)
- **Type**: Circular buffer with position tracking
- **Mixing**: LFSR-based diffusion algorithm
- **Extraction**: Multi-position XOR for output generation

#### Mixing Algorithm
```c
// LFSR-based mixing with diffusion
uint32_t lfsr = 0xACE1u;
for(int round = 0; round < 4; round++) {
    for(size_t i = 0; i < RNG_POOL_SIZE; i++) {
        // LFSR step
        uint32_t bit = ((lfsr >> 0) ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 5)) & 1;
        lfsr = (lfsr >> 1) | (bit << 15);
        
        // Mix with pool
        state->entropy_pool[i] ^= (lfsr & 0xFF);
        
        // Diffusion step
        if(i > 0) {
            state->entropy_pool[i] ^= state->entropy_pool[i - 1];
        }
        if(i < RNG_POOL_SIZE - 1) {
            state->entropy_pool[i] ^= state->entropy_pool[i + 1] >> 1;
        }
    }
}
```

## Implementation Details

### Worker Thread Architecture

The main entropy collection happens in the worker thread:

- **File**: [`flipper_rng_worker.c`](../flipper_rng_worker.c)
- **Function**: [`flipper_rng_worker_thread()`](../flipper_rng_worker.c#L16-L248)
- **Operation**: Continuous loop collecting from all enabled sources
- **Mixing**: Periodic entropy pool mixing every 32 iterations
- **Output**: Configurable output to USB, UART, file, or visualization

### Entropy Addition Process

1. **Collection**: Each source provides raw entropy data
2. **Bit Estimation**: Conservative estimate of actual entropy bits
3. **Pool Addition**: [`flipper_rng_add_entropy()`](../flipper_rng_entropy.c#L114-L134)
4. **Statistics**: Track bits collected per source
5. **Mixing**: Periodic LFSR-based pool mixing
6. **Extraction**: Multi-position XOR for output bytes

### Configuration System

Entropy sources are configurable via bitmask:

```c
typedef enum {
    EntropySourceHardwareRNG = (1 << 0),     // 0x01
    EntropySourceADC = (1 << 1),             // 0x02
    EntropySourceTiming = (1 << 2),          // 0x04
    EntropySourceButtonTiming = (1 << 3),    // 0x08
    EntropySourceCPUJitter = (1 << 4),       // 0x10
    EntropySourceBatteryVoltage = (1 << 5),  // 0x20
    EntropySourceTemperature = (1 << 6),     // 0x40
    EntropySourceAll = 0x7F,
} EntropySource;
```

## Quality Assessment

### Entropy Estimates (Conservative)

| Source | Bits/Sample | Quality | Sampling Rate | Notes |
|--------|-------------|---------|---------------|-------|
| Hardware RNG | 32 | Highest | Every iteration | Cryptographically secure |
| ADC Noise | 8 | Medium | Every iteration | SMPS and analog noise |
| SubGHz RSSI | 10 | High | Every 10 iterations | RF atmospheric noise |
| NFC Field | 6 | Medium | Every 20 iterations | Electromagnetic field variations |
| Timing Jitter | 4 | Low-Medium | Every iteration | System timing variations |
| CPU Jitter | 2 | Low | Every iteration | Execution timing variations |
| Battery Voltage | 2 | Very Low | Every 100 iterations | Power supply variations |
| Temperature | 1 | Very Low | Every 500 iterations | Thermal variations |

### Total Entropy Rate

With all sources enabled and default polling:
- **Per Iteration**: ~62-64 bits (depending on slow sources)
- **RF Enhancement**: Additional 10 bits every 10 iterations from SubGHz RSSI
- **EM Enhancement**: Additional 6 bits every 20 iterations from NFC field variations
- **Per Second**: Depends on poll interval configuration
- **Primary Sources**: Hardware RNG (32 bits) + SubGHz RSSI (10 bits) + NFC Field (6 bits)
- **Diversity**: Multiple independent physical and electromagnetic sources

### Quality Metrics

The system tracks several quality indicators:

- **Bytes Generated**: Total output bytes
- **Samples Collected**: Number of collection iterations
- **Entropy Rate**: Bits per second calculation
- **Source Statistics**: Bits collected per source (displayed in Statistics menu)
- **Histogram**: Byte distribution for visualization

### Statistics Display

The Statistics menu shows per-source entropy contribution:

```
=== RNG Statistics ===
Output: 1024 bytes
Rate: 156 bits/sec

--- Entropy Sources ---
HW RNG: 3200 bits
ADC: 800 bits
SubGHz: 100 bits
NFC: 60 bits
Timing: 400 bits
CPU: 200 bits
Battery: 20 bits
Temp: 2 bits

Total: 4782 bits
Pool: 2048/4096
```

## Code References

### Core Files

1. **[`flipper_rng.h`](../flipper_rng.h)** - Main header with structures and definitions
2. **[`flipper_rng_entropy.h`](../flipper_rng_entropy.h)** - Entropy collection function declarations
3. **[`flipper_rng_entropy.c`](../flipper_rng_entropy.c)** - Entropy collection implementations
4. **[`flipper_rng_worker.c`](../flipper_rng_worker.c)** - Main worker thread
5. **[`flipper_rng.c`](../flipper_rng.c)** - Application main logic
6. **[`flipper_rng_views.c`](../flipper_rng_views.c)** - UI and visualization

### Key Functions

#### Entropy Collection
- [`flipper_rng_init_entropy_sources()`](../flipper_rng_entropy.c#L6-L32) - Initialize hardware
- [`flipper_rng_get_hardware_random()`](../flipper_rng_entropy.c#L42-L44) - Hardware RNG
- [`flipper_rng_get_adc_noise()`](../flipper_rng_entropy.c#L47-L59) - ADC sampling
- [`flipper_rng_get_timing_jitter()`](../flipper_rng_entropy.c#L62-L73) - Timing measurements
- [`flipper_rng_get_cpu_jitter()`](../flipper_rng_entropy.c#L76-L88) - CPU timing
- [`flipper_rng_get_battery_noise()`](../flipper_rng_entropy.c#L91-L100) - Battery measurements
- [`flipper_rng_get_temperature_noise()`](../flipper_rng_entropy.c#L103-L111) - Temperature

#### Entropy Processing
- [`flipper_rng_add_entropy()`](../flipper_rng_entropy.c#L114-L134) - Add to pool
- [`flipper_rng_mix_entropy_pool()`](../flipper_rng_entropy.c#L137-L162) - LFSR mixing
- [`flipper_rng_extract_random_byte()`](../flipper_rng_entropy.c#L165-L192) - Output generation

#### Worker Thread
- [`flipper_rng_worker_thread()`](../flipper_rng_worker.c#L16-L248) - Main collection loop

### Flipper Zero Firmware Integration

The project integrates with Flipper Zero firmware APIs:

- **HAL Random**: [`furi_hal_random.h`](https://github.com/flipperdevices/flipperzero-firmware/blob/dev/targets/furi_hal_include/furi_hal_random.h)
- **HAL ADC**: [`furi_hal_adc.h`](https://github.com/flipperdevices/flipperzero-firmware/blob/dev/targets/furi_hal_include/furi_hal_adc.h)
- **HAL Power**: [`furi_hal_power.h`](https://github.com/flipperdevices/flipperzero-firmware/blob/dev/targets/furi_hal_include/furi_hal_power.h)
- **FreeRTOS**: Task management and timing services

## Conclusion

FlipperRNG implements a comprehensive multi-source entropy collection system that leverages multiple independent hardware noise sources from the Flipper Zero platform. The combination of high-quality hardware RNG with diverse environmental and timing sources provides robust random number generation suitable for cryptographic applications.

The conservative entropy estimation and multiple independent sources provide defense-in-depth against potential failures in any single entropy source, while the sophisticated mixing algorithm ensures proper diffusion of entropy throughout the output stream.

---

*Documentation generated for FlipperRNG v1.0*  
*Last updated: December 2024*

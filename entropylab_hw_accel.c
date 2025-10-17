/**
 * Hardware Acceleration Module for FlipperRNG
 * Leverages STM32WB55 hardware features for maximum performance
 */

#include "entropylab_hw_accel.h"
#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_serial.h>
#include <furi_hal_bus.h>
#include <furi_hal_cortex.h>
#include <stm32wbxx_ll_dma.h>
#include <stm32wbxx_ll_usart.h>

#define TAG "EntropyLab_HW"
#define CRYPTO_TIMEOUT_US 10000  // 10ms timeout for AES operations


// Forward declarations
static void flipper_rng_hw_aes_init(void);
static bool flipper_rng_hw_aes_wait_flag(uint32_t flag);

// Hardware AES context for fast mixing
static bool hw_aes_initialized = false;
static bool hw_aes_peripheral_ready = false;
static FuriMutex* hw_aes_mutex = NULL;

// Initialize hardware acceleration
void flipper_rng_hw_accel_init(void) {
    // Initialize AES mutex
    if(!hw_aes_mutex) {
        hw_aes_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    }
    
    // Initialize AES peripheral for mixing
    flipper_rng_hw_aes_init();
    
    FURI_LOG_I(TAG, "Hardware acceleration initialized");
}

// Initialize AES peripheral once for fast mixing
static void flipper_rng_hw_aes_init(void) {
    if(hw_aes_peripheral_ready) return;
    
    if(!hw_aes_mutex) return;
    
    if(furi_mutex_acquire(hw_aes_mutex, 100) != FuriStatusOk) {
        FURI_LOG_W(TAG, "Could not acquire AES mutex for initialization");
        return;
    }
    
    // Enable AES1 peripheral
    furi_hal_bus_enable(FuriHalBusAES1);
    
    // Configure AES for ECB mode (simple, fast mixing)
    CLEAR_BIT(AES1->CR, AES_CR_EN);
    MODIFY_REG(
        AES1->CR,
        AES_CR_DATATYPE | AES_CR_KEYSIZE | AES_CR_CHMOD,
        AES_CR_DATATYPE_1 | (0x2 << AES_CR_KEYSIZE_Pos) | (0x0 << AES_CR_CHMOD_Pos)  // 256-bit key, ECB mode
    );
    
    hw_aes_peripheral_ready = true;
    FURI_LOG_I(TAG, "AES peripheral initialized and ready for mixing");
    
    furi_mutex_release(hw_aes_mutex);
}

// Deinitialize hardware acceleration
void flipper_rng_hw_accel_deinit(void) {
    if(hw_aes_mutex) {
        furi_mutex_free(hw_aes_mutex);
        hw_aes_mutex = NULL;
    }
    
    // Disable AES peripheral if it was initialized
    if(hw_aes_peripheral_ready) {
        CLEAR_BIT(AES1->CR, AES_CR_EN);
        furi_hal_bus_disable(FuriHalBusAES1);
        hw_aes_peripheral_ready = false;
    }
    
    hw_aes_initialized = false;
}

// Optimized AES flag waiting based on official firmware
static bool flipper_rng_hw_aes_wait_flag(uint32_t flag) {
    FuriHalCortexTimer timer = furi_hal_cortex_timer_get(CRYPTO_TIMEOUT_US);
    while(!READ_BIT(AES1->SR, flag)) {
        if(furi_hal_cortex_timer_is_expired(timer)) {
            return false;
        }
    }
    return true;
}

// Ultra-fast hardware AES mixing for entropy pool
bool flipper_rng_hw_aes_mix_pool(uint8_t* pool, size_t pool_size, uint32_t* key) {
    if(!hw_aes_mutex || !hw_aes_peripheral_ready) return false;
    
    // Acquire mutex for AES hardware with short timeout
    if(furi_mutex_acquire(hw_aes_mutex, 10) != FuriStatusOk) {
        FURI_LOG_D(TAG, "AES mutex busy, using software mixing");
        return false;
    }
    
    // AES peripheral is already initialized, just load new key and process
    CLEAR_BIT(AES1->CR, AES_CR_EN);
    
    // Load 256-bit key (8 x 32-bit words) - this is the only per-mix overhead
    AES1->KEYR7 = key[0];
    AES1->KEYR6 = key[1];
    AES1->KEYR5 = key[2];
    AES1->KEYR4 = key[3];
    AES1->KEYR3 = key[4] ^ furi_hal_random_get();  // Mix with fresh HW random
    AES1->KEYR2 = key[5] ^ furi_hal_random_get();
    AES1->KEYR1 = key[6] ^ furi_hal_random_get();
    AES1->KEYR0 = key[7] ^ furi_hal_random_get();
    
    // Enable AES for processing
    SET_BIT(AES1->CR, AES_CR_EN);
    
    // Process pool in 16-byte blocks using hardware AES
    for(size_t i = 0; i < pool_size; i += 16) {
        if(i + 16 > pool_size) break;  // Skip incomplete blocks
        
        // Load 128-bit block
        uint32_t* block = (uint32_t*)&pool[i];
        AES1->DINR = __builtin_bswap32(block[0]);
        AES1->DINR = __builtin_bswap32(block[1]);
        AES1->DINR = __builtin_bswap32(block[2]);
        AES1->DINR = __builtin_bswap32(block[3]);
        
        // Wait for computation complete using optimized timer (based on official firmware)
        if(!flipper_rng_hw_aes_wait_flag(AES_SR_CCF)) {
            FURI_LOG_W(TAG, "AES operation timeout at block %zu", i/16);
            CLEAR_BIT(AES1->CR, AES_CR_EN);
            furi_mutex_release(hw_aes_mutex);
            return false;
        }
        
        // Clear completion flag first (official firmware pattern)
        SET_BIT(AES1->CR, AES_CR_CCFC);
        
        // Read encrypted result and XOR back into pool
        block[0] ^= __builtin_bswap32(AES1->DOUTR);
        block[1] ^= __builtin_bswap32(AES1->DOUTR);
        block[2] ^= __builtin_bswap32(AES1->DOUTR);
        block[3] ^= __builtin_bswap32(AES1->DOUTR);
    }
    
    // Keep AES peripheral enabled for next use (just disable processing)
    CLEAR_BIT(AES1->CR, AES_CR_EN);
    
    furi_mutex_release(hw_aes_mutex);
    
    return true;
}

// Fast bit manipulation using compiler intrinsics
uint32_t flipper_rng_hw_rotate_left(uint32_t value, uint8_t shift) {
    // Use compiler intrinsic for rotation if available
    return (value << shift) | (value >> (32 - shift));
}

uint32_t flipper_rng_hw_rotate_right(uint32_t value, uint8_t shift) {
    return (value >> shift) | (value << (32 - shift));
}

// Count leading zeros (useful for entropy estimation)
uint32_t flipper_rng_hw_clz(uint32_t value) {
    return value ? __builtin_clz(value) : 32;
}

// Fast byte swap
uint32_t flipper_rng_hw_bswap32(uint32_t value) {
    return __builtin_bswap32(value);
}

// Optimized XOR mixing using 32-bit operations
void flipper_rng_hw_xor_mix(uint32_t* dest, const uint32_t* src, size_t words) {
    // Process 4 words at a time for better performance
    size_t i;
    for(i = 0; i + 3 < words; i += 4) {
        dest[i] ^= src[i];
        dest[i+1] ^= src[i+1];
        dest[i+2] ^= src[i+2];
        dest[i+3] ^= src[i+3];
    }
    
    // Handle remaining words
    for(; i < words; i++) {
        dest[i] ^= src[i];
    }
}

// Optimized UART transmission using official firmware patterns
bool flipper_rng_hw_uart_tx_dma(FuriHalSerialHandle* handle, const uint8_t* data, size_t size) {
    if(!handle || !data || size == 0) {
        return false;
    }
    
    furi_hal_serial_tx(handle, data, size);
    
    return true;
}

// Optimized bulk UART transmission with minimal overhead
void flipper_rng_hw_uart_tx_bulk(FuriHalSerialHandle* handle, const uint8_t* data, size_t size) {
    if(!handle || !data || size == 0) return;
    
    // For bulk transfers, use larger chunks and less yielding
    const size_t bulk_chunk = 1024;
    size_t offset = 0;
    
    while(offset < size) {
        size_t to_send = (size - offset > bulk_chunk) ? bulk_chunk : (size - offset);
        furi_hal_serial_tx(handle, data + offset, to_send);
        offset += to_send;
    }
}

// High-resolution timing using DWT cycle counter
uint32_t flipper_rng_hw_get_cycles(void) {
    return DWT->CYCCNT;
}

// Calculate elapsed cycles
uint32_t flipper_rng_hw_cycles_elapsed(uint32_t start) {
    return DWT->CYCCNT - start;
}

// Convert cycles to microseconds (assuming 64MHz CPU)
uint32_t flipper_rng_hw_cycles_to_us(uint32_t cycles) {
    return cycles / 64;  // 64 cycles per microsecond at 64MHz
}

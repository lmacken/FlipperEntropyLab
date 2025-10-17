#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <furi_hal_serial.h>

// Hardware acceleration initialization
void flipper_rng_hw_accel_init(void);
void flipper_rng_hw_accel_deinit(void);

// Hardware AES mixing (ultra-fast)
bool flipper_rng_hw_aes_mix_pool(uint8_t* pool, size_t pool_size, uint32_t* key);

// Fast bit manipulation using compiler intrinsics
uint32_t flipper_rng_hw_rotate_left(uint32_t value, uint8_t shift);
uint32_t flipper_rng_hw_rotate_right(uint32_t value, uint8_t shift);
uint32_t flipper_rng_hw_clz(uint32_t value);
uint32_t flipper_rng_hw_bswap32(uint32_t value);

// Optimized XOR operations
void flipper_rng_hw_xor_mix(uint32_t* dest, const uint32_t* src, size_t words);

// DMA-based UART transmission
bool flipper_rng_hw_uart_tx_dma(FuriHalSerialHandle* handle, const uint8_t* data, size_t size);
void flipper_rng_hw_uart_tx_bulk(FuriHalSerialHandle* handle, const uint8_t* data, size_t size);

// High-resolution timing
uint32_t flipper_rng_hw_get_cycles(void);
uint32_t flipper_rng_hw_cycles_elapsed(uint32_t start);
uint32_t flipper_rng_hw_cycles_to_us(uint32_t cycles);

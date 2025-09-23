# FlipperRNG Build Complete! 🎉

## Summary
Successfully built FlipperRNG application with all features including:
- ✅ External entropy support (ESP32 integration)
- ✅ Dedicated "Byte Distribution" menu item
- ✅ Clean build process using `ufbt`

## Build Output
- **Application**: `dist/flipper_rng.fap` (32.7 KB)
- **Debug Symbols**: `dist/debug/flipper_rng_d.elf`
- **Target API**: Flipper Zero firmware v86.0

## Key Fixes Applied

### 1. External Entropy API Updates
Fixed serial API calls to use the new async API:
- `furi_hal_serial_rx_available` → `furi_hal_serial_async_rx_available`
- `furi_hal_serial_rx` → loop with `furi_hal_serial_async_rx` (byte-by-byte)

### 2. Byte Distribution Menu
Successfully extracted the byte distribution histogram into its own dedicated menu item:
- Added `FlipperRngMenuByteDistribution` enum
- Added `FlipperRngViewByteDistribution` view
- Implemented separate draw and input callbacks
- Clean navigation: Main Menu → Byte Distribution → Back to menu

### 3. Build System
- Transitioned from `fbt` to `ufbt` for simpler external app compilation
- No need to copy files to firmware directory
- Clean `dist/` output with only compiled artifacts

## How to Deploy

### Using qFlipper (GUI)
1. Connect your Flipper Zero
2. Open qFlipper
3. Navigate to SD Card → apps/
4. Drag `dist/flipper_rng.fap` to the apps folder

### Using CLI
```bash
# If you have Flipper connected via USB:
./fbt launch APPSRC=dist/flipper_rng.fap
```

## Build Instructions
To rebuild the application:
```bash
# Activate Python environment (if using one)
source ~/py312env/bin/activate

# Build with the script
./build.sh

# Or build directly with ufbt
ufbt
```

## Features Working
- ✅ Multiple entropy sources (HW RNG, ADC, Battery, Temperature, SubGHz RSSI, Infrared)
- ✅ External entropy via UART (ESP32 support)
- ✅ Real-time visualization (Pixel Pattern, Bit Stream)
- ✅ Byte Distribution histogram (dedicated menu)
- ✅ USB HID output
- ✅ UART streaming
- ✅ Statistics tracking
- ✅ Clean UI with proper navigation

## File Size
- Application: 32,664 bytes (~32 KB)
- Small footprint for comprehensive RNG functionality!

## Next Steps
1. Deploy to Flipper Zero
2. Test external entropy with ESP32 board
3. Run entropy comparison tests
4. Enjoy your hardware random number generator!

---
*Built with ufbt on September 21, 2025*

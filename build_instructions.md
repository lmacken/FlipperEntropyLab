# Build Instructions for FlipperRNG

## Prerequisites
- Flipper Zero firmware source code
- Flipper Build Tool (fbt)
- ARM toolchain (downloaded automatically by fbt)

## Building the Application

### Method 1: Direct Build (Recommended)
1. Copy the FlipperRNG folder to `flipperzero-firmware/applications_user/`
2. Navigate to the firmware directory:
   ```bash
   cd ~/code/flipper/flipperzero-firmware
   ```
3. Build the application:
   ```bash
   ./fbt fap_flipper_rng
   ```
4. The compiled `.fap` file will be in `build/f7-firmware-D/.extapps/`

### Method 2: Standalone Build
1. Ensure you have the Flipper Zero SDK installed
2. In the FlipperRNG directory:
   ```bash
   ufbt build
   ```

## Installation on Flipper Zero

### Via qFlipper
1. Connect your Flipper Zero via USB
2. Open qFlipper
3. Navigate to File Manager
4. Upload the `.fap` file to `/ext/apps/Tools/`

### Via SD Card
1. Copy the `.fap` file to your SD card
2. Place in `/apps/Tools/` directory
3. Insert SD card into Flipper Zero

### Via Flipper Mobile App
1. Connect to Flipper via Bluetooth
2. Use the file manager to upload the `.fap` file

## Testing the Application

### Basic Test
1. Launch FlipperRNG from Tools menu
2. Select "Start Generator"
3. Go to "Visualization" to see random data being generated
4. Check "Statistics" to verify entropy collection

### USB Output Test
1. Configure output mode to "USB CDC"
2. Start the generator
3. On your computer:
   ```bash
   # Linux/Mac
   cat /dev/ttyACM0 | xxd | head
   
   # Windows (PowerShell)
   Get-Content \\.\COM3 -Encoding Byte | Format-Hex
   ```

### Quality Test
1. Collect random data:
   ```bash
   dd if=/dev/ttyACM0 of=random_test.bin bs=1024 count=100
   ```
2. Test with entropy analysis tools:
   ```bash
   ent random_test.bin
   # or
   dieharder -a -f random_test.bin
   ```

## Troubleshooting

### Build Errors
- Ensure all `.c` files are included in compilation
- Check that all header files are present
- Verify the `application.fam` syntax

### Runtime Issues
- Check that all required services are available (GUI, Storage, etc.)
- Ensure sufficient memory is available
- Verify entropy sources are accessible

### No Output
- Check the selected output mode
- For USB: Ensure CDC driver is installed
- For UART: Verify GPIO connections
- For File: Check SD card space

## Development Tips

### Debug Build
```bash
./fbt fap_flipper_rng DEBUG=1
```

### View Logs
```bash
./fbt cli
> log
```

### Memory Profiling
Monitor memory usage in the Flipper Zero debug menu

## Performance Tuning

### Optimize for Speed
- Use "HW RNG Only" entropy source
- Set poll interval to 1ms
- Use USB output mode

### Optimize for Quality
- Use "All Sources" entropy mode
- Set poll interval to 10-50ms
- Enable all mixing algorithms

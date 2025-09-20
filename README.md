# FlipperRNG - High-Quality Random Entropy Generator

FlipperRNG is a sophisticated random number generator for the Flipper Zero that combines multiple hardware entropy sources to produce high-quality random data suitable for cryptographic applications.

## Features

### Multiple Entropy Sources
- **Hardware RNG**: STM32's built-in true random number generator
- **ADC Noise**: Analog-to-digital converter least significant bit noise
- **Timing Jitter**: High-resolution timer variations
- **Button Timing**: User input timing entropy (when available)
- **CPU Jitter**: Execution time variations
- **Battery Voltage**: Power system noise
- **Temperature Sensor**: Thermal noise variations

### Output Methods
- **USB CDC**: Stream random data over USB virtual serial port
- **UART GPIO**: Output via hardware UART pins
- **Visualization**: Real-time entropy visualization on screen
- **File Storage**: Save random data to SD card

### Advanced Features
- Configurable entropy source selection
- Adjustable polling intervals (1ms - 500ms)
- Real-time entropy quality estimation
- Von Neumann debiasing algorithm
- LFSR-based entropy pool mixing
- Statistics tracking and display

## Installation

1. Copy the `FlipperRNG` folder to your Flipper Zero's `applications_user` directory
2. Build using the Flipper Build Tool:
   ```bash
   ./fbt fap_FlipperRNG
   ```
3. Install the `.fap` file on your Flipper Zero

## Usage

### Basic Operation
1. Launch FlipperRNG from the Tools menu
2. Select "Start Generator" to begin entropy collection
3. Choose your preferred output method in Configuration
4. Random data will be streamed to the selected output

### Configuration Options

#### Entropy Sources
- **All Sources**: Uses all available entropy sources (recommended)
- **HW RNG Only**: Hardware RNG only (fastest, good quality)
- **ADC + HW RNG**: Combines hardware RNG with ADC noise
- **Timing Based**: Uses only timing-based sources
- **Custom Mix**: Predefined mix of selected sources

#### Output Modes
- **USB CDC**: Connect to computer and read from virtual serial port
- **UART GPIO**: Hardware serial output on GPIO pins
- **Visualization**: Visual representation of randomness
- **File**: Save to `/ext/flipper_rng_output.bin`

#### Poll Interval
Controls how frequently entropy is collected (1ms to 500ms)

### Visualization Mode

The visualization screen displays:
- Real-time entropy quality meter
- Total bytes generated
- Random pixel patterns
- Random walk visualization

## Technical Details

### Entropy Collection Process
1. Multiple entropy sources are sampled at different rates
2. Raw entropy is added to a 4KB circular buffer
3. LFSR-based mixing algorithm ensures uniform distribution
4. Von Neumann extraction removes bias
5. Shannon entropy calculation estimates quality

### Quality Assurance
- Continuous entropy quality monitoring
- Multiple independent entropy sources
- Cryptographic mixing functions
- Statistical bias removal

### Performance
- Generation rate: ~10-100 KB/s depending on settings
- Entropy pool size: 4096 bytes
- Output buffer: 256 bytes

## Use Cases

### Linux/Unix Entropy Pool
```bash
# Read from Flipper's USB serial and add to system entropy
cat /dev/ttyACM0 | rngd -f -r /dev/stdin
```

### Random Data Collection
```bash
# Collect 1MB of random data
dd if=/dev/ttyACM0 of=random.bin bs=1024 count=1024
```

### Testing Randomness
```bash
# Test with ent (entropy testing tool)
cat /dev/ttyACM0 | head -c 1000000 | ent
```

## GPIO Pinout (UART Mode)

When using UART output mode:
- TX: GPIO Pin 13 (USART TX)
- RX: GPIO Pin 14 (USART RX) - Not used for output
- GND: GPIO Pin 18
- Baud rate: 115200 bps

## Development

### Building from Source
```bash
cd flipperzero-firmware
./fbt fap_FlipperRNG COMPACT=1 DEBUG=0
```

### Adding New Entropy Sources

To add a new entropy source:
1. Add flag to `EntropySource` enum in `flipper_rng.h`
2. Implement collection function in `flipper_rng_entropy.c`
3. Add to worker thread collection cycle
4. Update configuration menu if needed

## Security Considerations

- FlipperRNG is designed for high-quality randomness but has not been formally audited
- For critical cryptographic applications, consider additional mixing with other sources
- The hardware RNG alone provides good quality for most uses
- Environmental factors can affect some entropy sources

## License

MIT License - See LICENSE file for details

## Contributing

Contributions are welcome! Please submit pull requests with:
- New entropy sources
- Output format options
- Performance improvements
- Bug fixes

## Acknowledgments

- Flipper Zero team for the excellent development platform
- STM32 hardware RNG documentation
- Von Neumann and LFSR algorithm implementations

## Version History

- v1.0 - Initial release with 7 entropy sources and 4 output modes
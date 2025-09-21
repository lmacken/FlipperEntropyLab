# FlipperRNG Documentation

This directory contains comprehensive documentation for the FlipperRNG multi-source entropy collection system.

## Documents

### [Entropy Sources Documentation](entropy-sources.md)
Detailed technical documentation of all entropy sources used in FlipperRNG, including:
- Hardware specifications and implementation details
- Code references with line numbers
- Quality assessment and entropy estimates
- Architecture overview and data flow
- Complete API documentation

### [Technical Reference](technical-reference.md)
Developer-focused technical reference covering:
- Build system integration
- Data structures and API reference
- Testing framework and performance characteristics
- Security considerations and best practices
- Troubleshooting guide and debugging information

## Quick Reference

### Entropy Sources Summary

| Source | Hardware | Quality | Bits/Sample | Code Reference |
|--------|----------|---------|-------------|----------------|
| **Hardware RNG** | STM32WB55 TRNG | Highest | 32 | [`flipper_rng_entropy.c:42`](../flipper_rng_entropy.c#L42) |
| **ADC Noise** | 12-bit ADC + SMPS | Medium | 8 | [`flipper_rng_entropy.c:47`](../flipper_rng_entropy.c#L47) |
| **Timing Jitter** | DWT Cycle Counter | Low-Med | 4 | [`flipper_rng_entropy.c:62`](../flipper_rng_entropy.c#L62) |
| **CPU Jitter** | Execution Timing | Low | 2 | [`flipper_rng_entropy.c:76`](../flipper_rng_entropy.c#L76) |
| **Battery Noise** | Power Management IC | Very Low | 2 | [`flipper_rng_entropy.c:91`](../flipper_rng_entropy.c#L91) |
| **Temperature** | Thermal Sensor | Very Low | 1 | [`flipper_rng_entropy.c:103`](../flipper_rng_entropy.c#L103) |

### Key Files

- [`flipper_rng.h`](../flipper_rng.h) - Main header with structures and definitions
- [`flipper_rng_entropy.c`](../flipper_rng_entropy.c) - Entropy collection implementations
- [`flipper_rng_worker.c`](../flipper_rng_worker.c) - Main worker thread
- [`flipper_rng_views.c`](../flipper_rng_views.c) - UI and visualization

### Architecture Overview

```
┌─────────────────┐    ┌──────────────────┐    ┌─────────────────┐
│   Hardware      │    │   Entropy Pool   │    │     Output      │
│   Sources       │───▶│   Processing     │───▶│   Generation    │
└─────────────────┘    └──────────────────┘    └─────────────────┘
│                      │                      │
├─ Hardware RNG       ├─ 4096-byte pool      ├─ USB CDC
├─ ADC Noise          ├─ LFSR mixing         ├─ UART
├─ Timing Jitter      ├─ Multi-position      ├─ File output
├─ CPU Jitter         │   extraction         └─ Visualization
├─ Battery Voltage    ├─ Statistics
└─ Temperature        └─ Quality metrics
```

## Getting Started

1. **Read the [Entropy Sources Documentation](entropy-sources.md)** for a comprehensive understanding of the system
2. **Review the [Technical Reference](technical-reference.md)** for implementation details
3. **Check the main [README](../README.md)** for build and usage instructions
4. **Examine the source code** using the provided line number references

## Contributing

When contributing to FlipperRNG, please:

1. Update documentation when adding new entropy sources
2. Include code references with line numbers
3. Update entropy estimates and quality assessments
4. Add appropriate testing for new features
5. Follow the existing code style and structure

## External References

### STM32WB55 Documentation
- [STM32WB55 Reference Manual](https://www.st.com/resource/en/reference_manual/rm0434-multiprotocol-wireless-32bit-mcu-armbased-cortexm4-with-fpu-bluetooth-lowenergy-and-802154-radio-solution-stmicroelectronics.pdf)
- [STM32WB55 Datasheet](https://www.st.com/resource/en/datasheet/stm32wb55cc.pdf)

### Flipper Zero Firmware
- [Flipper Zero Firmware Repository](https://github.com/flipperdevices/flipperzero-firmware)
- [Flipper Zero HAL Documentation](https://github.com/flipperdevices/flipperzero-firmware/tree/dev/targets/furi_hal_include)

### Cryptographic Standards
- [NIST SP 800-90B: Entropy Sources](https://csrc.nist.gov/publications/detail/sp/800-90b/final)
- [FIPS 140-2: Security Requirements](https://csrc.nist.gov/publications/detail/fips/140/2/final)

### Testing Tools
- [NIST Statistical Test Suite](https://csrc.nist.gov/projects/random-bit-generation/documentation-and-software)
- [Dieharder Random Number Test Suite](https://webhome.phy.duke.edu/~rgb/General/dieharder.php)
- [ENT - A Pseudorandom Number Sequence Test Program](https://www.fourmilab.ch/random/)

---

*Documentation Index for FlipperRNG v1.0*  
*Last updated: December 2024*

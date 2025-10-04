# Entropy Lab v1.0 - Release Notes

## 🎉 First Official Release!

Professional entropy generation and analysis laboratory for Flipper Zero.

### ✨ Key Features

**🎲 Multi-Source Entropy Generation:**
- Hardware RNG (STM32WB55 TRNG) - 32 bits, highest quality
- SubGHz RSSI (RF atmospheric noise) - 10 bits, high quality  
- Infrared Noise (IR ambient noise) - 8 bits, high quality
- Configurable source combinations

**📝 Professional Passphrase Generation:**
- **EFF wordlist** (7776 words, ~12.925 bits/word) - Maximum security
- **BIP-39 wordlist** (2048 words, 11.0 bits/word) - Bitcoin standard
- **SLIP-39 wordlist** (1024 words, 10.0 bits/word) - Shamir sharing
- 3-12 word configurable length
- Cryptographically secure word selection

**🔍 Real-Time Analysis:**
- Live entropy visualization (random walk, histogram)
- Quality testing and statistical analysis  
- Per-source bit counters and statistics
- Configurable refresh rates (100ms-1s)

**⚙️ Professional Configuration:**
- Entropy source selection (All, HW Only, combinations)
- Pool mixing (Hardware AES, Software XOR)
- Output modes (UART, File, Visualization)
- Wordlist selection (EFF, BIP-39, SLIP-39)
- Performance tuning (Poll Rate, Visual Rate)

**💡 User Experience:**
- LED status indicators (red=stopped, green=generating)
- Clean, professional UI optimized for Flipper screen
- Comprehensive documentation and help
- No external dependencies - everything included

### 🔒 Security & Quality

- **Cryptographically secure**: Uses official wordlists and proper entropy mixing
- **No modulo bias**: Rejection sampling for uniform distribution
- **Hardware acceleration**: STM32WB55 AES for entropy pool mixing
- **Professional standards**: EFF, BIP-39, SLIP-39 compliance

### 📦 Package Contents

- **Single file installation**: `entropylab.fap` (187KB)
- **All wordlists included**: No additional downloads needed
- **Complete documentation**: Installation guide, usage instructions
- **Professional quality**: Ready for production use

### 🎯 Perfect For

- Security professionals and researchers
- Cryptocurrency users (BIP-39 compatible)
- Hardware entropy testing and analysis
- Educational cryptography demonstrations
- Anyone needing high-quality random data

**Download, install, and start generating professional-grade entropy!** 🚀

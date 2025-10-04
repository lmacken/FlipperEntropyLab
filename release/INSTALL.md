# Entropy Lab v1.0 - Installation Guide

## 📦 What's Included

This package contains everything you need:
- `entropylab.fap` - Complete app with all wordlists (187KB)

## 🚀 Installation Methods

### Method 1: qFlipper (Recommended)
1. Connect your Flipper Zero via USB
2. Open qFlipper
3. Go to "Apps" tab
4. Click "Install from file"
5. Select `entropylab.fap`
6. Done! ✅

### Method 2: SD Card
1. Copy `entropylab.fap` to `/ext/apps/Tools/` on your SD card
2. Insert SD card into Flipper Zero
3. Navigate to Apps → Tools → Entropy Lab
4. Done! ✅

### Method 3: Command Line
```bash
# Using ufbt (if you have the toolchain)
ufbt launch_app ARGS=/ext/apps/Tools/entropylab.fap
```

## 🔍 What Gets Installed

When you install Entropy Lab, it automatically creates:
```
/ext/apps/Tools/entropylab.fap          # Main app (187KB)
/ext/apps/Tools/entropylab/              # Wordlist directory
├── eff_large_wordlist.txt              # EFF wordlist (7776 words)
├── bip39_english.txt                   # BIP-39 wordlist (2048 words)
├── slip39_english.txt                  # SLIP-39 wordlist (1024 words)
└── README.txt                          # Wordlist documentation
```

## ✨ Features

- **Professional entropy generation** from multiple hardware sources
- **3 cryptographic wordlist standards**: EFF, BIP-39, SLIP-39
- **Real-time visualization** and quality testing
- **Configurable output** (UART, File, Visualization)
- **LED status indicators** during entropy collection
- **No additional setup required** - everything is included!

## 🎯 Usage

1. Install the app using any method above
2. Navigate to Apps → Tools → Entropy Lab
3. Choose your entropy sources and wordlist in Configuration
4. Start generating high-quality entropy!

## 🔒 Security

All wordlists are official, cryptographically vetted standards:
- **EFF**: Electronic Frontier Foundation diceware
- **BIP-39**: Bitcoin mnemonic seed standard
- **SLIP-39**: Shamir's Secret Sharing standard

Enjoy secure, professional entropy generation! 🎲✨

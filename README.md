# ESP32 WiFi Morse Code Decoder

An Arduino sketch for the **Freenove ESP32-S3-WROOM** that creates a WiFi access point and hosts a real-time web interface to decode Morse code input from the BOOT button.

## Objective

Translate physical Morse code button presses into readable text, displayed live in a browser on any device connected to the ESP32's WiFi network.

## Hardware

| Component | Details |
|-----------|---------|
| Board | Freenove ESP32-S3-WROOM |
| Input | BOOT button — GPIO 0 (built-in) |
| Output | Onboard LED — GPIO 2 (change to 48 if NeoPixel) |

## How It Works

| Action | Morse Symbol |
|--------|-------------|
| Short press < 300ms | `.` dot — LED flickers |
| Long press ≥ 300ms | `-` dash — LED stays on until release |
| Silence 1.5s | End of letter → auto-decoded |
| Silence 3.5s | End of word → space added |

## Getting Started

1. Open `projet_test_morsecode.ino` in Arduino IDE
2. Select board: **ESP32S3 Dev Module**
3. Upload to your ESP32
4. Open Serial Monitor (115200 baud) — the IP address will be displayed
5. On your phone, connect to WiFi: **ESP32-Morse** / password: **12345678**
6. Open your browser at **http://192.168.4.1**
7. Start typing in Morse with the BOOT button

## Web Interface

- Live display of the current letter being typed
- Full decoded message
- Buttons: Clear, Backspace, Space

## Dependencies

- `WiFi.h` — built into ESP32 Arduino core
- `WebServer.h` — built into ESP32 Arduino core

No external libraries required.

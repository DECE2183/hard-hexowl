# hexowl hardware implementation

This is a hardware [hexowl](https://github.com/DECE2183/hexowl) calculator implementation for the ESP32 microcontroller. This software is designed for the `ESP32-WROVER-B` module because it requires a large amount of RAM.

## Build

### Software requirements:

 - ESP-IDF `v4.4`;
 - Go `v1.18` - `v1.19`;
 - tinygo `v0.26`.

### Build steps:

 1. Recursively clone this repo:

```bash
git clone --recursive https://github.com/DECE2183/hard-hexowl.git
```

 2. Copy the following resources from your existing `tinygo` installation to the same places in the local `tinygo` root folder `hexowl/tinygo`:

 - Folder `lib/clang`;
 - File `src/device/esp/esp32.go`.

 3. Build. Execute the following commands in the `ESP-IDF` environment:

 ```bash
 mkdir -p build && cd build
 ```

 ```bash
 cmake .. -G Ninja
 ```

 ```bash
 ninja
 ```
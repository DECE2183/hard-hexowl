# hexowl hardware implementation

This is a hardware [hexowl](https://github.com/DECE2183/hexowl) calculator implementation for the ESP32 microcontroller. This software is designed for the `ESP32-WROVER-B` module because it requires a large amount of RAM.

## Build

### Software requirements:

 - ESP-IDF `v5.1`;
 - Go `v1.18` - `v1.21`;
 - tinygo `v0.33`.

### Build steps:

 1. Clone this repo and init submodules:

```bash
git clone https://github.com/DECE2183/hard-hexowl.git
```

```bash
git submodule update --init
```

 2. Build. Execute the following commands in the `ESP-IDF` environment:

 ```bash
 mkdir -p build && cd build
 ```

 ```bash
 cmake .. -G Ninja
 ```

 ```bash
 ninja
 ```
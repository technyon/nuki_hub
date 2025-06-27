# ESP-Hosted Performance Optimization Guide

Quick reference for optimizing ESP-Hosted performance across different transport interfaces.

## Quick Start - High Performance Config

For immediate performance gains, add these to your host's `sdkconfig.defaults.esp32XX` file:

```
# Wi-Fi Performance
CONFIG_ESP_WIFI_STATIC_RX_BUFFER_NUM=16
CONFIG_ESP_WIFI_DYNAMIC_RX_BUFFER_NUM=64
CONFIG_ESP_WIFI_DYNAMIC_TX_BUFFER_NUM=64
CONFIG_ESP_WIFI_AMPDU_TX_ENABLED=y
CONFIG_ESP_WIFI_TX_BA_WIN=32
CONFIG_ESP_WIFI_AMPDU_RX_ENABLED=y
CONFIG_ESP_WIFI_RX_BA_WIN=32

# TCP/IP Performance
CONFIG_LWIP_TCP_SND_BUF_DEFAULT=65534
CONFIG_LWIP_TCP_WND_DEFAULT=65534
CONFIG_LWIP_TCP_RECVMBOX_SIZE=64
CONFIG_LWIP_UDP_RECVMBOX_SIZE=64
CONFIG_LWIP_TCPIP_RECVMBOX_SIZE=64
CONFIG_LWIP_TCP_SACK_OUT=y
```

> **Note**: Adjust values based on your MCU host's memory capacity and as per change as per build system

## Transport Optimization

### SDIO (Highest Performance)
- **Clock Speed**: Start at 20 MHz, optimize up to 50 MHz
- **Bus Width**: Use 4-bit mode
- **Hardware**: Use PCB with controlled impedance, external pull-ups (51kΩ)
- **Checksum**: Optional (SDIO hardware handles verification)

```
CONFIG_ESP_HOSTED_SDIO_CLOCK_FREQ_KHZ=40000
CONFIG_ESP_HOSTED_SDIO_BUS_WIDTH=4
```

> [!NOTE]
> See [Performance and Memory Usage](sdio.md#9-performance-and-memory-usage) on the trade-off between SDIO Performance and Memory Use

### SPI Full-Duplex
- **Clock Speed**: ESP32: ≤10 MHz, Others: ≤40 MHz
- **Hardware**: Use IO_MUX pins, short traces (≤10cm for jumpers)
- **Checksum**: Mandatory (SPI hardware lacks error detection)

```
CONFIG_ESP_HOSTED_SPI_CLK_FREQ=40
```

### SPI Half-Duplex
- **Data Lines**: Use 4-line (Quad SPI) mode
- **Similar optimizations as SPI Full-Duplex**

### UART (Lowest Performance)
- **Baud Rate**: Use 921600 (highest stable rate)
- **Best for**: Low-throughput applications, debugging

## Memory Optimization

- Reduce memory footprint for resource-constrained applications:

  ```
  # Reduce queue sizes
  CONFIG_ESP_HOSTED_SDIO_TX_Q_SIZE=10    # Default: 20
  CONFIG_ESP_HOSTED_SDIO_RX_Q_SIZE=10    # Default: 20

  # Enable memory pooling
  CONFIG_ESP_HOSTED_USE_MEMPOOL=y
  ```

- Disable the not-in-use features
  - For example, disable bluetooth if not needed
- Use external RAM, for higher memory (PSRAM is supported)
- Optimise internal RAM using [ESP-IDF iram optimization tricks](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/performance/ram-usage.html) 
## Hardware Guidelines

### Critical Requirements
1. **Signal Integrity**: Use PCB designs for production, jumpers only for prototyping
2. **Power Supply**: Stable 3.3V, proper decoupling capacitors
3. **Trace Length**: Match lengths, especially clock vs data lines
4. **Pull-ups**: Required for SDIO (51kΩ) on CMD, D0-D3 lines

### PCB Design Checklist
- [ ] Equal trace lengths for communication signals
- [ ] Ground plane for signal stability
- [ ] Controlled impedance traces (50Ω typical)
- [ ] Series termination resistors for high-speed signals
- [ ] Extra GPIOs reserved for future features (deep sleep, etc.)

## Development Workflow
1. **Proof of Concept**: Start with jumper wires, low clock speeds
2. **Incremental Optimization**: Increase transport clock step by step
3. **Hardware Validation**: Move to PCB for final validation
4. **Performance Tuning**: Optimize buffers and configurations
5. **Disable features**: Any unsued components from ESP-IDF or
ESP-Hosted-MCU features could be disabled for more memory
availability.

## docs/spi.md

```markdown
# SPI (Serial Peripheral Interface)

## Overview

The Jetson Orin Nano provides high-speed SPI interfaces capable of up to 50 MHz, making it suitable for sensors, ADCs, displays, and other high-bandwidth peripherals.

## Hardware Specifications

| Parameter | Value |
|-----------|-------|
| SPI Controllers | 2 (SPI1 on header, SPI2 internal) |
| Max Speed | 50 MHz |
| Modes | 0, 1, 2, 3 |
| Data Width | 4-32 bits |
| FIFO Size | 64 bytes TX/RX |
| Chip Selects | CS0, CS1 |
| DMA Support | Yes |
| Voltage | 3.3V |

## Pin Mapping

### SPI1 on 40-pin Header

Pin 19 (SPI1_MOSI) ─── Master Out Slave In
Pin 21 (SPI1_MISO) ─── Master In Slave Out
Pin 23 (SPI1_SCK) ─── Serial Clock
Pin 24 (SPI1_CS0) ─── Chip Select 0
Pin 11 (GPIO17) ─── SPI1_CS1 (Alternate function)
text


### SPI2 (Not on header, for reference)

SPI2_MOSI ─── Pin 37 (GPIO26, alt function)
SPI2_MISO ─── Pin 38 (GPIO20, alt function)
SPI2_SCK ─── Pin 31 (GPIO6, alt function)
SPI2_CS0 ─── Pin 32 (GPIO0, alt function)
SPI2_CS1 ─── Pin 33 (GPIO1, alt function)
text


## SPI Modes

| Mode | CPOL | CPHA | Description |
|------|------|------|-------------|
| 0 | 0 | 0 | Clock idle low, sample on rising edge |
| 1 | 0 | 1 | Clock idle low, sample on falling edge |
| 2 | 1 | 0 | Clock idle high, sample on falling edge |
| 3 | 1 | 1 | Clock idle high, sample on rising edge |

Mode 0: CLK |‾‾‾||‾‾‾||‾‾‾|
└─┘ └─┘ └─┘
MOSI: X---BIT---X (sampled on rising edge)

Mode 3: CLK ‾‾‾||‾‾‾||‾‾‾|___|‾‾‾
└─┘ └─┘ └─┘
MOSI: X---BIT---X (sampled on rising edge)
text


## Register Map

Base Address: 0x3240000

| Register | Offset | Description |
|----------|--------|-------------|
| CONTROL0 | 0x00 | Control register 0 |
| CONTROL1 | 0x04 | Control register 1 |
| STATUS | 0x08 | Status register |
| FIFO | 0x0C | FIFO data register |
| DMA | 0x10 | DMA control |
| INTERRUPT | 0x14 | Interrupt enable/status |
| CLOCK | 0x18 | Clock configuration |
| CS | 0x1C | Chip select control |

## Implementation

### High-Performance SPI Class
```cpp
class SPI {
private:
    int fd;
    std::string device;
    uint32_t speed;
    uint8_t mode;
    uint8_t bits;
    uint16_t delay;
    
    void spi_transfer(const uint8_t* tx, uint8_t* rx, size_t len) {
        struct spi_ioc_transfer tr = {
            .tx_buf = (unsigned long)tx,
            .rx_buf = (unsigned long)rx,
            .len = len,
            .speed_hz = speed,
            .delay_usecs = delay,
            .bits_per_word = bits,
            .cs_change = 0,
            .tx_nbits = 0,
            .rx_nbits = 0,
            .pad = 0,
        };
        
        ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
    }
    
public:
    SPI(const std::string& dev, uint32_t max_speed = 1000000) 
        : device(dev), speed(max_speed), mode(0), bits(8), delay(0) {
        fd = open(device.c_str(), O_RDWR);
        if(fd < 0) {
            throw std::runtime_error("Cannot open SPI device");
        }
        
        // Set SPI mode
        ioctl(fd, SPI_IOC_WR_MODE, &mode);
        ioctl(fd, SPI_IOC_RD_MODE, &mode);
        
        // Set bits per word
        ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
        ioctl(fd, SPI_IOC_RD_BITS_PER_WORD, &bits);
        
        // Set max speed
        ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
        ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &speed);
    }
    
    ~SPI() {
        if(fd >= 0) close(fd);
    }
    
    void setMode(uint8_t m) {
        mode = m;
        ioctl(fd, SPI_IOC_WR_MODE, &mode);
    }
    
    void setSpeed(uint32_t hz) {
        speed = hz;
        ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
    }
    
    void setBitsPerWord(uint8_t b) {
        bits = b;
        ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
    }
    
    uint8_t transfer(uint8_t data) {
        uint8_t rx;
        spi_transfer(&data, &rx, 1);
        return rx;
    }
    
    void transfer(const std::vector<uint8_t>& tx, std::vector<uint8_t>& rx) {
        rx.resize(tx.size());
        spi_transfer(tx.data(), rx.data(), tx.size());
    }
    
    void write(const uint8_t* data, size_t len) {
        spi_transfer(data, nullptr, len);
    }
    
    void read(uint8_t* data, size_t len) {
        std::vector<uint8_t> tx(len, 0xFF);  // Send dummy bytes
        spi_transfer(tx.data(), data, len);
    }
};

DMA-Enabled SPI
cpp

class SPIDMA : public SPI {
private:
    int dma_fd;
    void* tx_dma_buf;
    void* rx_dma_buf;
    size_t dma_buf_size;
    
    void allocateDMABuffers(size_t size) {
        dma_buf_size = size;
        // Align to cache line (64 bytes)
        posix_memalign(&tx_dma_buf, 64, size);
        posix_memalign(&rx_dma_buf, 64, size);
        
        // Flush cache
        __builtin___clear_cache((char*)tx_dma_buf, (char*)tx_dma_buf + size);
        __builtin___clear_cache((char*)rx_dma_buf, (char*)rx_dma_buf + size);
    }
    
public:
    SPIDMA(const std::string& dev, uint32_t speed) 
        : SPI(dev, speed), tx_dma_buf(nullptr), rx_dma_buf(nullptr) {
        
        // Enable DMA in SPI controller
        int dma_enable = 1;
        ioctl(fd, SPI_IOC_ENABLE_DMA, &dma_enable);
        
        allocateDMABuffers(65536);  // 64KB buffer
    }
    
    void dmaTransfer(const uint8_t* tx, uint8_t* rx, size_t len) {
        if(len > dma_buf_size) {
            allocateDMABuffers(len);
        }
        
        memcpy(tx_dma_buf, tx, len);
        
        // Configure DMA transfer
        struct spi_ioc_transfer tr = {
            .tx_buf = (unsigned long)tx_dma_buf,
            .rx_buf = (unsigned long)rx_dma_buf,
            .len = len,
            .speed_hz = speed,
            .delay_usecs = 0,
            .bits_per_word = bits,
            .cs_change = 0,
            .dma = 1,  // Use DMA
        };
        
        ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
        
        memcpy(rx, rx_dma_buf, len);
    }
    
    ~SPIDMA() {
        if(tx_dma_buf) free(tx_dma_buf);
        if(rx_dma_buf) free(rx_dma_buf);
    }
};

Device Drivers
MCP3008 ADC Driver
cpp

class MCP3008 {
    SPI spi;
    uint8_t channel;
    
public:
    MCP3008(const std::string& spi_dev, uint8_t ch) 
        : spi(spi_dev, 1000000), channel(ch) {}
    
    uint16_t readADC() {
        uint8_t tx[] = {0x01, 0x80 | (channel << 4), 0x00};
        uint8_t rx[3];
        
        spi.transfer(tx, rx, 3);
        
        // Combine 10-bit result
        return ((rx[1] & 0x03) << 8) | rx[2];
    }
    
    float readVoltage(float vref = 3.3) {
        return (readADC() * vref) / 1023.0;
    }
};

MAX31855 Thermocouple Driver
cpp

class MAX31855 {
    SPI spi;
    
public:
    MAX31855(const std::string& spi_dev) : spi(spi_dev, 5000000) {}
    
    struct Reading {
        float temperature;
        float internal_temp;
        bool fault;
        bool short_to_vcc;
        bool short_to_gnd;
        bool open_circuit;
    };
    
    Reading read() {
        uint8_t rx[4];
        spi.read(rx, 4);
        
        uint32_t data = (rx[0] << 24) | (rx[1] << 16) | (rx[2] << 8) | rx[3];
        
        Reading result;
        
        // Extract thermocouple temperature (14-bit signed)
        int16_t temp_raw = (data >> 18) & 0x3FFF;
        if(temp_raw & 0x2000) temp_raw |= 0xC000;  // Sign extend
        result.temperature = temp_raw * 0.25;
        
        // Extract internal temperature (12-bit signed)
        int16_t int_temp_raw = (data >> 4) & 0xFFF;
        if(int_temp_raw & 0x800) int_temp_raw |= 0xF000;
        result.internal_temp = int_temp_raw * 0.0625;
        
        // Check faults
        result.fault = (data & 0x10000) != 0;
        result.short_to_vcc = (data & 0x04) != 0;
        result.short_to_gnd = (data & 0x02) != 0;
        result.open_circuit = (data & 0x01) != 0;
        
        return result;
    }
};

ILI9341 TFT Display Driver
cpp

class ILI9341 {
    SPI spi;
    GPIO dc_pin;   // Data/Command pin
    GPIO cs_pin;   // Chip select
    GPIO rst_pin;  // Reset pin
    
    void writeCommand(uint8_t cmd) {
        dc_pin.write(GPIO::Value::LOW);  // Command mode
        cs_pin.write(GPIO::Value::LOW);
        spi.transfer(cmd);
        cs_pin.write(GPIO::Value::HIGH);
    }
    
    void writeData(uint8_t data) {
        dc_pin.write(GPIO::Value::HIGH);  // Data mode
        cs_pin.write(GPIO::Value::LOW);
        spi.transfer(data);
        cs_pin.write(GPIO::Value::HIGH);
    }
    
    void writeData16(uint16_t data) {
        writeData(data >> 8);
        writeData(data & 0xFF);
    }
    
public:
    ILI9341(const std::string& spi_dev, unsigned int dc_pin_num, 
            unsigned int cs_pin_num, unsigned int rst_pin_num)
        : spi(spi_dev, 40000000), 
          dc_pin(dc_pin_num, GPIO::Direction::OUTPUT),
          cs_pin(cs_pin_num, GPIO::Direction::OUTPUT),
          rst_pin(rst_pin_num, GPIO::Direction::OUTPUT) {
        
        // Hardware reset
        rst_pin.write(GPIO::Value::LOW);
        usleep(10000);
        rst_pin.write(GPIO::Value::HIGH);
        usleep(120000);
        
        // Initialize display
        writeCommand(0x01);  // Software reset
        usleep(120000);
        
        writeCommand(0x11);  // Exit sleep
        usleep(120000);
        
        writeCommand(0x3A);  // Set pixel format
        writeData(0x55);     // 16-bit color
        
        writeCommand(0x36);  // Memory access control
        writeData(0x48);     // BGR mode
        
        writeCommand(0x29);  // Display on
    }
    
    void setWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
        writeCommand(0x2A);  // Column address set
        writeData16(x0);
        writeData16(x1);
        
        writeCommand(0x2B);  // Row address set
        writeData16(y0);
        writeData16(y1);
        
        writeCommand(0x2C);  // Memory write
    }
    
    void drawPixel(uint16_t x, uint16_t y, uint16_t color) {
        setWindow(x, y, x, y);
        writeData16(color);
    }
    
    void fillRect(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color) {
        setWindow(x0, y0, x1, y1);
        
        dc_pin.write(GPIO::Value::HIGH);  // Data mode
        
        cs_pin.write(GPIO::Value::LOW);
        uint32_t pixels = (x1 - x0 + 1) * (y1 - y0 + 1);
        
        // Bulk transfer
        std::vector<uint16_t> buffer(pixels, color);
        for(uint32_t i = 0; i < pixels; i++) {
            spi.transfer(buffer[i] >> 8);
            spi.transfer(buffer[i] & 0xFF);
        }
        cs_pin.write(GPIO::Value::HIGH);
    }
};

Performance Optimization
Dual SPI (Quad-SPI)
cpp

class QuadSPI {
    int fd;
    std::vector<uint8_t> buffer;
    
public:
    void quadWrite(const uint8_t* data, size_t len) {
        struct spi_ioc_transfer tr = {
            .tx_buf = (unsigned long)data,
            .len = len,
            .tx_nbits = 4,  // 4-bit transfer
        };
        ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
    }
    
    void quadRead(uint8_t* data, size_t len) {
        struct spi_ioc_transfer tr = {
            .rx_buf = (unsigned long)data,
            .len = len,
            .rx_nbits = 4,  // 4-bit transfer
        };
        ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
    }
};

Overlapping DMA Transfers
cpp

class PipelinedSPI {
    static constexpr size_t BUFFER_SIZE = 32768;
    
    void* buffers[2][2];  // [buffer_index][tx/rx]
    int current_buffer = 0;
    
    void startDMATransfer(int buffer_idx, size_t len) {
        struct spi_ioc_transfer tr = {
            .tx_buf = (unsigned long)buffers[buffer_idx][0],
            .rx_buf = (unsigned long)buffers[buffer_idx][1],
            .len = len,
            .dma = 1,
        };
        ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
    }
    
public:
    void continuousTransfer() {
        // Prepare first buffer
        prepareBuffer(0, BUFFER_SIZE);
        startDMATransfer(0, BUFFER_SIZE);
        
        while(true) {
            // Prepare next buffer while current transfers
            int next = (current_buffer + 1) % 2;
            prepareBuffer(next, BUFFER_SIZE);
            
            // Wait for current transfer
            waitForDMAComplete(current_buffer);
            
            // Process received data
            processBuffer(current_buffer, BUFFER_SIZE);
            
            // Start next transfer
            startDMATransfer(next, BUFFER_SIZE);
            current_buffer = next;
        }
    }
};

Troubleshooting
Common Issues

    No clock output

        Check pin mux configuration

        Verify SPI device is enabled in device tree

        Test with logic analyzer

    Data corruption

        Reduce SPI speed

        Check signal integrity (ringing, overshoot)

        Add series resistors (22-33Ω) on SCK/MOSI

    CS not asserting

        Verify CS pin configuration

        Check if CS change mode is correct

Debug Tools
bash

# Check SPI devices
ls -l /dev/spidev*

# Test SPI loopback (requires hardware connection)
spidev_test -D /dev/spidev0.0 -s 1000000 -v

# Monitor with logic analyzer
sigrok-cli --driver fx2lafw --config samplerate=100m \
  --protocol-decoder spi --channels 0=CLK,1=MOSI,2=MISO,3=CS \
  --continuous -o spi_capture.sr

Best Practices

    Use DMA for large transfers (> 64 bytes)

    Keep SPI traces short (< 10cm for 50MHz)

    Add ground return paths for each signal

    Use separate power for SPI devices if drawing significant current

    Implement CS toggling between transfers to prevent bus conflicts

    Verify signal integrity with oscilloscope at high speeds

    Use termination resistors for long traces (> 30cm)
